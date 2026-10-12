// SPDX-License-Identifier: MPL-2.0
#include "mydesk/infra/infra_document.hpp"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

namespace mydesk::infra {

namespace {

std::string escapeJson(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    constexpr char hex[] = "0123456789abcdef";
    for (const char raw : s) {
        const auto c = static_cast<unsigned char>(raw);
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                out += "\\u00";
                out.push_back(hex[c >> 4]);
                out.push_back(hex[c & 0x0f]);
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    return out;
}

using Json = nlohmann::json;

bool jsonIntegerInRange(const Json& value, int minimum, int& result) {
    if (!value.is_number_integer()) return false;
    if (value.is_number_unsigned()) {
        const auto number = value.get<std::uint64_t>();
        if (number > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) return false;
        result = static_cast<int>(number);
        return result >= minimum;
    }
    const auto number = value.get<std::int64_t>();
    if (number < minimum || number > std::numeric_limits<int>::max()) return false;
    result = static_cast<int>(number);
    return true;
}

bool atomicallyReplaceFile(const std::filesystem::path& source, const std::filesystem::path& destination) {
#ifdef _WIN32
    return MoveFileExW(source.c_str(), destination.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return std::rename(source.c_str(), destination.c_str()) == 0;
#endif
}

double planDistance(const RoadVertex& a, const RoadVertex& b) {
    return std::hypot(b.easting - a.easting, b.northing - a.northing);
}

double integratePositiveLinear(double a, double b, double width) {
    if (a >= 0.0 && b >= 0.0) return (a + b) * 0.5 * width;
    if (a <= 0.0 && b <= 0.0) return 0.0;
    const double positive = std::max(a, b);
    const double fraction = positive / (std::abs(a) + std::abs(b));
    return 0.5 * positive * width * fraction;
}

} // namespace

std::optional<double> DemGrid::elevationAt(double easting, double northing) const {
    if (cols < 2 || rows < 2 || cellSizeMeters <= 0.0 ||
        cols > std::numeric_limits<std::size_t>::max() / rows || elevations.size() != cols * rows) {
        return std::nullopt;
    }

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

InfraDocument::State InfraDocument::captureState() const {
    return {name_, crs_, dem_, roads_, bridges_, buildings_, nextId_};
}

void InfraDocument::restoreState(State state) {
    name_ = std::move(state.name);
    crs_ = std::move(state.crs);
    dem_ = std::move(state.dem);
    roads_ = std::move(state.roads);
    bridges_ = std::move(state.bridges);
    buildings_ = std::move(state.buildings);
    nextId_ = state.nextId;
}

void InfraDocument::checkpoint() {
    undoStack_.push_back(captureState());
    redoStack_.clear();
}

void InfraDocument::setName(std::string name) {
    if (name == name_) return;
    checkpoint();
    name_ = std::move(name);
}

void InfraDocument::setCrs(std::string crs) {
    if (crs == crs_) return;
    checkpoint();
    crs_ = std::move(crs);
}

void InfraDocument::setDemGrid(double originEasting, double originNorthing, double cellSize,
                               std::size_t cols, std::size_t rows, std::vector<double> elevations) {
    if (!std::isfinite(originEasting) || !std::isfinite(originNorthing) ||
        !std::isfinite(cellSize) || cellSize <= 0.0 || cols < 2 || rows < 2 ||
        cols > std::numeric_limits<std::size_t>::max() / rows || elevations.size() != cols * rows ||
        std::any_of(elevations.begin(), elevations.end(), [](double z) { return !std::isfinite(z); })) {
        throw std::invalid_argument("DEM must be a finite rectangular grid of at least 2x2 cells");
    }
    checkpoint();
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
    if (name.empty() || numLanes < 1 || !std::isfinite(laneWidth) || laneWidth <= 0.0 ||
        !std::isfinite(designSpeed) || designSpeed <= 0.0 || centerline.size() < 2 ||
        std::any_of(centerline.begin(), centerline.end(), [](const RoadVertex& point) {
            return !std::isfinite(point.easting) || !std::isfinite(point.northing) || !std::isfinite(point.elevation);
        })) {
        throw std::invalid_argument("road requires a name, positive parameters, and at least two finite vertices");
    }
    checkpoint();
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

bool InfraDocument::updateRoad(ElementId id, std::string name, int numLanes,
                               double laneWidth, double designSpeed) {
    if (name.empty() || numLanes < 1 || !std::isfinite(laneWidth) || laneWidth <= 0.0 ||
        !std::isfinite(designSpeed) || designSpeed <= 0.0) return false;
    const auto found = std::find_if(roads_.begin(), roads_.end(),
        [id](const ComponentRoad& road) { return road.id == id; });
    if (found == roads_.end()) return false;
    checkpoint();
    found->name = std::move(name); found->numLanes = numLanes;
    found->laneWidthMeters = laneWidth; found->designSpeedKmh = designSpeed;
    return true;
}

bool InfraDocument::updateRoadCenterline(ElementId id, std::vector<RoadVertex> centerline) {
    if (centerline.size() < 2 || std::any_of(centerline.begin(), centerline.end(), [](const RoadVertex& point) {
            return !std::isfinite(point.easting) || !std::isfinite(point.northing) || !std::isfinite(point.elevation);
        })) {
        return false;
    }
    const auto found = std::find_if(roads_.begin(), roads_.end(), [id](const ComponentRoad& road) { return road.id == id; });
    if (found == roads_.end()) return false;
    checkpoint();
    found->centerline = std::move(centerline);
    return true;
}

bool InfraDocument::removeRoad(ElementId id) {
    const auto found = std::find_if(roads_.begin(), roads_.end(), [id](const ComponentRoad& road) { return road.id == id; });
    if (found == roads_.end()) return false;
    checkpoint();
    roads_.erase(found);
    bridges_.erase(std::remove_if(bridges_.begin(), bridges_.end(),
                                  [id](const BridgeStructure& bridge) { return bridge.roadId == id; }),
                   bridges_.end());
    return true;
}

std::optional<CorridorEarthworkReport> InfraDocument::analyzeRoadEarthworks(
    ElementId roadId, double stationIntervalMeters, double shoulderWidthMeters,
    double crossSectionIntervalMeters) const {
    const ComponentRoad* road = findRoad(roadId);
    if (!road || road->centerline.size() < 2 || !std::isfinite(stationIntervalMeters) ||
        stationIntervalMeters <= 0.0 || !std::isfinite(shoulderWidthMeters) || shoulderWidthMeters < 0.0 ||
        !std::isfinite(crossSectionIntervalMeters) || crossSectionIntervalMeters <= 0.0) {
        return std::nullopt;
    }

    std::vector<double> cumulative(road->centerline.size(), 0.0);
    for (std::size_t i = 1; i < road->centerline.size(); ++i) {
        const double segment = planDistance(road->centerline[i - 1], road->centerline[i]);
        if (!std::isfinite(segment) || segment <= 1e-9 || !std::isfinite(cumulative[i - 1] + segment)) {
            return std::nullopt;
        }
        cumulative[i] = cumulative[i - 1] + segment;
    }
    const double totalStation = cumulative.back();
    if (totalStation <= 0.0) return std::nullopt;

    CorridorEarthworkReport report;
    report.roadId = roadId;
    report.formationWidthMeters = road->numLanes * road->laneWidthMeters + 2.0 * shoulderWidthMeters;
    if (report.formationWidthMeters <= 0.0) return std::nullopt;

    const double stationStepEstimate = std::ceil(totalStation / stationIntervalMeters);
    const double crossStepEstimate = std::ceil(report.formationWidthMeters / crossSectionIntervalMeters);
    if (!std::isfinite(stationStepEstimate) || !std::isfinite(crossStepEstimate) ||
        stationStepEstimate > 1'000'000.0 || crossStepEstimate > 100'000.0) {
        return std::nullopt;
    }
    const std::size_t stationSteps = static_cast<std::size_t>(stationStepEstimate);
    const std::size_t crossSteps = std::max<std::size_t>(1, static_cast<std::size_t>(crossStepEstimate));
    const double crossStep = report.formationWidthMeters / static_cast<double>(crossSteps);

    for (std::size_t sampleIndex = 0; sampleIndex <= stationSteps; ++sampleIndex) {
        const double station = sampleIndex == stationSteps
            ? totalStation
            : std::min(totalStation, static_cast<double>(sampleIndex) * stationIntervalMeters);
        const auto upper = std::upper_bound(cumulative.begin(), cumulative.end(), station);
        const std::size_t segmentIndex = upper == cumulative.begin()
            ? 0
            : std::min<std::size_t>(static_cast<std::size_t>(upper - cumulative.begin() - 1), road->centerline.size() - 2);
        const RoadVertex& a = road->centerline[segmentIndex];
        const RoadVertex& b = road->centerline[segmentIndex + 1];
        const double segmentLength = cumulative[segmentIndex + 1] - cumulative[segmentIndex];
        const double fraction = (station - cumulative[segmentIndex]) / segmentLength;

        CorridorSample sample;
        sample.stationMeters = station;
        sample.centerline = {
            a.easting + fraction * (b.easting - a.easting),
            a.northing + fraction * (b.northing - a.northing),
            a.elevation + fraction * (b.elevation - a.elevation)};
        const auto centerTerrain = terrainElevationAt(sample.centerline.easting, sample.centerline.northing);
        if (!centerTerrain) return std::nullopt;
        sample.terrainElevation = *centerTerrain;
        sample.gradePercent = 100.0 * (b.elevation - a.elevation) / segmentLength;
        report.maximumGradePercent = std::max(report.maximumGradePercent, std::abs(sample.gradePercent));

        const double tangentE = (b.easting - a.easting) / segmentLength;
        const double tangentN = (b.northing - a.northing) / segmentLength;
        const double normalE = -tangentN;
        const double normalN = tangentE;
        double previousDelta = 0.0;
        for (std::size_t crossIndex = 0; crossIndex <= crossSteps; ++crossIndex) {
            const double offset = -report.formationWidthMeters * 0.5 + static_cast<double>(crossIndex) * crossStep;
            const auto terrain = terrainElevationAt(sample.centerline.easting + normalE * offset,
                                                    sample.centerline.northing + normalN * offset);
            if (!terrain) return std::nullopt;
            const double delta = *terrain - sample.centerline.elevation;
            if (crossIndex > 0) {
                sample.cutAreaSquareMeters += integratePositiveLinear(previousDelta, delta, crossStep);
                sample.fillAreaSquareMeters += integratePositiveLinear(-previousDelta, -delta, crossStep);
            }
            previousDelta = delta;
        }

        if (!report.samples.empty()) {
            const CorridorSample& previous = report.samples.back();
            const double distance = sample.stationMeters - previous.stationMeters;
            report.cutVolumeCubicMeters += 0.5 * (previous.cutAreaSquareMeters + sample.cutAreaSquareMeters) * distance;
            report.fillVolumeCubicMeters += 0.5 * (previous.fillAreaSquareMeters + sample.fillAreaSquareMeters) * distance;
        }
        report.samples.push_back(sample);
    }
    return report;
}

ElementId InfraDocument::addBridge(std::string name, ElementId roadId, double startSta, double endSta, int piers) {
    const ComponentRoad* road = findRoad(roadId);
    if (!road || name.empty() || !std::isfinite(startSta) || !std::isfinite(endSta) ||
        startSta < 0.0 || endSta <= startSta || endSta > road->totalLengthMeters() + 1e-9 || piers < 0) {
        throw std::invalid_argument("bridge requires an existing road and a valid station range");
    }
    checkpoint();
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

const BridgeStructure* InfraDocument::findBridge(ElementId id) const {
    const auto found = std::find_if(bridges_.begin(), bridges_.end(),
        [id](const BridgeStructure& bridge) { return bridge.id == id; });
    return found == bridges_.end() ? nullptr : &*found;
}

bool InfraDocument::updateBridge(ElementId id, std::string name, double startSta, double endSta,
                                 int piers, double deckThickness) {
    const auto found = std::find_if(bridges_.begin(), bridges_.end(),
        [id](const BridgeStructure& bridge) { return bridge.id == id; });
    if (found == bridges_.end() || name.empty() || !std::isfinite(startSta) ||
        !std::isfinite(endSta) || startSta < 0.0 || endSta <= startSta || piers < 0 ||
        !std::isfinite(deckThickness) || deckThickness <= 0.0) return false;
    const auto* road = findRoad(found->roadId);
    if (!road || endSta > road->totalLengthMeters() + 1e-9) return false;
    checkpoint();
    found->name = std::move(name); found->startStation = startSta; found->endStation = endSta;
    found->pierCount = piers; found->deckThicknessMeters = deckThickness;
    return true;
}

ElementId InfraDocument::addBuilding(std::string name, std::string usage, double roofHeight,
                                     std::vector<std::pair<double, double>> footprint) {
    if (name.empty() || usage.empty() || !std::isfinite(roofHeight) || roofHeight <= 0.0 || footprint.size() < 3 ||
        std::any_of(footprint.begin(), footprint.end(), [](const auto& point) {
            return !std::isfinite(point.first) || !std::isfinite(point.second);
        })) {
        throw std::invalid_argument("building requires a name, usage, positive height, and a finite polygon");
    }
    checkpoint();
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

const CityBuilding* InfraDocument::findBuilding(ElementId id) const {
    const auto found = std::find_if(buildings_.begin(), buildings_.end(),
        [id](const CityBuilding& building) { return building.id == id; });
    return found == buildings_.end() ? nullptr : &*found;
}

bool InfraDocument::updateBuilding(ElementId id, std::string name, std::string usage,
                                   double roofHeight) {
    const auto found = std::find_if(buildings_.begin(), buildings_.end(),
        [id](const CityBuilding& building) { return building.id == id; });
    if (found == buildings_.end() || name.empty() || usage.empty() ||
        !std::isfinite(roofHeight) || roofHeight <= 0.0) return false;
    checkpoint();
    found->name = std::move(name); found->usageCategory = std::move(usage);
    found->roofHeightMeters = roofHeight;
    return true;
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
    Json root;
    root["format"] = "mydesk-infra";
    root["schemaVersion"] = 2;
    root["name"] = name_;
    root["crs"] = crs_;
    root["dem"] = {
        {"originEasting", dem_.originEasting},
        {"originNorthing", dem_.originNorthing},
        {"cellSize", dem_.cellSizeMeters},
        {"cols", dem_.cols},
        {"rows", dem_.rows},
        {"elevations", dem_.elevations}
    };
    root["roads"] = Json::array();
    for (const ComponentRoad& road : roads_) {
        Json points = Json::array();
        for (const RoadVertex& point : road.centerline) {
            points.push_back(Json::array({point.easting, point.northing, point.elevation}));
        }
        root["roads"].push_back({
            {"id", road.id}, {"name", road.name}, {"lanes", road.numLanes},
            {"laneWidth", road.laneWidthMeters}, {"speed", road.designSpeedKmh},
            {"pts", std::move(points)}
        });
    }
    root["bridges"] = Json::array();
    for (const BridgeStructure& bridge : bridges_) {
        root["bridges"].push_back({
            {"id", bridge.id}, {"name", bridge.name}, {"roadId", bridge.roadId},
            {"startStation", bridge.startStation}, {"endStation", bridge.endStation},
            {"pierCount", bridge.pierCount}, {"deckThickness", bridge.deckThicknessMeters}
        });
    }
    root["buildings"] = Json::array();
    for (const CityBuilding& building : buildings_) {
        Json footprint = Json::array();
        for (const auto& point : building.footprint) {
            footprint.push_back(Json::array({point.first, point.second}));
        }
        root["buildings"].push_back({
            {"id", building.id}, {"name", building.name}, {"usage", building.usageCategory},
            {"baseElevation", building.baseElevation}, {"roofHeight", building.roofHeightMeters},
            {"footprint", std::move(footprint)}
        });
    }
    return root.dump(2) + '\n';
}

InfraDocument InfraDocument::deserializeNative(std::string_view source, std::string* error) {
    InfraDocument doc;
    if (error) error->clear();
    const auto fail = [&](std::string message) {
        if (error) *error = std::move(message);
        return InfraDocument{};
    };

    Json root = Json::parse(source.begin(), source.end(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) return fail("project is not valid JSON object data");
    if (!root.contains("format") || !root["format"].is_string() ||
        root["format"].get<std::string>() != "mydesk-infra") {
        return fail("not a mydesk-infra document");
    }
    int schemaVersion = 1; // Version 1 documents did not carry an explicit field.
    if (root.contains("schemaVersion")) {
        if (!jsonIntegerInRange(root["schemaVersion"], 1, schemaVersion) || schemaVersion > 2) {
            return fail("unsupported mydesk-infra schema version");
        }
    }
    if (!root.contains("name") || !root["name"].is_string() ||
        !root.contains("crs") || !root["crs"].is_string()) {
        return fail("project name and CRS must be strings");
    }
    doc.name_ = root["name"].get<std::string>();
    doc.crs_ = root["crs"].get<std::string>();

    if (!root.contains("dem") || !root["dem"].is_object()) return fail("field 'dem' must be an object");
    const Json& dem = root["dem"];
    for (const char* key : {"originEasting", "originNorthing", "cellSize", "cols", "rows", "elevations"}) {
        if (!dem.contains(key)) return fail("DEM is missing field '" + std::string(key) + "'");
    }
    if (!dem["originEasting"].is_number() || !dem["originNorthing"].is_number() ||
        !dem["cellSize"].is_number() || !dem["cols"].is_number_unsigned() ||
        !dem["rows"].is_number_unsigned() || !dem["elevations"].is_array()) {
        return fail("DEM fields have invalid types");
    }
    doc.dem_.originEasting = dem["originEasting"].get<double>();
    doc.dem_.originNorthing = dem["originNorthing"].get<double>();
    doc.dem_.cellSizeMeters = dem["cellSize"].get<double>();
    doc.dem_.cols = dem["cols"].get<std::size_t>();
    doc.dem_.rows = dem["rows"].get<std::size_t>();
    const bool emptyDem = doc.dem_.cols == 0 && doc.dem_.rows == 0 && dem["elevations"].empty();
    const bool validPopulatedDem = doc.dem_.cols >= 2 && doc.dem_.rows >= 2 &&
        doc.dem_.cols <= std::numeric_limits<std::size_t>::max() / doc.dem_.rows;
    if (!std::isfinite(doc.dem_.originEasting) || !std::isfinite(doc.dem_.originNorthing) ||
        !std::isfinite(doc.dem_.cellSizeMeters) || doc.dem_.cellSizeMeters <= 0.0 ||
        (!emptyDem && !validPopulatedDem)) {
        return fail("DEM dimensions and cell size are invalid");
    }
    for (const Json& elevation : dem["elevations"]) {
        if (!elevation.is_number()) return fail("DEM elevations must contain only numbers");
        const double value = elevation.get<double>();
        if (!std::isfinite(value)) return fail("DEM elevations must be finite");
        doc.dem_.elevations.push_back(value);
    }
    if (doc.dem_.elevations.size() != doc.dem_.cols * doc.dem_.rows) {
        return fail("DEM elevation count does not match rows multiplied by columns");
    }

    std::set<ElementId> ids;
    std::set<ElementId> roadIds;
    ElementId maximumId = 0;
    if (!root.contains("roads") || !root["roads"].is_array()) return fail("field 'roads' must be an array");
    for (const Json& item : root["roads"]) {
        if (!item.is_object()) return fail("each road must be an object");
        for (const char* key : {"id", "name", "lanes", "laneWidth", "speed", "pts"}) {
            if (!item.contains(key)) return fail("road is missing field '" + std::string(key) + "'");
        }
        if (!item["id"].is_number_unsigned() || !item["name"].is_string() ||
            !item["lanes"].is_number_integer() || !item["laneWidth"].is_number() ||
            !item["speed"].is_number() || !item["pts"].is_array()) {
            return fail("road fields have invalid types");
        }
        ComponentRoad road;
        road.id = item["id"].get<ElementId>();
        road.name = item["name"].get<std::string>();
        if (!jsonIntegerInRange(item["lanes"], 1, road.numLanes)) return fail("road lane count is invalid");
        road.laneWidthMeters = item["laneWidth"].get<double>();
        road.designSpeedKmh = item["speed"].get<double>();
        if (road.id == 0 || !ids.insert(road.id).second || road.name.empty() || road.numLanes < 1 ||
            !std::isfinite(road.laneWidthMeters) || road.laneWidthMeters <= 0.0 ||
            !std::isfinite(road.designSpeedKmh) || road.designSpeedKmh <= 0.0) {
            return fail("road identity or parameters are invalid");
        }
        for (const Json& point : item["pts"]) {
            if (!point.is_array() || point.size() != 3 ||
                !point[0].is_number() || !point[1].is_number() || !point[2].is_number()) {
                return fail("each road point must contain exactly three numbers");
            }
            RoadVertex vertex{point[0].get<double>(), point[1].get<double>(), point[2].get<double>()};
            if (!std::isfinite(vertex.easting) || !std::isfinite(vertex.northing) || !std::isfinite(vertex.elevation)) {
                return fail("road point coordinates must be finite");
            }
            road.centerline.push_back(vertex);
        }
        if (road.centerline.size() < 2) return fail("each road must contain at least two points");
        roadIds.insert(road.id);
        maximumId = std::max(maximumId, road.id);
        doc.roads_.push_back(std::move(road));
    }

    if (root.contains("bridges")) {
        if (!root["bridges"].is_array()) return fail("field 'bridges' must be an array");
        for (const Json& item : root["bridges"]) {
            if (!item.is_object()) return fail("each bridge must be an object");
            for (const char* key : {"id", "name", "roadId", "startStation", "endStation", "pierCount", "deckThickness"}) {
                if (!item.contains(key)) return fail("bridge is missing field '" + std::string(key) + "'");
            }
            if (!item["id"].is_number_unsigned() || !item["roadId"].is_number_unsigned() ||
                !item["name"].is_string() || !item["startStation"].is_number() ||
                !item["endStation"].is_number() || !item["pierCount"].is_number_integer() ||
                !item["deckThickness"].is_number()) {
                return fail("bridge fields have invalid types");
            }
            BridgeStructure bridge;
            bridge.id = item["id"].get<ElementId>();
            bridge.name = item["name"].get<std::string>();
            bridge.roadId = item["roadId"].get<ElementId>();
            bridge.startStation = item["startStation"].get<double>();
            bridge.endStation = item["endStation"].get<double>();
            if (!jsonIntegerInRange(item["pierCount"], 0, bridge.pierCount)) return fail("bridge pier count is invalid");
            bridge.deckThicknessMeters = item["deckThickness"].get<double>();
            if (bridge.id == 0 || !ids.insert(bridge.id).second || !roadIds.contains(bridge.roadId) ||
                bridge.name.empty() || !std::isfinite(bridge.startStation) || !std::isfinite(bridge.endStation) ||
                bridge.startStation < 0.0 || bridge.endStation <= bridge.startStation || bridge.pierCount < 0 ||
                !std::isfinite(bridge.deckThicknessMeters) || bridge.deckThicknessMeters <= 0.0) {
                return fail("bridge identity or parameters are invalid");
            }
            const auto road = std::find_if(doc.roads_.begin(), doc.roads_.end(),
                                           [&](const ComponentRoad& candidate) { return candidate.id == bridge.roadId; });
            if (road == doc.roads_.end() || bridge.endStation > road->totalLengthMeters() + 1e-9) {
                return fail("bridge station range exceeds its referenced road");
            }
            maximumId = std::max(maximumId, bridge.id);
            doc.bridges_.push_back(std::move(bridge));
        }
    }

    if (root.contains("buildings")) {
        if (!root["buildings"].is_array()) return fail("field 'buildings' must be an array");
        for (const Json& item : root["buildings"]) {
            if (!item.is_object()) return fail("each building must be an object");
            for (const char* key : {"id", "name", "usage", "baseElevation", "roofHeight", "footprint"}) {
                if (!item.contains(key)) return fail("building is missing field '" + std::string(key) + "'");
            }
            if (!item["id"].is_number_unsigned() || !item["name"].is_string() || !item["usage"].is_string() ||
                !item["baseElevation"].is_number() || !item["roofHeight"].is_number() || !item["footprint"].is_array()) {
                return fail("building fields have invalid types");
            }
            CityBuilding building;
            building.id = item["id"].get<ElementId>();
            building.name = item["name"].get<std::string>();
            building.usageCategory = item["usage"].get<std::string>();
            building.baseElevation = item["baseElevation"].get<double>();
            building.roofHeightMeters = item["roofHeight"].get<double>();
            if (building.id == 0 || !ids.insert(building.id).second || building.name.empty() ||
                building.usageCategory.empty() || !std::isfinite(building.baseElevation) ||
                !std::isfinite(building.roofHeightMeters) || building.roofHeightMeters <= 0.0) {
                return fail("building identity or parameters are invalid");
            }
            for (const Json& point : item["footprint"]) {
                if (!point.is_array() || point.size() != 2 || !point[0].is_number() || !point[1].is_number()) {
                    return fail("each footprint point must contain exactly two numbers");
                }
                const double easting = point[0].get<double>();
                const double northing = point[1].get<double>();
                if (!std::isfinite(easting) || !std::isfinite(northing)) return fail("footprint coordinates must be finite");
                building.footprint.emplace_back(easting, northing);
            }
            if (building.footprint.size() < 3) return fail("each building footprint must contain at least three points");
            maximumId = std::max(maximumId, building.id);
            doc.buildings_.push_back(std::move(building));
        }
    }
    if (maximumId == std::numeric_limits<ElementId>::max()) return fail("element id space is exhausted");
    doc.nextId_ = maximumId + 1;
    doc.clearHistory();
    return doc;
}

bool InfraDocument::saveNativeFile(const std::filesystem::path& path, std::string* error) const {
    if (error) error->clear();
    if (path.empty()) {
        if (error) *error = "save path is empty";
        return false;
    }
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            if (error) *error = "cannot open temporary project file for writing";
            return false;
        }
        const std::string contents = serializeNative();
        stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        stream.flush();
        if (!stream) {
            stream.close();
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            if (error) *error = "failed while writing temporary project file";
            return false;
        }
    }
    if (!atomicallyReplaceFile(temporary, path)) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        if (error) *error = "failed to atomically replace project file";
        return false;
    }
    return true;
}

std::optional<InfraDocument> InfraDocument::loadNativeFile(const std::filesystem::path& path, std::string* error) {
    if (error) error->clear();
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        if (error) *error = "cannot open project file";
        return std::nullopt;
    }
    const std::streamoff size = stream.tellg();
    constexpr std::streamoff maximumProjectBytes = 256 * 1024 * 1024;
    if (size < 0 || size > maximumProjectBytes) {
        if (error) *error = "project file is empty, unreadable, or exceeds the 256 MiB safety limit";
        return std::nullopt;
    }
    std::string contents(static_cast<std::size_t>(size), '\0');
    stream.seekg(0);
    stream.read(contents.data(), size);
    if (!stream && size != 0) {
        if (error) *error = "failed while reading project file";
        return std::nullopt;
    }
    std::string parseError;
    InfraDocument document = deserializeNative(contents, &parseError);
    if (!parseError.empty()) {
        if (error) *error = std::move(parseError);
        return std::nullopt;
    }
    return document;
}

bool InfraDocument::undo() {
    if (undoStack_.empty()) return false;
    redoStack_.push_back(captureState());
    State state = std::move(undoStack_.back());
    undoStack_.pop_back();
    restoreState(std::move(state));
    return true;
}

bool InfraDocument::redo() {
    if (redoStack_.empty()) return false;
    undoStack_.push_back(captureState());
    State state = std::move(redoStack_.back());
    redoStack_.pop_back();
    restoreState(std::move(state));
    return true;
}

void InfraDocument::clearHistory() {
    undoStack_.clear();
    redoStack_.clear();
}

} // namespace mydesk::infra
