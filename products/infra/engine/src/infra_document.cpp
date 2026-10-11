// SPDX-License-Identifier: MPL-2.0
#include "mydesk/infra/infra_document.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace mydesk::infra {

namespace {

std::string escapeJson(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

bool extractString(std::string_view json, std::string_view key, std::string& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) return false;
    const auto q1 = json.find('"', json.find(':', pos));
    if (q1 == std::string_view::npos) return false;
    const auto q2 = json.find('"', q1 + 1);
    if (q2 == std::string_view::npos) return false;
    out = std::string(json.substr(q1 + 1, q2 - q1 - 1));
    return true;
}

bool extractNumber(std::string_view json, std::string_view key, double& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) return false;
    const auto colon = json.find(':', pos);
    if (colon == std::string_view::npos) return false;
    try {
        out = std::stod(std::string(json.substr(colon + 1)));
        return std::isfinite(out);
    } catch (...) {
        return false;
    }
}

} // namespace

std::optional<double> DemGrid::elevationAt(double easting, double northing) const {
    if (cols < 2 || rows < 2 || cellSizeMeters <= 0.0) return std::nullopt;

    const double gx = (easting - originEasting) / cellSizeMeters;
    const double gy = (northing - originNorthing) / cellSizeMeters;

    if (gx < 0.0 || gy < 0.0 || gx > static_cast<double>(cols - 1) + 1e-9 || gy > static_cast<double>(rows - 1) + 1e-9) {
        return std::nullopt;
    }

    const double gxClamped = std::clamp(gx, 0.0, static_cast<double>(cols - 1));
    const double gyClamped = std::clamp(gy, 0.0, static_cast<double>(rows - 1));

    const std::size_t c0 = std::min(static_cast<std::size_t>(std::floor(gxClamped)), cols - 2);
    const std::size_t r0 = std::min(static_cast<std::size_t>(std::floor(gyClamped)), rows - 2);
    const std::size_t c1 = c0 + 1;
    const std::size_t r1 = r0 + 1;

    const double fx = gxClamped - static_cast<double>(c0);
    const double fy = gyClamped - static_cast<double>(r0);

    const double z00 = elevations[r0 * cols + c0];
    const double z10 = elevations[r0 * cols + c1];
    const double z01 = elevations[r1 * cols + c0];
    const double z11 = elevations[r1 * cols + c1];

    // Bilinear interpolation
    const double zTop = (1.0 - fx) * z00 + fx * z10;
    const double zBottom = (1.0 - fx) * z01 + fx * z11;
    return (1.0 - fy) * zTop + fy * zBottom;
}

