#include "mydesk/civil/civil_document.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace mydesk::civil {

namespace {

using Json = nlohmann::json;

std::string escapeXml(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&apos;"; break;
        default: out.push_back(c); break;
        }
    }
    return out;
}

struct DelaunayVertex {
    double x{0.0};
    double y{0.0};
    PointId id{0};
};

struct DelaunayTriangle {
    std::size_t a{0};
    std::size_t b{0};
    std::size_t c{0};
};

bool circumcircleContains(const DelaunayVertex& a, const DelaunayVertex& b,
                          const DelaunayVertex& c, const DelaunayVertex& point) {
    const double denominator = 2.0 *
        (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
    if (std::abs(denominator) < 1e-14) {
        return false;
    }
    const double aa = a.x * a.x + a.y * a.y;
    const double bb = b.x * b.x + b.y * b.y;
    const double cc = c.x * c.x + c.y * c.y;
    const double ux = (aa * (b.y - c.y) + bb * (c.y - a.y) +
                       cc * (a.y - b.y)) / denominator;
    const double uy = (aa * (c.x - b.x) + bb * (a.x - c.x) +
                       cc * (b.x - a.x)) / denominator;
    const double radiusSquared = (ux - a.x) * (ux - a.x) + (uy - a.y) * (uy - a.y);
    const double distanceSquared = (ux - point.x) * (ux - point.x) +
                                   (uy - point.y) * (uy - point.y);
    return distanceSquared <= radiusSquared + std::max(1.0, radiusSquared) * 1e-11;
}

std::vector<Triangle3D> triangulate(const std::vector<CogoPoint>& points) {
    std::vector<Triangle3D> result;
    if (points.size() < 3) {
        return result;
    }

    double minX = points.front().easting;
    double maxX = minX;
    double minY = points.front().northing;
    double maxY = minY;
    std::vector<DelaunayVertex> vertices;
    vertices.reserve(points.size() + 3);
    for (const auto& point : points) {
        minX = std::min(minX, point.easting);
        maxX = std::max(maxX, point.easting);
        minY = std::min(minY, point.northing);
        maxY = std::max(maxY, point.northing);
        vertices.push_back({point.easting, point.northing, point.id});
    }

    const double extent = std::max(maxX - minX, maxY - minY);
    if (extent < 1e-12) {
        return result;
    }
    const double midX = (minX + maxX) * 0.5;
    const double midY = (minY + maxY) * 0.5;
    const std::size_t realCount = vertices.size();
    vertices.push_back({midX - 32.0 * extent, midY - extent, 0});
    vertices.push_back({midX, midY + 32.0 * extent, 0});
    vertices.push_back({midX + 32.0 * extent, midY - extent, 0});

    std::vector<DelaunayTriangle> triangles{{realCount, realCount + 1, realCount + 2}};
    for (std::size_t pointIndex = 0; pointIndex < realCount; ++pointIndex) {
        std::vector<bool> bad(triangles.size(), false);
        std::map<std::pair<std::size_t, std::size_t>, int> edgeUse;
        for (std::size_t triangleIndex = 0; triangleIndex < triangles.size(); ++triangleIndex) {
            const auto& triangle = triangles[triangleIndex];
            if (!circumcircleContains(vertices[triangle.a], vertices[triangle.b],
                                      vertices[triangle.c], vertices[pointIndex])) {
                continue;
            }
            bad[triangleIndex] = true;
            const std::pair<std::size_t, std::size_t> edges[] = {
                std::minmax(triangle.a, triangle.b), std::minmax(triangle.b, triangle.c),
                std::minmax(triangle.c, triangle.a)};
            for (const auto& edge : edges) {
                ++edgeUse[edge];
            }
        }

        std::vector<DelaunayTriangle> kept;
        kept.reserve(triangles.size() + edgeUse.size());
        for (std::size_t i = 0; i < triangles.size(); ++i) {
            if (!bad[i]) {
                kept.push_back(triangles[i]);
            }
        }
        for (const auto& [edge, count] : edgeUse) {
            if (count == 1) {
                kept.push_back({edge.first, edge.second, pointIndex});
            }
        }
        triangles = std::move(kept);
    }

    for (const auto& triangle : triangles) {
        if (triangle.a >= realCount || triangle.b >= realCount || triangle.c >= realCount) {
            continue;
        }
        const auto& a = vertices[triangle.a];
        const auto& b = vertices[triangle.b];
        const auto& c = vertices[triangle.c];
        const double twiceArea = (b.x - a.x) * (c.y - a.y) -
                                 (b.y - a.y) * (c.x - a.x);
        if (std::abs(twiceArea) > 1e-12) {
            result.push_back({a.id, b.id, c.id});
        }
    }
    return result;
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
            double angleSpan = seg.endAngle - seg.startAngle;
            constexpr double twoPi = 6.28318530717958647692;
            if (seg.isClockwise) {
                while (angleSpan > 0.0) {
                    angleSpan -= twoPi;
                }
            } else {
                while (angleSpan < 0.0) {
                    angleSpan += twoPi;
                }
            }
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

bool CivilDocument::updatePoint(PointId id, std::string pointNum, double easting,
                                double northing, double elevation, std::string desc) {
    auto point = std::find_if(points_.begin(), points_.end(),
                              [id](const CogoPoint& candidate) { return candidate.id == id; });
    if (point == points_.end() || !std::isfinite(easting) || !std::isfinite(northing) ||
        !std::isfinite(elevation) || pointNum.empty()) {
        return false;
    }
    point->pointNumber = std::move(pointNum);
    point->easting = easting;
    point->northing = northing;
    point->elevation = elevation;
    point->rawDescription = std::move(desc);
    for (auto& surface : surfaces_) {
        auto surfacePoint = std::find_if(surface.points.begin(), surface.points.end(),
            [id](const CogoPoint& candidate) { return candidate.id == id; });
        if (surfacePoint != surface.points.end()) {
            *surfacePoint = *point;
            surface.triangles = triangulate(surface.points);
        }
    }
    return true;
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

    surf.triangles = triangulate(surf.points);

    surfaces_.push_back(std::move(surf));
    return surfaces_.back().id;
}

bool CivilDocument::renameSurface(SurfaceId id, std::string name) {
    for (auto& surface : surfaces_) {
        if (surface.id == id && !name.empty()) {
            surface.name = std::move(name);
            return true;
        }
    }
    return false;
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
    if (!(radius > 0.0) || !std::isfinite(radius)) {
        return false;
    }
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
            double span = endAngle - startAngle;
            constexpr double twoPi = 6.28318530717958647692;
            if (clockwise) {
                while (span > 0.0) {
                    span -= twoPi;
                }
            } else {
                while (span < 0.0) {
                    span += twoPi;
                }
            }
            seg.length = radius * std::abs(span);
            al.segments.push_back(seg);
            return true;
        }
    }
    return false;
}

