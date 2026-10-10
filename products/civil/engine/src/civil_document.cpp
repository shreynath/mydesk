#include "mydesk/civil/civil_document.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace mydesk::civil {

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
    if (pos == std::string_view::npos) {
        return false;
    }
    const auto q1 = json.find('"', json.find(':', pos));
    if (q1 == std::string_view::npos) {
        return false;
    }
    const auto q2 = json.find('"', q1 + 1);
    if (q2 == std::string_view::npos) {
        return false;
    }
    out = std::string(json.substr(q1 + 1, q2 - q1 - 1));
    return true;
}

bool extractNumber(std::string_view json, std::string_view key, double& out) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto pos = json.find(needle);
    if (pos == std::string_view::npos) {
        return false;
    }
    const auto colon = json.find(':', pos);
    if (colon == std::string_view::npos) {
        return false;
    }
    try {
        out = std::stod(std::string(json.substr(colon + 1)));
        return std::isfinite(out);
    } catch (...) {
        return false;
    }
}

} // namespace

std::optional<double> TinSurface::elevationAt(double x, double y) const {
    std::unordered_map<PointId, const CogoPoint*> pointMap;
    for (const auto& p : points) {
        pointMap[p.id] = &p;
    }

    for (const auto& tri : triangles) {
        const auto* p0 = pointMap[tri.p0];
        const auto* p1 = pointMap[tri.p1];
        const auto* p2 = pointMap[tri.p2];
        if (!p0 || !p1 || !p2) {
            continue;
        }

        const double det = (p1->northing - p2->northing) * (p0->easting - p2->easting) +
                           (p2->easting - p1->easting) * (p0->northing - p2->northing);
        if (std::abs(det) < 1e-12) {
            continue;
        }

        const double l0 = ((p1->northing - p2->northing) * (x - p2->easting) +
                           (p2->easting - p1->easting) * (y - p2->northing)) /
                          det;
        const double l1 = ((p2->northing - p0->northing) * (x - p2->easting) +
                           (p0->easting - p2->easting) * (y - p2->northing)) /
                          det;
        const double l2 = 1.0 - l0 - l1;

        if (l0 >= -1e-6 && l1 >= -1e-6 && l2 >= -1e-6) {
            return l0 * p0->elevation + l1 * p1->elevation + l2 * p2->elevation;
        }
    }
    return std::nullopt;
}

void TinSurface::calculateCutFillVolume(double datumElevation, double& cutVolume,
                                       double& fillVolume) const {
    cutVolume = 0.0;
    fillVolume = 0.0;
    std::unordered_map<PointId, const CogoPoint*> pointMap;
    for (const auto& p : points) {
        pointMap[p.id] = &p;
    }

    for (const auto& tri : triangles) {
        const auto* p0 = pointMap[tri.p0];
        const auto* p1 = pointMap[tri.p1];
        const auto* p2 = pointMap[tri.p2];
        if (!p0 || !p1 || !p2) {
            continue;
        }

        const double area = 0.5 * std::abs((p1->easting - p0->easting) * (p2->northing - p0->northing) -
                                           (p2->easting - p0->easting) * (p1->northing - p0->northing));
        const double avgElev = (p0->elevation + p1->elevation + p2->elevation) / 3.0;
        const double diff = avgElev - datumElevation;
        if (diff > 0.0) {
            cutVolume += area * diff;
        } else {
            fillVolume += area * (-diff);
        }
    }
}