double ComponentRoad::totalLengthMeters() const {
    double len = 0.0;
    for (std::size_t i = 1; i < centerline.size(); ++i) {
        const double dx = centerline[i].easting - centerline[i - 1].easting;
        const double dy = centerline[i].northing - centerline[i - 1].northing;
        const double dz = centerline[i].elevation - centerline[i - 1].elevation;
        len += std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    return len;
}

InfraDocument::InfraDocument() = default;

void InfraDocument::setDemGrid(double originEasting, double originNorthing, double cellSize,
                               std::size_t cols, std::size_t rows, std::vector<double> elevations) {
    dem_.originEasting = originEasting;
    dem_.originNorthing = originNorthing;
    dem_.cellSizeMeters = cellSize;
    dem_.cols = cols;
    dem_.rows = rows;
    dem_.elevations = std::move(elevations);
}

std::optional<double> InfraDocument::terrainElevationAt(double easting, double northing) const {
    return dem_.elevationAt(easting, northing);
}

ElementId InfraDocument::addRoad(std::string name, int numLanes, double laneWidth, double designSpeed,
                                 std::vector<RoadVertex> centerline) {
    ComponentRoad road;
    road.id = nextId_++;
    road.name = std::move(name);
    road.numLanes = numLanes;
    road.laneWidthMeters = laneWidth;
    road.designSpeedKmh = designSpeed;
    road.centerline = std::move(centerline);
    roads_.push_back(std::move(road));
    return roads_.back().id;
}

const ComponentRoad* InfraDocument::findRoad(ElementId id) const {
    for (const auto& r : roads_) {
        if (r.id == id) return &r;
    }
    return nullptr;
}

ElementId InfraDocument::addBridge(std::string name, ElementId roadId, double startSta, double endSta, int piers) {
    BridgeStructure bridge;
    bridge.id = nextId_++;
    bridge.name = std::move(name);
    bridge.roadId = roadId;
    bridge.startStation = startSta;
    bridge.endStation = endSta;
    bridge.pierCount = piers;
    bridges_.push_back(std::move(bridge));
    return bridges_.back().id;
}

ElementId InfraDocument::addBuilding(std::string name, std::string usage, double roofHeight,
                                     std::vector<std::pair<double, double>> footprint) {
    CityBuilding b;
    b.id = nextId_++;
    b.name = std::move(name);
    b.usageCategory = std::move(usage);
    b.roofHeightMeters = roofHeight;
    b.footprint = std::move(footprint);
    if (!b.footprint.empty()) {
        auto elev = terrainElevationAt(b.footprint[0].first, b.footprint[0].second);
        b.baseElevation = elev.value_or(0.0);
    }
    buildings_.push_back(std::move(b));
    return buildings_.back().id;
}

std::string InfraDocument::exportGeoJson() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"type\": \"FeatureCollection\",\n";
    ss << "  \"crs\": { \"type\": \"name\", \"properties\": { \"name\": \"" << escapeJson(crs_) << "\" } },\n";
    ss << "  \"features\": [\n";

    bool first = true;
    for (const auto& road : roads_) {
        if (!first) ss << ",\n";
        first = false;
        ss << "    {\n";
        ss << "      \"type\": \"Feature\",\n";
        ss << "      \"properties\": { \"name\": \"" << escapeJson(road.name) << "\", \"lanes\": " << road.numLanes
           << ", \"speed_kmh\": " << road.designSpeedKmh << " },\n";
        ss << "      \"geometry\": {\n";
        ss << "        \"type\": \"LineString\",\n";
        ss << "        \"coordinates\": [";
        for (std::size_t i = 0; i < road.centerline.size(); ++i) {
            const auto& v = road.centerline[i];
            ss << "[" << v.easting << ", " << v.northing << ", " << v.elevation << "]";
            if (i + 1 < road.centerline.size()) ss << ", ";
        }
        ss << "]\n      }\n    }";
    }

    for (const auto& b : buildings_) {
        if (!first) ss << ",\n";
        first = false;
        ss << "    {\n";
        ss << "      \"type\": \"Feature\",\n";
        ss << "      \"properties\": { \"name\": \"" << escapeJson(b.name) << "\", \"usage\": \""
           << escapeJson(b.usageCategory) << "\", \"height_m\": " << b.roofHeightMeters << " },\n";
        ss << "      \"geometry\": {\n";
        ss << "        \"type\": \"Polygon\",\n";
        ss << "        \"coordinates\": [[";
        for (std::size_t i = 0; i < b.footprint.size(); ++i) {
            ss << "[" << b.footprint[i].first << ", " << b.footprint[i].second << "]";
            if (i + 1 < b.footprint.size()) ss << ", ";
        }
        ss << "]]\n      }\n    }";
    }

    ss << "\n  ]\n}\n";
    return ss.str();
}