bool CivilDocument::updateAlignment(AlignmentId id, std::string name, double startStation) {
    for (auto& alignment : alignments_) {
        if (alignment.id == id && !name.empty() && std::isfinite(startStation)) {
            alignment.name = std::move(name);
            alignment.startStation = startStation;
            double station = startStation;
            for (auto& segment : alignment.segments) {
                segment.startStation = station;
                station += segment.length;
            }
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

bool CivilDocument::renameProfile(ProfileId id, std::string name) {
    for (auto& profile : profiles_) {
        if (profile.id == id && !name.empty()) {
            profile.name = std::move(name);
            return true;
        }
    }
    return false;
}

std::string CivilDocument::exportLandXml() const {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<LandXML xmlns=\"http://www.landxml.org/schema/LandXML-1.2\" version=\"1.2\" date=\"2026-10-10\">\n";
    xml << "  <Project name=\"" << escapeXml(name_) << "\"/>\n";

    // CgPoints
    xml << "  <CgPoints>\n";
    for (const auto& pt : points_) {
        xml << "    <CgPoint name=\"" << escapeXml(pt.pointNumber)
            << "\" desc=\"" << escapeXml(pt.rawDescription) << "\">"
            << pt.northing << " " << pt.easting << " " << pt.elevation << "</CgPoint>\n";
    }
    xml << "  </CgPoints>\n";

    // Surfaces
    if (!surfaces_.empty()) {
        xml << "  <Surfaces>\n";
        for (const auto& surf : surfaces_) {
            xml << "    <Surface name=\"" << escapeXml(surf.name) << "\">\n";
            xml << "      <Definition surfType=\"TIN\">\n";
            xml << "        <Pnts>\n";
            for (const auto& point : surf.points) {
                xml << "          <P id=\"" << point.id << "\">" << point.northing << " "
                    << point.easting << " " << point.elevation << "</P>\n";
            }
            xml << "        </Pnts>\n";
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
            xml << "    <Alignment name=\"" << escapeXml(al.name) << "\" staStart=\""
                << al.startStation << "\" length=\"" << al.totalLength() << "\">\n";
            xml << "      <CoordGeom>\n";
            for (const auto& seg : al.segments) {
                if (seg.kind == AlignmentSegment::Kind::Tangent) {
                    xml << "        <Line length=\"" << seg.length << "\">\n";
                    xml << "          <Start>" << seg.startY << " " << seg.startX << "</Start>\n";
                    xml << "          <End>" << seg.endY << " " << seg.endX << "</End>\n";
                    xml << "        </Line>\n";
                } else {
                    xml << "        <Curve rot=\"" << (seg.isClockwise ? "cw" : "ccw")
                        << "\" radius=\"" << seg.radius << "\" length=\"" << seg.length
                        << "\">\n";
                    xml << "          <Center>" << seg.centerY << " " << seg.centerX
                        << "</Center>\n";
                    xml << "        </Curve>\n";
                }
            }
            xml << "      </CoordGeom>\n";
            xml << "    </Alignment>\n";
        }
        xml << "  </Alignments>\n";
    }

    if (!profiles_.empty()) {
        xml << "  <ProfAlignments>\n";
        for (const auto& profile : profiles_) {
            xml << "    <ProfAlignment name=\"" << escapeXml(profile.name)
                << "\" alignmentRef=\"" << profile.alignmentId << "\">\n";
            xml << "      <Profile>\n";
            for (const auto& pvi : profile.pvis) {
                xml << "        <PVI sta=\"" << pvi.station << "\" elev=\"" << pvi.elevation
                    << "\" curveLength=\"" << pvi.curveLength << "\"/>\n";
            }
            xml << "      </Profile>\n";
            xml << "    </ProfAlignment>\n";
        }
        xml << "  </ProfAlignments>\n";
    }

    xml << "</LandXML>\n";
    return xml.str();
}

std::string CivilDocument::serializeNative() const {
    Json root{{"format", "mydesk-civil"}, {"version", 2}, {"name", name_}};
    root["points"] = Json::array();
    for (const auto& point : points_) {
        root["points"].push_back({{"id", point.id}, {"number", point.pointNumber},
            {"easting", point.easting}, {"northing", point.northing},
            {"elevation", point.elevation}, {"description", point.rawDescription}});
    }
    root["surfaces"] = Json::array();
    for (const auto& surface : surfaces_) {
        Json pointIds = Json::array();
        for (const auto& point : surface.points) {
            pointIds.push_back(point.id);
        }
        Json triangles = Json::array();
        for (const auto& triangle : surface.triangles) {
            triangles.push_back({triangle.p0, triangle.p1, triangle.p2});
        }
        root["surfaces"].push_back({{"id", surface.id}, {"name", surface.name},
                                    {"point_ids", pointIds}, {"triangles", triangles}});
    }
    root["alignments"] = Json::array();
    for (const auto& alignment : alignments_) {
        Json segments = Json::array();
        for (const auto& segment : alignment.segments) {
            segments.push_back({{"kind", segment.kind == AlignmentSegment::Kind::Tangent
                                             ? "tangent" : "curve"},
                {"start_station", segment.startStation}, {"length", segment.length},
                {"start_x", segment.startX}, {"start_y", segment.startY},
                {"end_x", segment.endX}, {"end_y", segment.endY},
                {"center_x", segment.centerX}, {"center_y", segment.centerY},
                {"radius", segment.radius}, {"start_angle", segment.startAngle},
                {"end_angle", segment.endAngle}, {"clockwise", segment.isClockwise}});
        }
        root["alignments"].push_back({{"id", alignment.id}, {"name", alignment.name},
                                       {"start_station", alignment.startStation},
                                       {"segments", segments}});
    }
    root["profiles"] = Json::array();
    for (const auto& profile : profiles_) {
        Json pvis = Json::array();
        for (const auto& pvi : profile.pvis) {
            pvis.push_back({{"station", pvi.station}, {"elevation", pvi.elevation},
                            {"curve_length", pvi.curveLength}});
        }
        root["profiles"].push_back({{"id", profile.id}, {"name", profile.name},
                                     {"alignment_id", profile.alignmentId}, {"pvis", pvis}});
    }
    return root.dump(2) + "\n";
}

CivilDocument CivilDocument::deserializeNative(std::string_view source, std::string* error) {
    CivilDocument doc;
    try {
        const Json root = Json::parse(source);
        if (root.value("format", std::string{}) != "mydesk-civil") {
            throw std::runtime_error("not a mydesk-civil document");
        }
        doc.name_ = root.value("name", std::string{"CivilProject1"});
        for (const auto& value : root.value("points", Json::array())) {
            CogoPoint point;
            point.id = value.at("id").get<PointId>();
            point.pointNumber = value.value("number", value.value("pnum", std::string{}));
            point.easting = value.value("easting", value.value("e", 0.0));
            point.northing = value.value("northing", value.value("n", 0.0));
            point.elevation = value.value("elevation", value.value("z", 0.0));
            point.rawDescription = value.value("description", value.value("desc", std::string{}));
            doc.points_.push_back(point);
            doc.nextPointId_ = std::max(doc.nextPointId_, point.id + 1);
        }
        for (const auto& value : root.value("surfaces", Json::array())) {
            TinSurface surface;
            surface.id = value.at("id").get<SurfaceId>();
            surface.name = value.value("name", std::string{"Surface"});
            for (const auto& pointIdValue : value.value("point_ids", Json::array())) {
                const auto pointId = pointIdValue.get<PointId>();
                if (const auto* point = doc.findPoint(pointId)) {
                    surface.points.push_back(*point);
                }
            }
            for (const auto& triangle : value.value("triangles", Json::array())) {
                if (triangle.is_array() && triangle.size() == 3) {
                    surface.triangles.push_back({triangle[0].get<PointId>(),
                        triangle[1].get<PointId>(), triangle[2].get<PointId>()});
                }
            }
            if (surface.triangles.empty()) {
                surface.triangles = triangulate(surface.points);
            }
            doc.surfaces_.push_back(std::move(surface));
            doc.nextSurfaceId_ = std::max(doc.nextSurfaceId_, doc.surfaces_.back().id + 1);
        }
        for (const auto& value : root.value("alignments", Json::array())) {
            Alignment alignment;
            alignment.id = value.at("id").get<AlignmentId>();
            alignment.name = value.value("name", std::string{"Alignment"});
            alignment.startStation = value.value("start_station", 0.0);
            for (const auto& segmentValue : value.value("segments", Json::array())) {
                AlignmentSegment segment;
                segment.kind = segmentValue.value("kind", std::string{"tangent"}) == "curve"
                                   ? AlignmentSegment::Kind::Curve : AlignmentSegment::Kind::Tangent;
                segment.startStation = segmentValue.value("start_station", 0.0);
                segment.length = segmentValue.value("length", 0.0);
                segment.startX = segmentValue.value("start_x", 0.0);
                segment.startY = segmentValue.value("start_y", 0.0);
                segment.endX = segmentValue.value("end_x", 0.0);
                segment.endY = segmentValue.value("end_y", 0.0);
                segment.centerX = segmentValue.value("center_x", 0.0);
                segment.centerY = segmentValue.value("center_y", 0.0);
                segment.radius = segmentValue.value("radius", 0.0);
                segment.startAngle = segmentValue.value("start_angle", 0.0);
                segment.endAngle = segmentValue.value("end_angle", 0.0);
                segment.isClockwise = segmentValue.value("clockwise", false);
                alignment.segments.push_back(segment);
            }
            doc.alignments_.push_back(std::move(alignment));
            doc.nextAlignmentId_ = std::max(doc.nextAlignmentId_, doc.alignments_.back().id + 1);
        }
        for (const auto& value : root.value("profiles", Json::array())) {
            Profile profile;
            profile.id = value.at("id").get<ProfileId>();
            profile.name = value.value("name", std::string{"Profile"});
            profile.alignmentId = value.value("alignment_id", AlignmentId{0});
            for (const auto& pviValue : value.value("pvis", Json::array())) {
                profile.pvis.push_back({pviValue.value("station", 0.0),
                    pviValue.value("elevation", 0.0), pviValue.value("curve_length", 0.0)});
            }
            doc.profiles_.push_back(std::move(profile));
            doc.nextProfileId_ = std::max(doc.nextProfileId_, doc.profiles_.back().id + 1);
        }
        if (error) {
            error->clear();
        }
    } catch (const std::exception& exception) {
        if (error) {
            *error = exception.what();
        }
        return CivilDocument{};
    }
    return doc;
}

} // namespace mydesk::civil
