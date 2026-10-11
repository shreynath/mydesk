#include "mydesk/coordinate/coordinate_document.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace mydesk::coordinate {

namespace {

constexpr double kGeometryEpsilon = 1.0e-10;
constexpr double kParallelAxisEpsilon = 1.0e-8;

Vector3D add(Vector3D a, Vector3D b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vector3D subtract(Vector3D a, Vector3D b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vector3D multiply(Vector3D v, double scale) { return {v.x * scale, v.y * scale, v.z * scale}; }
double dot(Vector3D a, Vector3D b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vector3D cross(Vector3D a, Vector3D b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}
double lengthSquared(Vector3D v) { return dot(v, v); }
double length(Vector3D v) { return std::sqrt(lengthSquared(v)); }
Vector3D normalized(Vector3D v) {
    const double len = length(v);
    return len > kGeometryEpsilon ? multiply(v, 1.0 / len) : Vector3D{};
}

OrientedBox3D asOrientedBox(const CoordinationElement& element) {
    if (element.orientedBounds) {
        return *element.orientedBounds;
    }
    const auto [cx, cy, cz] = element.bounds.center();
    return {{cx, cy, cz},
            {(element.bounds.maxX - element.bounds.minX) * 0.5,
             (element.bounds.maxY - element.bounds.minY) * 0.5,
             (element.bounds.maxZ - element.bounds.minZ) * 0.5}};
}

double projectionRadius(const OrientedBox3D& box, Vector3D axis) {
    return box.halfExtents.x * std::abs(dot(box.axes[0], axis)) +
           box.halfExtents.y * std::abs(dot(box.axes[1], axis)) +
           box.halfExtents.z * std::abs(dot(box.axes[2], axis));
}

Vector3D supportPoint(const OrientedBox3D& box, Vector3D direction) {
    Vector3D point = box.center;
    const std::array<double, 3> extent{box.halfExtents.x, box.halfExtents.y,
                                       box.halfExtents.z};
    for (std::size_t i = 0; i < 3; ++i) {
        point = add(point, multiply(box.axes[i], dot(box.axes[i], direction) >= 0.0
                                                       ? extent[i]
                                                       : -extent[i]));
    }
    return point;
}

struct SatResult {
    bool intersects{true};
    double penetration{std::numeric_limits<double>::infinity()};
    Vector3D axis{1.0, 0.0, 0.0};
};

SatResult intersectObb(const OrientedBox3D& a, const OrientedBox3D& b) {
    SatResult result;
    const Vector3D centerDelta = subtract(b.center, a.center);
    std::array<Vector3D, 15> axes{};
    axes[0] = a.axes[0];
    axes[1] = a.axes[1];
    axes[2] = a.axes[2];
    axes[3] = b.axes[0];
    axes[4] = b.axes[1];
    axes[5] = b.axes[2];
    std::size_t index = 6;
    for (const auto& axisA : a.axes) {
        for (const auto& axisB : b.axes) {
            axes[index++] = cross(axisA, axisB);
        }
    }

    for (auto axis : axes) {
        if (lengthSquared(axis) <= kParallelAxisEpsilon * kParallelAxisEpsilon) {
            continue;
        }
        axis = normalized(axis);
        const double signedCenterDistance = dot(centerDelta, axis);
        const double overlap = projectionRadius(a, axis) + projectionRadius(b, axis) -
                               std::abs(signedCenterDistance);
        if (overlap < -kGeometryEpsilon) {
            result.intersects = false;
            result.penetration = 0.0;
            return result;
        }
        if (overlap < result.penetration) {
            result.penetration = std::max(0.0, overlap);
            result.axis = signedCenterDistance < 0.0 ? multiply(axis, -1.0) : axis;
        }
    }
    return result;
}

Vector3D closestPointOnObb(Vector3D point, const OrientedBox3D& box) {
    Vector3D closest = box.center;
    const Vector3D delta = subtract(point, box.center);
    const std::array<double, 3> extent{box.halfExtents.x, box.halfExtents.y,
                                       box.halfExtents.z};
    for (std::size_t i = 0; i < 3; ++i) {
        const double coordinate = std::clamp(dot(delta, box.axes[i]), -extent[i], extent[i]);
        closest = add(closest, multiply(box.axes[i], coordinate));
    }
    return closest;
}

std::array<Vector3D, 8> vertices(const OrientedBox3D& box) {
    std::array<Vector3D, 8> result{};
    std::size_t index = 0;
    for (double sx : {-1.0, 1.0}) {
        for (double sy : {-1.0, 1.0}) {
            for (double sz : {-1.0, 1.0}) {
                result[index++] = add(add(add(box.center,
                                                  multiply(box.axes[0], sx * box.halfExtents.x)),
                                              multiply(box.axes[1], sy * box.halfExtents.y)),
                                          multiply(box.axes[2], sz * box.halfExtents.z));
            }
        }
    }
    return result;
}

struct Segment {
    Vector3D begin;
    Vector3D end;
};

std::array<Segment, 12> edges(const std::array<Vector3D, 8>& v) {
    // Vertex index bits correspond to X, Y, Z signs in vertices().
    return {{{v[0], v[4]}, {v[1], v[5]}, {v[2], v[6]}, {v[3], v[7]},
             {v[0], v[2]}, {v[1], v[3]}, {v[4], v[6]}, {v[5], v[7]},
             {v[0], v[1]}, {v[2], v[3]}, {v[4], v[5]}, {v[6], v[7]}}};
}

double segmentDistanceSquared(const Segment& first, const Segment& second, Vector3D& pointA,
                              Vector3D& pointB) {
    const Vector3D d1 = subtract(first.end, first.begin);
    const Vector3D d2 = subtract(second.end, second.begin);
    const Vector3D r = subtract(first.begin, second.begin);
    const double a = dot(d1, d1);
    const double e = dot(d2, d2);
    const double f = dot(d2, r);
    double s = 0.0;
    double t = 0.0;

    if (a <= kGeometryEpsilon && e <= kGeometryEpsilon) {
        pointA = first.begin;
        pointB = second.begin;
        return lengthSquared(subtract(pointA, pointB));
    }
    if (a <= kGeometryEpsilon) {
        t = std::clamp(f / e, 0.0, 1.0);
    } else {
        const double c = dot(d1, r);
        if (e <= kGeometryEpsilon) {
            s = std::clamp(-c / a, 0.0, 1.0);
        } else {
            const double b = dot(d1, d2);
            const double denominator = a * e - b * b;
            if (std::abs(denominator) > kGeometryEpsilon) {
                s = std::clamp((b * f - c * e) / denominator, 0.0, 1.0);
            }
            t = (b * s + f) / e;
            if (t < 0.0) {
                t = 0.0;
                s = std::clamp(-c / a, 0.0, 1.0);
            } else if (t > 1.0) {
                t = 1.0;
                s = std::clamp((b - c) / a, 0.0, 1.0);
            }
        }
    }
    pointA = add(first.begin, multiply(d1, s));
    pointB = add(second.begin, multiply(d2, t));
    return lengthSquared(subtract(pointA, pointB));
}

struct DistanceResult {
    double distance{0.0};
    Vector3D pointA;
    Vector3D pointB;
};

DistanceResult distanceBetweenObbs(const OrientedBox3D& a, const OrientedBox3D& b) {
    const auto sat = intersectObb(a, b);
    if (sat.intersects) {
        const auto pointA = supportPoint(a, sat.axis);
        const auto pointB = supportPoint(b, multiply(sat.axis, -1.0));
        return {0.0, pointA, pointB};
    }

    DistanceResult best;
    double bestSquared = std::numeric_limits<double>::infinity();
    const auto verticesA = vertices(a);
    const auto verticesB = vertices(b);
    for (const auto& vertex : verticesA) {
        const auto closest = closestPointOnObb(vertex, b);
        const double distanceSquared = lengthSquared(subtract(vertex, closest));
        if (distanceSquared < bestSquared) {
            bestSquared = distanceSquared;
            best.pointA = vertex;
            best.pointB = closest;
        }
    }
    for (const auto& vertex : verticesB) {
        const auto closest = closestPointOnObb(vertex, a);
        const double distanceSquared = lengthSquared(subtract(vertex, closest));
        if (distanceSquared < bestSquared) {
            bestSquared = distanceSquared;
            best.pointA = closest;
            best.pointB = vertex;
        }
    }
    for (const auto& edgeA : edges(verticesA)) {
        for (const auto& edgeB : edges(verticesB)) {
            Vector3D pointA;
            Vector3D pointB;
            const double distanceSquared = segmentDistanceSquared(edgeA, edgeB, pointA, pointB);
            if (distanceSquared < bestSquared) {
                bestSquared = distanceSquared;
                best.pointA = pointA;
                best.pointB = pointB;
            }
        }
    }
    best.distance = std::sqrt(std::max(0.0, bestSquared));
    return best;
}

bool validBounds(const BoundingBox3D& bounds) {
    return std::isfinite(bounds.minX) && std::isfinite(bounds.minY) &&
           std::isfinite(bounds.minZ) && std::isfinite(bounds.maxX) &&
           std::isfinite(bounds.maxY) && std::isfinite(bounds.maxZ) &&
           bounds.minX <= bounds.maxX && bounds.minY <= bounds.maxY &&
           bounds.minZ <= bounds.maxZ;
}

std::string escapeJson(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out.push_back(c); break;
        }
    }
    return out;
}

std::string escapeXml(std::string_view text) {
    std::string escaped;
    for (const char c : text) {
        switch (c) {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '"': escaped += "&quot;"; break;
        case '\'': escaped += "&apos;"; break;
        default: escaped.push_back(c); break;
        }
    }
    return escaped;
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
    out.clear();
    bool escaped = false;
    for (std::size_t i = q1 + 1; i < json.size(); ++i) {
        const char c = json[i];
        if (escaped) {
            if (c == 'n') {
                out.push_back('\n');
            } else if (c == 'r') {
                out.push_back('\r');
            } else if (c == 't') {
                out.push_back('\t');
            } else {
                out.push_back(c);
            }
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            return true;
        } else {
            out.push_back(c);
        }
    }
    return false;
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

std::optional<std::string_view> arrayForKey(std::string_view json, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\"";
    const auto keyPosition = json.find(needle);
    if (keyPosition == std::string_view::npos) {
        return std::nullopt;
    }
    const auto begin = json.find('[', keyPosition + needle.size());
    if (begin == std::string_view::npos) {
        return std::nullopt;
    }
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (std::size_t i = begin; i < json.size(); ++i) {
        const char c = json[i];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        if (c == '"') {
            inString = true;
        } else if (c == '[') {
            ++depth;
        } else if (c == ']' && --depth == 0) {
            return json.substr(begin + 1, i - begin - 1);
        }
    }
    return std::nullopt;
}

std::vector<std::string_view> objectsInArray(std::string_view array) {
    std::vector<std::string_view> objects;
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    std::size_t begin = std::string_view::npos;
    for (std::size_t i = 0; i < array.size(); ++i) {
        const char c = array[i];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        if (c == '"') {
            inString = true;
        } else if (c == '{') {
            if (depth++ == 0) {
                begin = i;
            }
        } else if (c == '}' && --depth == 0 && begin != std::string_view::npos) {
            objects.push_back(array.substr(begin, i - begin + 1));
            begin = std::string_view::npos;
        }
    }
    return objects;
}

std::vector<double> numbersInArray(std::string_view array) {
    std::vector<double> numbers;
    std::size_t position = 0;
    while (position < array.size()) {
        while (position < array.size() &&
               (std::isspace(static_cast<unsigned char>(array[position])) ||
                array[position] == ',')) {
            ++position;
        }
        if (position >= array.size()) {
            break;
        }
        const char* begin = array.data() + position;
        char* end = nullptr;
        const double value = std::strtod(begin, &end);
        if (end == begin || !std::isfinite(value)) {
            break;
        }
        numbers.push_back(value);
        position = static_cast<std::size_t>(end - array.data());
    }
    return numbers;
}

std::uint64_t extractId(std::string_view object, std::string_view key) {
    double value = 0.0;
    return extractNumber(object, key, value) && value >= 0.0
               ? static_cast<std::uint64_t>(value)
               : 0;
}

} // namespace

OrientedBox3D OrientedBox3D::fromEuler(Vector3D center, Vector3D halfExtents,
                                        double yawRadians, double pitchRadians,
                                        double rollRadians) {
    const double cy = std::cos(yawRadians);
    const double sy = std::sin(yawRadians);
    const double cp = std::cos(pitchRadians);
    const double sp = std::sin(pitchRadians);
    const double cr = std::cos(rollRadians);
    const double sr = std::sin(rollRadians);

    // Rz(yaw) * Ry(pitch) * Rx(roll), stored as the three local basis columns.
    OrientedBox3D box;
    box.center = center;
    box.halfExtents = halfExtents;
    box.axes = {{{cy * cp, sy * cp, -sp},
                 {cy * sp * sr - sy * cr, sy * sp * sr + cy * cr, cp * sr},
                 {cy * sp * cr + sy * sr, sy * sp * cr - cy * sr, cp * cr}}};
    return box;
}

BoundingBox3D OrientedBox3D::axisAlignedBounds() const {
    const double ex = halfExtents.x * std::abs(axes[0].x) +
                      halfExtents.y * std::abs(axes[1].x) +
                      halfExtents.z * std::abs(axes[2].x);
    const double ey = halfExtents.x * std::abs(axes[0].y) +
                      halfExtents.y * std::abs(axes[1].y) +
                      halfExtents.z * std::abs(axes[2].y);
    const double ez = halfExtents.x * std::abs(axes[0].z) +
                      halfExtents.y * std::abs(axes[1].z) +
                      halfExtents.z * std::abs(axes[2].z);
    return {center.x - ex, center.y - ey, center.z - ez,
            center.x + ex, center.y + ey, center.z + ez};
}

bool OrientedBox3D::valid() const {
    const bool finiteCenter = std::isfinite(center.x) && std::isfinite(center.y) &&
                              std::isfinite(center.z);
    const bool positiveExtents = std::isfinite(halfExtents.x) &&
                                 std::isfinite(halfExtents.y) &&
                                 std::isfinite(halfExtents.z) && halfExtents.x > 0.0 &&
                                 halfExtents.y > 0.0 && halfExtents.z > 0.0;
    if (!finiteCenter || !positiveExtents) {
        return false;
    }
    for (const auto& axis : axes) {
        if (!std::isfinite(axis.x) || !std::isfinite(axis.y) || !std::isfinite(axis.z) ||
            std::abs(lengthSquared(axis) - 1.0) > 1.0e-8) {
            return false;
        }
    }
    return std::abs(dot(axes[0], axes[1])) <= 1.0e-8 &&
           std::abs(dot(axes[0], axes[2])) <= 1.0e-8 &&
           std::abs(dot(axes[1], axes[2])) <= 1.0e-8;
}

bool BoundingBox3D::intersects(const BoundingBox3D& other) const {
    return (minX <= other.maxX && maxX >= other.minX) &&
           (minY <= other.maxY && maxY >= other.minY) &&
           (minZ <= other.maxZ && maxZ >= other.minZ);
}

bool BoundingBox3D::withinDistance(const BoundingBox3D& other, double clearance) const {
    const double dx = std::max(0.0, std::max(minX - other.maxX, other.minX - maxX));
    const double dy = std::max(0.0, std::max(minY - other.maxY, other.minY - maxY));
    const double dz = std::max(0.0, std::max(minZ - other.maxZ, other.minZ - maxZ));
    return std::sqrt(dx * dx + dy * dy + dz * dz) <= clearance;
}

double BoundingBox3D::penetrationDepth(const BoundingBox3D& other) const {
    if (!intersects(other)) {
        return 0.0;
    }
    const double ox = std::min(maxX, other.maxX) - std::max(minX, other.minX);
    const double oy = std::min(maxY, other.maxY) - std::max(minY, other.minY);
    const double oz = std::min(maxZ, other.maxZ) - std::max(minZ, other.minZ);
    return std::min({ox, oy, oz});
}

std::tuple<double, double, double> BoundingBox3D::center() const {
    return {(minX + maxX) * 0.5, (minY + maxY) * 0.5, (minZ + maxZ) * 0.5};
}

CoordinateDocument::CoordinateDocument() = default;

ModelId CoordinateDocument::addModel(std::string name, std::string sourcePath,
                                     std::string discipline) {
    AggregatedModel m;
    m.id = nextModelId_++;
    m.name = std::move(name);
    m.sourcePath = std::move(sourcePath);
    m.discipline = std::move(discipline);
    models_.push_back(std::move(m));
    return models_.back().id;
}

ElementId CoordinateDocument::addElementToModel(ModelId modelId, std::string name,
                                                std::string category, const BoundingBox3D& bounds) {
    if (!validBounds(bounds)) {
        return 0;
    }
    for (auto& m : models_) {
        if (m.id == modelId) {
            CoordinationElement el;
            el.id = nextElementId_++;
            el.modelId = modelId;
            el.name = std::move(name);
            el.category = std::move(category);
            el.bounds = bounds;
            m.elements.push_back(el);
            return el.id;
        }
    }
    return 0;
}

ElementId CoordinateDocument::addOrientedElementToModel(ModelId modelId, std::string name,
                                                        std::string category,
                                                        const OrientedBox3D& bounds) {
    if (!bounds.valid()) {
        return 0;
    }
    for (auto& model : models_) {
        if (model.id == modelId) {
            CoordinationElement element;
            element.id = nextElementId_++;
            element.modelId = modelId;
            element.name = std::move(name);
            element.category = std::move(category);
            element.bounds = bounds.axisAlignedBounds();
            element.orientedBounds = bounds;
            model.elements.push_back(std::move(element));
            return model.elements.back().id;
        }
    }
    return 0;
}

const AggregatedModel* CoordinateDocument::findModel(ModelId id) const {
    for (const auto& m : models_) {
        if (m.id == id) {
            return &m;
        }
    }
    return nullptr;
}

const CoordinationElement* CoordinateDocument::findElement(ElementId id) const {
    for (const auto& m : models_) {
        for (const auto& el : m.elements) {
            if (el.id == id) {
                return &el;
            }
        }
    }
    return nullptr;
}

ClashTestId CoordinateDocument::createClashTest(std::string name, ClashType type, double tolerance,
                                                double clearance) {
    ClashTest t;
    t.id = nextTestId_++;
    t.name = std::move(name);
    t.type = type;
    t.tolerance = std::max(0.0, tolerance);
    t.clearance = std::max(0.0, clearance);
    clashTests_.push_back(std::move(t));
    return clashTests_.back().id;
}

bool CoordinateDocument::setClashTestSets(ClashTestId testId, std::vector<ModelId> setA,
                                          std::vector<ModelId> setB) {
    for (auto& t : clashTests_) {
        if (t.id == testId) {
            t.selectionSetA = std::move(setA);
            t.selectionSetB = std::move(setB);
            return true;
        }
    }
    return false;
}

std::size_t CoordinateDocument::runClashTest(ClashTestId testId) {
    ClashTest* test = nullptr;
    for (auto& t : clashTests_) {
        if (t.id == testId) {
            test = &t;
            break;
        }
    }
    if (!test) {
        return 0;
    }

    using PairKey = std::pair<ElementId, ElementId>;
    std::map<PairKey, ClashResult> previousResults;
    for (const auto& result : test->results) {
        previousResults[{std::min(result.elementA, result.elementB),
                         std::max(result.elementA, result.elementB)}] = result;
    }
    test->results.clear();
    test->lastRun = {};

    auto gatherElements = [this](const std::vector<ModelId>& modelIds) {
        std::vector<const CoordinationElement*> elements;
        std::set<ElementId> seen;
        for (const auto modelId : modelIds) {
            if (const auto* model = findModel(modelId)) {
                for (const auto& element : model->elements) {
                    if (seen.insert(element.id).second) {
                        elements.push_back(&element);
                    }
                }
            }
        }
        return elements;
    };

    auto elementsA = gatherElements(test->selectionSetA);
    const auto elementsB = gatherElements(test->selectionSetB);
    std::sort(elementsA.begin(), elementsA.end(), [](const auto* left, const auto* right) {
        return left->bounds.minX < right->bounds.minX;
    });

    const double expansion = test->type == ClashType::Clearance ? test->clearance : 0.0;
    std::set<PairKey> rawPairs;
    for (const auto* elementA : elementsA) {
        for (const auto* elementB : elementsB) {
            if (elementA->id != elementB->id) {
                rawPairs.insert({std::min(elementA->id, elementB->id),
                                 std::max(elementA->id, elementB->id)});
            }
        }
    }
    test->lastRun.rawPairCount = rawPairs.size();
    std::set<PairKey> visitedPairs;
    for (const auto* elementB : elementsB) {
        for (const auto* elementA : elementsA) {
            if (elementA->bounds.minX > elementB->bounds.maxX + expansion) {
                break; // X-axis sweep-and-prune: all remaining A elements are farther away.
            }
            if (elementA->id == elementB->id) {
                continue;
            }
            const PairKey pair{std::min(elementA->id, elementB->id),
                               std::max(elementA->id, elementB->id)};
            if (!visitedPairs.insert(pair).second) {
                continue;
            }
            if (elementA->bounds.maxX < elementB->bounds.minX - expansion ||
                elementA->bounds.minY > elementB->bounds.maxY + expansion ||
                elementA->bounds.maxY < elementB->bounds.minY - expansion ||
                elementA->bounds.minZ > elementB->bounds.maxZ + expansion ||
                elementA->bounds.maxZ < elementB->bounds.minZ - expansion) {
                continue;
            }
            ++test->lastRun.broadPhaseCandidates;
            ++test->lastRun.narrowPhaseChecks;

            const auto orientedA = asOrientedBox(*elementA);
            const auto orientedB = asOrientedBox(*elementB);
            const auto sat = intersectObb(orientedA, orientedB);
            bool isClash = false;
            double measurement = 0.0;
            Vector3D pointA;
            Vector3D pointB;
            if (test->type == ClashType::Hard) {
                measurement = (!elementA->orientedBounds && !elementB->orientedBounds)
                                  ? elementA->bounds.penetrationDepth(elementB->bounds)
                                  : sat.penetration;
                isClash = sat.intersects && measurement + kGeometryEpsilon >= test->tolerance;
                pointA = closestPointOnObb(orientedB.center, orientedA);
                pointB = closestPointOnObb(orientedA.center, orientedB);
            } else {
                const auto distance = distanceBetweenObbs(orientedA, orientedB);
                isClash = distance.distance <= test->clearance + kGeometryEpsilon;
                measurement = distance.distance;
                pointA = distance.pointA;
                pointB = distance.pointB;
            }
            if (!isClash) {
                continue;
            }

            ClashResult result;
            if (const auto old = previousResults.find(pair); old != previousResults.end()) {
                result = old->second; // Keep review decisions stable when a test is rerun.
            } else {
                result.id = nextResultId_++;
                result.testId = testId;
                result.type = test->type;
                result.status = ClashStatus::New;
                result.elementA = elementA->id;
                result.elementB = elementB->id;
                result.comments = test->type == ClashType::Hard
                                      ? "Hard clash detected between " + elementA->name + " and " +
                                            elementB->name
                                      : "Clearance violation between " + elementA->name + " and " +
                                            elementB->name;
            }
            result.penetrationOrDistance = measurement;
            const Vector3D clashPoint = multiply(add(pointA, pointB), 0.5);
            result.clashX = clashPoint.x;
            result.clashY = clashPoint.y;
            result.clashZ = clashPoint.z;
            test->results.push_back(std::move(result));
        }
    }
    return test->results.size();
}

bool CoordinateDocument::setClashResultReview(ClashTestId testId, ClashResultId resultId,
                                              ClashStatus status, std::string comments) {
    for (auto& test : clashTests_) {
        if (test.id != testId) {
            continue;
        }
        for (auto& result : test.results) {
            if (result.id == resultId) {
                if (result.status == status && result.comments == comments) {
                    return true;
                }
                undoHistory_.push_back({testId, resultId, result.status, status, result.comments,
                                        comments});
                redoHistory_.clear();
                result.status = status;
                result.comments = std::move(comments);
                return true;
            }
        }
    }
    return false;
}

bool CoordinateDocument::undo() {
    if (undoHistory_.empty()) {
        return false;
    }
    const auto change = undoHistory_.back();
    undoHistory_.pop_back();
    for (auto& test : clashTests_) {
        for (auto& result : test.results) {
            if (test.id == change.testId && result.id == change.resultId) {
                result.status = change.beforeStatus;
                result.comments = change.beforeComments;
                redoHistory_.push_back(change);
                return true;
            }
        }
    }
    return false;
}

bool CoordinateDocument::redo() {
    if (redoHistory_.empty()) {
        return false;
    }
    const auto change = redoHistory_.back();
    redoHistory_.pop_back();
    for (auto& test : clashTests_) {
        for (auto& result : test.results) {
            if (test.id == change.testId && result.id == change.resultId) {
                result.status = change.afterStatus;
                result.comments = change.afterComments;
                undoHistory_.push_back(change);
                return true;
            }
        }
    }
    return false;
}

const ClashTest* CoordinateDocument::findClashTest(ClashTestId testId) const {
    for (const auto& t : clashTests_) {
        if (t.id == testId) {
            return &t;
        }
    }
    return nullptr;
}

std::string CoordinateDocument::exportBcfXml() const {
    std::ostringstream xml;
    xml << std::setprecision(std::numeric_limits<double>::max_digits10);
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<VisualizationInfo xmlns=\"http://www.buildingsmart-tech.org/specifications/bcf-releases\">\n";
    xml << "  <Components>\n";
    for (const auto& t : clashTests_) {
        for (const auto& r : t.results) {
            xml << "    <Component IfcGuid=\"EL_" << r.elementA << "\">\n";
            xml << "      <Origin X=\"" << r.clashX << "\" Y=\"" << r.clashY << "\" Z=\"" << r.clashZ << "\"/>\n";
            xml << "      <Status>" << (r.status == ClashStatus::New ? "New" : "Active") << "</Status>\n";
            xml << "      <Comment>" << escapeXml(r.comments) << "</Comment>\n";
            xml << "    </Component>\n";
        }
    }
    xml << "  </Components>\n";
    xml << "</VisualizationInfo>\n";
    return xml.str();
}

std::string CoordinateDocument::serializeNative() const {
    std::ostringstream oss;
    oss << std::setprecision(std::numeric_limits<double>::max_digits10);
    oss << "{\n";
    oss << "  \"format\": \"mydesk-coordinate\", \"version\": 2,\n";
    oss << "  \"name\": \"" << escapeJson(name_) << "\",\n";
    oss << "  \"models\": [\n";
    for (std::size_t i = 0; i < models_.size(); ++i) {
        const auto& m = models_[i];
        oss << "    {\"id\": " << m.id << ", \"name\": \"" << escapeJson(m.name)
            << "\", \"sourcePath\": \"" << escapeJson(m.sourcePath)
            << "\", \"discipline\": \"" << escapeJson(m.discipline) << "\", \"elements\": [\n";
        for (std::size_t e = 0; e < m.elements.size(); ++e) {
            const auto& el = m.elements[e];
            oss << "      {\"id\": " << el.id << ", \"name\": \"" << escapeJson(el.name)
                << "\", \"cat\": \"" << escapeJson(el.category)
                << "\", \"bounds\": [" << el.bounds.minX << ", " << el.bounds.minY << ", " << el.bounds.minZ
                << ", " << el.bounds.maxX << ", " << el.bounds.maxY << ", " << el.bounds.maxZ << "]";
            if (el.orientedBounds) {
                const auto& box = *el.orientedBounds;
                oss << ", \"oriented\": [" << box.center.x << ", " << box.center.y << ", "
                    << box.center.z << ", " << box.halfExtents.x << ", " << box.halfExtents.y
                    << ", " << box.halfExtents.z;
                for (const auto& axis : box.axes) {
                    oss << ", " << axis.x << ", " << axis.y << ", " << axis.z;
                }
                oss << "]";
            }
            oss << "}";
            if (e + 1 < m.elements.size()) {
                oss << ",";
            }
            oss << "\n";
        }
        oss << "    ]}";
        if (i + 1 < models_.size()) {
            oss << ",";
        }
        oss << "\n";
    }
    oss << "  ],\n";
    oss << "  \"clashTests\": [\n";
    for (std::size_t i = 0; i < clashTests_.size(); ++i) {
        const auto& test = clashTests_[i];
        oss << "    {\"id\": " << test.id << ", \"name\": \"" << escapeJson(test.name)
            << "\", \"type\": " << static_cast<int>(test.type) << ", \"tolerance\": "
            << test.tolerance << ", \"clearance\": " << test.clearance << ", \"setA\": [";
        for (std::size_t index = 0; index < test.selectionSetA.size(); ++index) {
            if (index) oss << ", ";
            oss << test.selectionSetA[index];
        }
        oss << "], \"setB\": [";
        for (std::size_t index = 0; index < test.selectionSetB.size(); ++index) {
            if (index) oss << ", ";
            oss << test.selectionSetB[index];
        }
        oss << "], \"results\": [\n";
        for (std::size_t index = 0; index < test.results.size(); ++index) {
            const auto& result = test.results[index];
            oss << "      {\"id\": " << result.id << ", \"type\": "
                << static_cast<int>(result.type) << ", \"status\": "
                << static_cast<int>(result.status) << ", \"elementA\": " << result.elementA
                << ", \"elementB\": " << result.elementB << ", \"measurement\": "
                << result.penetrationOrDistance << ", \"point\": [" << result.clashX << ", "
                << result.clashY << ", " << result.clashZ << "], \"comments\": \""
                << escapeJson(result.comments) << "\"}";
            if (index + 1 < test.results.size()) oss << ',';
            oss << '\n';
        }
        oss << "    ]}";
        if (i + 1 < clashTests_.size()) oss << ',';
        oss << '\n';
    }
    oss << "  ]\n}\n";
    return oss.str();
}

CoordinateDocument CoordinateDocument::deserializeNative(std::string_view json, std::string* error) {
    CoordinateDocument doc;
    auto fail = [&](std::string message) {
        if (error) {
            *error = std::move(message);
        }
        return CoordinateDocument{};
    };
    std::string format;
    if (!extractString(json, "format", format) || format != "mydesk-coordinate") {
        return fail("not a mydesk-coordinate document");
    }
    extractString(json, "name", doc.name_);

    const auto modelArray = arrayForKey(json, "models");
    if (!modelArray) {
        return fail("missing models array");
    }
    for (const auto modelObject : objectsInArray(*modelArray)) {
        AggregatedModel m;
        m.id = extractId(modelObject, "id");
        extractString(modelObject, "name", m.name);
        extractString(modelObject, "sourcePath", m.sourcePath);
        extractString(modelObject, "discipline", m.discipline);
        if (m.id == 0) {
            return fail("model has invalid id");
        }
        if (const auto elementArray = arrayForKey(modelObject, "elements")) {
            for (const auto elementObject : objectsInArray(*elementArray)) {
                CoordinationElement element;
                element.id = extractId(elementObject, "id");
                element.modelId = m.id;
                extractString(elementObject, "name", element.name);
                extractString(elementObject, "cat", element.category);
                const auto boundsArray = arrayForKey(elementObject, "bounds");
                if (!boundsArray) {
                    return fail("element is missing bounds");
                }
                const auto values = numbersInArray(*boundsArray);
                if (element.id == 0 || values.size() != 6) {
                    return fail("element has invalid id or bounds");
                }
                element.bounds = {values[0], values[1], values[2], values[3], values[4], values[5]};
                if (!validBounds(element.bounds)) {
                    return fail("element bounds are invalid");
                }
                if (const auto orientedArray = arrayForKey(elementObject, "oriented")) {
                    const auto oriented = numbersInArray(*orientedArray);
                    if (oriented.size() != 15) {
                        return fail("oriented box must contain 15 numbers");
                    }
                    OrientedBox3D box;
                    box.center = {oriented[0], oriented[1], oriented[2]};
                    box.halfExtents = {oriented[3], oriented[4], oriented[5]};
                    box.axes = {{{oriented[6], oriented[7], oriented[8]},
                                 {oriented[9], oriented[10], oriented[11]},
                                 {oriented[12], oriented[13], oriented[14]}}};
                    if (!box.valid()) {
                        return fail("oriented box basis is invalid");
                    }
                    element.orientedBounds = box;
                    element.bounds = box.axisAlignedBounds();
                }
                doc.nextElementId_ = std::max(doc.nextElementId_, element.id + 1);
                m.elements.push_back(std::move(element));
            }
        }
        doc.nextModelId_ = std::max(doc.nextModelId_, m.id + 1);
        doc.models_.push_back(std::move(m));
    }

    if (const auto testArray = arrayForKey(json, "clashTests")) {
        for (const auto testObject : objectsInArray(*testArray)) {
            ClashTest test;
            test.id = extractId(testObject, "id");
            extractString(testObject, "name", test.name);
            double type = 0.0;
            extractNumber(testObject, "type", type);
            test.type = type == 1.0 ? ClashType::Clearance : ClashType::Hard;
            extractNumber(testObject, "tolerance", test.tolerance);
            extractNumber(testObject, "clearance", test.clearance);
            if (test.id == 0 || test.tolerance < 0.0 || test.clearance < 0.0) {
                return fail("clash test parameters are invalid");
            }
            if (const auto setA = arrayForKey(testObject, "setA")) {
                for (const auto value : numbersInArray(*setA)) {
                    test.selectionSetA.push_back(static_cast<ModelId>(value));
                }
            }
            if (const auto setB = arrayForKey(testObject, "setB")) {
                for (const auto value : numbersInArray(*setB)) {
                    test.selectionSetB.push_back(static_cast<ModelId>(value));
                }
            }
            if (const auto resultArray = arrayForKey(testObject, "results")) {
                for (const auto resultObject : objectsInArray(*resultArray)) {
                    ClashResult result;
                    result.id = extractId(resultObject, "id");
                    result.testId = test.id;
                    double resultType = 0.0;
                    double status = 0.0;
                    extractNumber(resultObject, "type", resultType);
                    extractNumber(resultObject, "status", status);
                    result.type = resultType == 1.0 ? ClashType::Clearance : ClashType::Hard;
                    if (status < 0.0 || status > static_cast<double>(ClashStatus::Resolved)) {
                        return fail("clash result status is invalid");
                    }
                    result.status = static_cast<ClashStatus>(static_cast<int>(status));
                    result.elementA = extractId(resultObject, "elementA");
                    result.elementB = extractId(resultObject, "elementB");
                    extractNumber(resultObject, "measurement", result.penetrationOrDistance);
                    extractString(resultObject, "comments", result.comments);
                    const auto pointArray = arrayForKey(resultObject, "point");
                    const auto point = pointArray ? numbersInArray(*pointArray) : std::vector<double>{};
                    if (result.id == 0 || result.elementA == 0 || result.elementB == 0 ||
                        point.size() != 3) {
                        return fail("clash result is incomplete");
                    }
                    result.clashX = point[0];
                    result.clashY = point[1];
                    result.clashZ = point[2];
                    doc.nextResultId_ = std::max(doc.nextResultId_, result.id + 1);
                    test.results.push_back(std::move(result));
                }
            }
            doc.nextTestId_ = std::max(doc.nextTestId_, test.id + 1);
            doc.clashTests_.push_back(std::move(test));
        }
    }
    if (error) {
        error->clear();
    }
    return doc;
}

} // namespace mydesk::coordinate