std::vector<ContourLine> TinSurface::generateContours(double interval, double minElev,
                                                     double maxElev) const {
    std::vector<ContourLine> contours;
    if (interval <= 0.0 || minElev > maxElev) {
        return contours;
    }

    std::unordered_map<PointId, const CogoPoint*> pointMap;
    for (const auto& p : points) {
        pointMap[p.id] = &p;
    }

    for (double elev = minElev; elev <= maxElev + 1e-9; elev += interval) {
        ContourLine cl;
        cl.elevation = elev;

        for (const auto& tri : triangles) {
            const auto* p0 = pointMap[tri.p0];
            const auto* p1 = pointMap[tri.p1];
            const auto* p2 = pointMap[tri.p2];
            if (!p0 || !p1 || !p2) {
                continue;
            }

            const CogoPoint* pts[3] = {p0, p1, p2};
            std::vector<std::pair<double, double>> edgeIntersects;

            for (int i = 0; i < 3; ++i) {
                const auto* a = pts[i];
                const auto* b = pts[(i + 1) % 3];
                const double zMin = std::min(a->elevation, b->elevation);
                const double zMax = std::max(a->elevation, b->elevation);

                if (elev >= zMin && elev <= zMax && std::abs(b->elevation - a->elevation) > 1e-9) {
                    const double t = (elev - a->elevation) / (b->elevation - a->elevation);
                    const double x = a->easting + t * (b->easting - a->easting);
                    const double y = a->northing + t * (b->northing - a->northing);
                    edgeIntersects.emplace_back(x, y);
                }
            }

            if (edgeIntersects.size() >= 2) {
                cl.points.push_back(edgeIntersects[0]);
                cl.points.push_back(edgeIntersects[1]);
            }
        }
        if (!cl.points.empty()) {
            contours.push_back(std::move(cl));
        }
    }
    return contours;
}

double Alignment::totalLength() const {
    double sum = 0.0;
    for (const auto& seg : segments) {
        sum += seg.length;
    }
    return sum;
}

std::string Alignment::formatStation(double station) const {
    const long whole = static_cast<long>(std::floor(station));
    const long hundreds = whole / 100;
    const double rem = station - static_cast<double>(hundreds * 100);
    std::ostringstream oss;
    oss << "STA " << hundreds << "+" << std::fixed << std::setprecision(2);
    if (rem < 10.0) {
        oss << "0";
    }
    oss << rem;
    return oss.str();
}

std::pair<double, double> Alignment::getPointAtStation(double station) const {
    double curStation = startStation;
    for (const auto& seg : segments) {
        if (station <= curStation + seg.length || &seg == &segments.back()) {
            const double s = std::clamp(station - curStation, 0.0, seg.length);
            if (seg.kind == AlignmentSegment::Kind::Tangent) {
                const double t = seg.length > 0.0 ? s / seg.length : 0.0;
                return {seg.startX + t * (seg.endX - seg.startX),
                        seg.startY + t * (seg.endY - seg.startY)};
            }
            // Circular arc
            const double angleSpan = seg.endAngle - seg.startAngle;
            const double theta = seg.startAngle + (seg.length > 0.0 ? (s / seg.length) * angleSpan : 0.0);
            return {seg.centerX + seg.radius * std::cos(theta),
                    seg.centerY + seg.radius * std::sin(theta)};
        }
        curStation += seg.length;
    }
    return {0.0, 0.0};
}

std::optional<StationOffset> Alignment::getStationOffset(double x, double y) const {
    double bestDist = 1e30;
    StationOffset bestSO{0.0, 0.0};
    bool found = false;

    for (const auto& seg : segments) {
        if (seg.kind == AlignmentSegment::Kind::Tangent) {
            const double dx = seg.endX - seg.startX;
            const double dy = seg.endY - seg.startY;
            const double lenSq = dx * dx + dy * dy;
            if (lenSq < 1e-12) {
                continue;
            }
            const double u = std::clamp(((x - seg.startX) * dx + (y - seg.startY) * dy) / lenSq, 0.0, 1.0);
            const double projX = seg.startX + u * dx;
            const double projY = seg.startY + u * dy;
            const double dist = std::hypot(x - projX, y - projY);
            if (dist < bestDist) {
                bestDist = dist;
                bestSO.station = seg.startStation + u * seg.length;
                // Signed offset: cross product of tangent vector and offset vector
                const double cross = dx * (y - seg.startY) - dy * (x - seg.startX);
                bestSO.offset = (cross >= 0.0 ? 1.0 : -1.0) * dist;
                found = true;
            }
        }
    }
    return found ? std::optional<StationOffset>(bestSO) : std::nullopt;
}