std::string InfraDocument::serializeNative() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"format\": \"mydesk-infra\",\n";
    ss << "  \"name\": \"" << escapeJson(name_) << "\",\n";
    ss << "  \"crs\": \"" << escapeJson(crs_) << "\",\n";
    ss << "  \"dem\": {\n";
    ss << "    \"originEasting\": " << dem_.originEasting << ",\n";
    ss << "    \"originNorthing\": " << dem_.originNorthing << ",\n";
    ss << "    \"cellSize\": " << dem_.cellSizeMeters << ",\n";
    ss << "    \"cols\": " << dem_.cols << ",\n";
    ss << "    \"rows\": " << dem_.rows << ",\n";
    ss << "    \"elevations\": [";
    for (std::size_t i = 0; i < dem_.elevations.size(); ++i) {
        ss << dem_.elevations[i];
        if (i + 1 < dem_.elevations.size()) ss << ", ";
    }
    ss << "]\n  },\n";
    ss << "  \"roads\": [\n";
    for (std::size_t i = 0; i < roads_.size(); ++i) {
        const auto& r = roads_[i];
        ss << "    {\"id\": " << r.id << ", \"name\": \"" << escapeJson(r.name) << "\", \"lanes\": " << r.numLanes
           << ", \"laneWidth\": " << r.laneWidthMeters << ", \"speed\": " << r.designSpeedKmh << ", \"pts\": [";
        for (std::size_t j = 0; j < r.centerline.size(); ++j) {
            const auto& p = r.centerline[j];
            ss << "[" << p.easting << ", " << p.northing << ", " << p.elevation << "]";
            if (j + 1 < r.centerline.size()) ss << ", ";
        }
        ss << "]}";
        if (i + 1 < roads_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n}\n";
    return ss.str();
}

InfraDocument InfraDocument::deserializeNative(std::string_view json, std::string* error) {
    InfraDocument doc;
    std::string format;
    if (!extractString(json, "format", format) || format != "mydesk-infra") {
        if (error) *error = "not a mydesk-infra document";
        return doc;
    }
    extractString(json, "name", doc.name_);
    extractString(json, "crs", doc.crs_);

    // DEM parsing
    const auto demPos = json.find("\"dem\":");
    if (demPos != std::string_view::npos) {
        double ox = 0, oy = 0, cs = 10, cols = 0, rows = 0;
        extractNumber(json.substr(demPos, 200), "originEasting", ox);
        extractNumber(json.substr(demPos, 200), "originNorthing", oy);
        extractNumber(json.substr(demPos, 200), "cellSize", cs);
        extractNumber(json.substr(demPos, 200), "cols", cols);
        extractNumber(json.substr(demPos, 200), "rows", rows);

        std::vector<double> elevs;
        const auto elevPos = json.find("\"elevations\":", demPos);
        if (elevPos != std::string_view::npos) {
            const auto lb = json.find('[', elevPos);
            const auto rb = json.find(']', lb);
            if (lb != std::string_view::npos && rb != std::string_view::npos) {
                std::string body(json.substr(lb + 1, rb - lb - 1));
                std::stringstream ss(body);
                std::string token;
                while (std::getline(ss, token, ',')) {
                    if (token.find_first_of("0123456789.-") == std::string::npos) continue;
                    elevs.push_back(std::stod(token));
                }
            }
        }
        doc.setDemGrid(ox, oy, cs, static_cast<std::size_t>(cols), static_cast<std::size_t>(rows), std::move(elevs));
    }

    // Roads parsing
    std::size_t search = json.find("\"roads\"");
    while (search != std::string_view::npos) {
        const auto idPos = json.find("\"id\":", search);
        if (idPos == std::string_view::npos) break;
        double id = 0, lanes = 2, laneW = 3.5, speed = 80;
        extractNumber(json.substr(idPos, 30), "id", id);
        std::string name;
        extractString(json.substr(idPos, 120), "name", name);
        extractNumber(json.substr(idPos, 120), "lanes", lanes);
        extractNumber(json.substr(idPos, 120), "laneWidth", laneW);
        extractNumber(json.substr(idPos, 120), "speed", speed);

        std::vector<RoadVertex> pts;
        const auto ptsPos = json.find("\"pts\":", idPos);
        if (ptsPos != std::string_view::npos) {
            const auto lb = json.find('[', ptsPos);
            const auto rb = json.find(']', ptsPos + 10);
            (void)lb; (void)rb;
        }

        doc.addRoad(name, static_cast<int>(lanes), laneW, speed, std::move(pts));
        search = idPos + 10;
    }

    return doc;
}

} // namespace mydesk::infra