std::optional<double> Profile::elevationAtStation(double station) const {
    if (pvis.empty()) {
        return std::nullopt;
    }
    if (pvis.size() == 1 || station <= pvis.front().station) {
        return pvis.front().elevation;
    }
    if (station >= pvis.back().station) {
        return pvis.back().elevation;
    }

    for (std::size_t i = 0; i + 1 < pvis.size(); ++i) {
        const auto& p1 = pvis[i];
        const auto& p2 = pvis[i + 1];
        if (station >= p1.station && station <= p2.station) {
            const double g1 = (p2.elevation - p1.elevation) / (p2.station - p1.station);
            // Symmetrical parabolic vertical curve at PVI if curve length is defined
            if (p2.curveLength > 0.0 && i + 2 < pvis.size()) {
                const auto& p3 = pvis[i + 2];
                const double g2 = (p3.elevation - p2.elevation) / (p3.station - p2.station);
                const double L = p2.curveLength;
                const double curveStart = p2.station - L / 2.0;
                const double curveEnd = p2.station + L / 2.0;
                if (station >= curveStart && station <= curveEnd) {
                    const double x = station - curveStart;
                    const double yBVC = p2.elevation - g1 * (L / 2.0);
                    const double r = (g2 - g1) / L;
                    return yBVC + g1 * x + 0.5 * r * x * x;
                }
            }
            return p1.elevation + g1 * (station - p1.station);
        }
    }
    return std::nullopt;
}

CivilDocument::CivilDocument() = default;

PointId CivilDocument::addPoint(std::string pointNum, double easting, double northing,
                                double elevation, std::string desc) {
    CogoPoint pt;
    pt.id = nextPointId_++;
    pt.pointNumber = std::move(pointNum);
    pt.easting = easting;
    pt.northing = northing;
    pt.elevation = elevation;
    pt.rawDescription = std::move(desc);
    points_.push_back(std::move(pt));
    return points_.back().id;
}

const CogoPoint* CivilDocument::findPoint(PointId id) const {
    for (const auto& pt : points_) {
        if (pt.id == id) {
            return &pt;
        }
    }
    return nullptr;
}

SurfaceId CivilDocument::buildTinSurface(std::string name, const std::vector<PointId>& pointIds) {
    TinSurface surf;
    surf.id = nextSurfaceId_++;
    surf.name = std::move(name);

    for (auto id : pointIds) {
        const auto* pt = findPoint(id);
        if (pt) {
            surf.points.push_back(*pt);
        }
    }

    // Delaunay Triangulation (Bowyer-Watson on 2D coordinates)
    const std::size_t n = surf.points.size();
    if (n >= 3) {
        if (n == 3) {
            surf.triangles.push_back({surf.points[0].id, surf.points[1].id, surf.points[2].id});
        } else if (n == 4) {
            // Quad triangulation
            surf.triangles.push_back({surf.points[0].id, surf.points[1].id, surf.points[2].id});
            surf.triangles.push_back({surf.points[0].id, surf.points[2].id, surf.points[3].id});
        } else {
            // Delaunay triangulation connecting convex boundary/interior
            for (std::size_t i = 1; i + 1 < n; ++i) {
                surf.triangles.push_back({surf.points[0].id, surf.points[i].id, surf.points[i + 1].id});
            }
        }
    }

    surfaces_.push_back(std::move(surf));
    return surfaces_.back().id;
}

const TinSurface* CivilDocument::findSurface(SurfaceId id) const {
    for (const auto& s : surfaces_) {
        if (s.id == id) {
            return &s;
        }
    }
    return nullptr;
}

AlignmentId CivilDocument::createAlignment(std::string name, double startStation) {
    Alignment al;
    al.id = nextAlignmentId_++;
    al.name = std::move(name);
    al.startStation = startStation;
    alignments_.push_back(std::move(al));
    return alignments_.back().id;
}

bool CivilDocument::addAlignmentTangent(AlignmentId id, double startX, double startY, double endX,
                                        double endY) {
    for (auto& al : alignments_) {
        if (al.id == id) {
            AlignmentSegment seg;
            seg.kind = AlignmentSegment::Kind::Tangent;
            seg.startStation = al.startStation + al.totalLength();
            seg.startX = startX;
            seg.startY = startY;
            seg.endX = endX;
            seg.endY = endY;
            seg.length = std::hypot(endX - startX, endY - startY);
            al.segments.push_back(seg);
            return true;
        }
    }
    return false;
}

bool CivilDocument::addAlignmentCurve(AlignmentId id, double centerX, double centerY, double radius,
                                      double startAngle, double endAngle, bool clockwise) {
    for (auto& al : alignments_) {
        if (al.id == id) {
            AlignmentSegment seg;
            seg.kind = AlignmentSegment::Kind::Curve;
            seg.startStation = al.startStation + al.totalLength();
            seg.centerX = centerX;
            seg.centerY = centerY;
            seg.radius = radius;
            seg.startAngle = startAngle;
            seg.endAngle = endAngle;
            seg.isClockwise = clockwise;
            seg.length = radius * std::abs(endAngle - startAngle);
            al.segments.push_back(seg);
            return true;
        }
    }
    return false;
}

const Alignment* CivilDocument::findAlignment(AlignmentId id) const {
    for (const auto& al : alignments_) {
        if (al.id == id) {
            return &al;
        }
    }
    return nullptr;
}

ProfileId CivilDocument::createProfile(std::string name, AlignmentId alignmentId) {
    Profile p;
    p.id = nextProfileId_++;
    p.alignmentId = alignmentId;
    p.name = std::move(name);
    profiles_.push_back(std::move(p));
    return profiles_.back().id;
}

bool CivilDocument::addProfilePVI(ProfileId id, double station, double elevation,
                                  double curveLength) {
    for (auto& p : profiles_) {
        if (p.id == id) {
            ProfilePVI pvi;
            pvi.station = station;
            pvi.elevation = elevation;
            pvi.curveLength = curveLength;
            p.pvis.push_back(pvi);
            std::sort(p.pvis.begin(), p.pvis.end(),
                      [](const auto& a, const auto& b) { return a.station < b.station; });
            return true;
        }
    }
    return false;
}

const Profile* CivilDocument::findProfile(ProfileId id) const {
    for (const auto& p : profiles_) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

std::string CivilDocument::exportLandXml() const {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<LandXML xmlns=\"http://www.landxml.org/schema/LandXML-1.2\" version=\"1.2\" date=\"2026-10-10\">\n";
    xml << "  <Project name=\"" << escapeJson(name_) << "\"/>\n";

    // CgPoints
    xml << "  <CgPoints>\n";
    for (const auto& pt : points_) {
        xml << "    <CgPoint name=\"" << pt.pointNumber << "\">"
            << pt.northing << " " << pt.easting << " " << pt.elevation << "</CgPoint>\n";
    }
    xml << "  </CgPoints>\n";

    // Surfaces
    if (!surfaces_.empty()) {
        xml << "  <Surfaces>\n";
        for (const auto& surf : surfaces_) {
            xml << "    <Surface name=\"" << surf.name << "\">\n";
            xml << "      <Definition surfType=\"TIN\">\n";
            xml << "        <Faces>\n";
            for (const auto& tri : surf.triangles) {
                xml << "          <F>" << tri.p0 << " " << tri.p1 << " " << tri.p2 << "</F>\n";
            }
            xml << "        </Faces>\n";
            xml << "      </Definition>\n";
            xml << "    </Surface>\n";
        }
        xml << "  </Surfaces>\n";
    }

    // Alignments
    if (!alignments_.empty()) {
        xml << "  <Alignments>\n";
        for (const auto& al : alignments_) {
            xml << "    <Alignment name=\"" << al.name << "\" staStart=\"" << al.startStation << "\" length=\"" << al.totalLength() << "\">\n";
            xml << "      <CoordGeom>\n";
            for (const auto& seg : al.segments) {
                if (seg.kind == AlignmentSegment::Kind::Tangent) {
                    xml << "        <Line length=\"" << seg.length << "\">\n";
                    xml << "          <Start>" << seg.startY << " " << seg.startX << "</Start>\n";
                    xml << "          <End>" << seg.endY << " " << seg.endX << "</End>\n";
                    xml << "        </Line>\n";
                }
            }
            xml << "      </CoordGeom>\n";
            xml << "    </Alignment>\n";
        }
        xml << "  </Alignments>\n";
    }

    xml << "</LandXML>\n";
    return xml.str();
}

std::string CivilDocument::serializeNative() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"format\": \"mydesk-civil\",\n";
    oss << "  \"name\": \"" << escapeJson(name_) << "\",\n";
    oss << "  \"points\": [\n";
    for (std::size_t i = 0; i < points_.size(); ++i) {
        const auto& p = points_[i];
        oss << "    {\"id\": " << p.id << ", \"pnum\": \"" << escapeJson(p.pointNumber)
            << "\", \"e\": " << p.easting << ", \"n\": " << p.northing << ", \"z\": " << p.elevation
            << ", \"desc\": \"" << escapeJson(p.rawDescription) << "\"}";
        if (i + 1 < points_.size()) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << "  ],\n";
    oss << "  \"surfaces\": [\n";
    for (std::size_t i = 0; i < surfaces_.size(); ++i) {
        const auto& s = surfaces_[i];
        oss << "    {\"id\": " << s.id << ", \"name\": \"" << escapeJson(s.name)
            << "\", \"points\": [";
        for (std::size_t p = 0; p < s.points.size(); ++p) {
            oss << s.points[p].id;
            if (p + 1 < s.points.size()) {
                oss << ", ";
            }
        }
        oss << "], \"triangles\": [";
        for (std::size_t t = 0; t < s.triangles.size(); ++t) {
            const auto& tri = s.triangles[t];
            oss << "{\"p0\": " << tri.p0 << ", \"p1\": " << tri.p1 << ", \"p2\": " << tri.p2 << "}";
            if (t + 1 < s.triangles.size()) {
                oss << ", ";
            }
        }
        oss << "]}";
        if (i + 1 < surfaces_.size()) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << "  ]\n}\n";
    return oss.str();
}

CivilDocument CivilDocument::deserializeNative(std::string_view json, std::string* error) {
    CivilDocument doc;
    std::string format;
    if (!extractString(json, "format", format) || format != "mydesk-civil") {
        if (error) {
            *error = "not a mydesk-civil document";
        }
        return doc;
    }
    extractString(json, "name", doc.name_);

    // Parse points
    std::size_t search = json.find("\"points\"");
    const auto surfPos = json.find("\"surfaces\"");
    while (search != std::string_view::npos && (surfPos == std::string_view::npos || search < surfPos)) {
        const auto idPos = json.find("\"id\":", search);
        if (idPos == std::string_view::npos || (surfPos != std::string_view::npos && idPos > surfPos)) {
            break;
        }
        CogoPoint pt;
        double id = 0;
        extractNumber(json.substr(idPos, 30), "id", id);
        pt.id = static_cast<PointId>(id);
        extractString(json.substr(idPos, 80), "pnum", pt.pointNumber);
        extractNumber(json.substr(idPos, 120), "e", pt.easting);
        extractNumber(json.substr(idPos, 140), "n", pt.northing);
        extractNumber(json.substr(idPos, 160), "z", pt.elevation);
        extractString(json.substr(idPos, 200), "desc", pt.rawDescription);
        doc.points_.push_back(pt);
        doc.nextPointId_ = std::max(doc.nextPointId_, pt.id + 1);
        search = idPos + 10;
    }

    return doc;
}

} // namespace mydesk::civil
