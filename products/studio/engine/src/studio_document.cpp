// SPDX-License-Identifier: MPL-2.0
#include "mydesk/studio/studio_document.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace mydesk::studio {

namespace {

constexpr double kPi = 3.14159265358979323846;

bool finitePositive(double value) noexcept {
    return std::isfinite(value) && value > 0.0;
}

bool validPrimitive(int value) noexcept {
    return value >= static_cast<int>(PrimitiveKind::Box) &&
           value <= static_cast<int>(PrimitiveKind::Sphere);
}

bool validModifier(int value) noexcept {
    return value >= static_cast<int>(ModifierKind::Bend) &&
           value <= static_cast<int>(ModifierKind::TurboSmooth);
}

} // namespace

double MeshGeometry::computeSurfaceArea() const {
    double totalArea = 0.0;
    for (const auto& tri : triangles) {
        if (tri.v0 >= vertices.size() || tri.v1 >= vertices.size() || tri.v2 >= vertices.size()) continue;
        const auto& p0 = vertices[tri.v0];
        const auto& p1 = vertices[tri.v1];
        const auto& p2 = vertices[tri.v2];

        const double ax = p1.x - p0.x, ay = p1.y - p0.y, az = p1.z - p0.z;
        const double bx = p2.x - p0.x, by = p2.y - p0.y, bz = p2.z - p0.z;

        const double cx = ay * bz - az * by;
        const double cy = az * bx - ax * bz;
        const double cz = ax * by - ay * bx;

        totalArea += 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
    }
    return totalArea;
}

std::array<double, 6> MeshGeometry::computeBoundingBox() const {
    if (vertices.empty()) return {0, 0, 0, 0, 0, 0};
    double minX = vertices[0].x, maxX = vertices[0].x;
    double minY = vertices[0].y, maxY = vertices[0].y;
    double minZ = vertices[0].z, maxZ = vertices[0].z;

    for (const auto& v : vertices) {
        minX = std::min(minX, v.x); maxX = std::max(maxX, v.x);
        minY = std::min(minY, v.y); maxY = std::max(maxY, v.y);
        minZ = std::min(minZ, v.z); maxZ = std::max(maxZ, v.z);
    }
    return {minX, minY, minZ, maxX, maxY, maxZ};
}

StudioDocument::StudioDocument() = default;

StudioDocument::Snapshot StudioDocument::capture() const {
    return {name_, objects_, materials_, nextObjId_, nextModId_};
}

void StudioDocument::restore(Snapshot snapshot) {
    name_ = std::move(snapshot.name);
    objects_ = std::move(snapshot.objects);
    materials_ = std::move(snapshot.materials);
    nextObjId_ = snapshot.nextObjId;
    nextModId_ = snapshot.nextModId;
    evaluateAll();
}

void StudioDocument::checkpoint() {
    undo_.push_back(capture());
    redo_.clear();
}

void StudioDocument::setName(std::string name) {
    if (name.empty()) throw std::invalid_argument("scene name cannot be empty");
    if (name == name_) return;
    checkpoint();
    name_ = std::move(name);
}

bool StudioDocument::undo() {
    if (undo_.empty()) return false;
    redo_.push_back(capture());
    Snapshot previous = std::move(undo_.back());
    undo_.pop_back();
    restore(std::move(previous));
    return true;
}

bool StudioDocument::redo() {
    if (redo_.empty()) return false;
    undo_.push_back(capture());
    Snapshot next = std::move(redo_.back());
    redo_.pop_back();
    restore(std::move(next));
    return true;
}

ObjectId StudioDocument::addBox(std::string name, double length, double width, double height) {
    if (name.empty() || !finitePositive(length) || !finitePositive(width) ||
        !finitePositive(height)) {
        throw std::invalid_argument("box name and dimensions must be valid");
    }
    checkpoint();
    StudioObject obj;
    obj.id = nextObjId_++;
    obj.name = std::move(name);
    obj.primitive = PrimitiveKind::Box;
    obj.primitiveParams["length"] = length;
    obj.primitiveParams["width"] = width;
    obj.primitiveParams["height"] = height;
    evaluateObject(obj);
    objects_.push_back(std::move(obj));
    return objects_.back().id;
}

ObjectId StudioDocument::addCylinder(std::string name, double radius, double height, int sides) {
    if (name.empty() || !finitePositive(radius) || !finitePositive(height) || sides < 3) {
        throw std::invalid_argument("cylinder name, dimensions, and side count must be valid");
    }
    checkpoint();
    StudioObject obj;
    obj.id = nextObjId_++;
    obj.name = std::move(name);
    obj.primitive = PrimitiveKind::Cylinder;
    obj.primitiveParams["radius"] = radius;
    obj.primitiveParams["height"] = height;
    obj.primitiveParams["sides"] = static_cast<double>(sides);
    evaluateObject(obj);
    objects_.push_back(std::move(obj));
    return objects_.back().id;
}

ObjectId StudioDocument::addSphere(std::string name, double radius, int segments) {
    if (name.empty() || !finitePositive(radius) || segments < 4) {
        throw std::invalid_argument("sphere name, radius, and segment count must be valid");
    }
    checkpoint();
    StudioObject obj;
    obj.id = nextObjId_++;
    obj.name = std::move(name);
    obj.primitive = PrimitiveKind::Sphere;
    obj.primitiveParams["radius"] = radius;
    obj.primitiveParams["segments"] = static_cast<double>(segments);
    evaluateObject(obj);
    objects_.push_back(std::move(obj));
    return objects_.back().id;
}

StudioObject* StudioDocument::findObject(ObjectId id) {
    for (auto& obj : objects_) {
        if (obj.id == id) return &obj;
    }
    return nullptr;
}

const StudioObject* StudioDocument::findObject(ObjectId id) const {
    for (const auto& obj : objects_) {
        if (obj.id == id) return &obj;
    }
    return nullptr;
}

ModifierId StudioDocument::addBendModifier(ObjectId objId, double angleDeg, double directionDeg) {
    auto* obj = findObject(objId);
    if (!obj || !std::isfinite(angleDeg) || !std::isfinite(directionDeg)) return 0;
    checkpoint();
    obj = findObject(objId);
    Modifier m;
    m.id = nextModId_++;
    m.kind = ModifierKind::Bend;
    m.name = "Bend";
    m.parameters["angle"] = angleDeg;
    m.parameters["direction"] = directionDeg;
    obj->modifierStack.push_back(m);
    evaluateObject(*obj);
    return m.id;
}

ModifierId StudioDocument::addTaperModifier(ObjectId objId, double amount, double curve) {
    auto* obj = findObject(objId);
    if (!obj || !std::isfinite(amount) || !std::isfinite(curve)) return 0;
    checkpoint();
    obj = findObject(objId);
    Modifier m;
    m.id = nextModId_++;
    m.kind = ModifierKind::Taper;
    m.name = "Taper";
    m.parameters["amount"] = amount;
    m.parameters["curve"] = curve;
    obj->modifierStack.push_back(m);
    evaluateObject(*obj);
    return m.id;
}

ModifierId StudioDocument::addTwistModifier(ObjectId objId, double angleDeg) {
    auto* obj = findObject(objId);
    if (!obj || !std::isfinite(angleDeg)) return 0;
    checkpoint();
    obj = findObject(objId);
    Modifier m;
    m.id = nextModId_++;
    m.kind = ModifierKind::Twist;
    m.name = "Twist";
    m.parameters["angle"] = angleDeg;
    obj->modifierStack.push_back(m);
    evaluateObject(*obj);
    return m.id;
}

ModifierId StudioDocument::addTurboSmoothModifier(ObjectId objId, int iterations) {
    auto* obj = findObject(objId);
    if (!obj || iterations < 1 || iterations > 4) return 0;
    checkpoint();
    obj = findObject(objId);
    Modifier m;
    m.id = nextModId_++;
    m.kind = ModifierKind::TurboSmooth;
    m.name = "TurboSmooth";
    m.parameters["iterations"] = static_cast<double>(iterations);
    obj->modifierStack.push_back(m);
    evaluateObject(*obj);
    return m.id;
}

bool StudioDocument::setModifierEnabled(ObjectId objId, ModifierId modId, bool enabled) {
    auto* obj = findObject(objId);
    if (!obj) return false;
    for (auto& mod : obj->modifierStack) {
        if (mod.id == modId) {
            if (mod.enabled == enabled) return true;
            checkpoint();
            obj = findObject(objId);
            auto found = std::find_if(obj->modifierStack.begin(), obj->modifierStack.end(),
                                      [modId](const Modifier& value) { return value.id == modId; });
            if (found == obj->modifierStack.end()) return false;
            found->enabled = enabled;
            evaluateObject(*obj);
            return true;
        }
    }
    return false;
}

bool StudioDocument::setModifierParameter(ObjectId objId, ModifierId modId,
                                          std::string parameter, double value) {
    if (parameter.empty() || !std::isfinite(value)) return false;
    auto* obj = findObject(objId);
    if (!obj) return false;
    auto found = std::find_if(obj->modifierStack.begin(), obj->modifierStack.end(),
                              [modId](const Modifier& modifier) { return modifier.id == modId; });
    if (found == obj->modifierStack.end()) return false;
    if (found->kind == ModifierKind::TurboSmooth &&
        (parameter != "iterations" || value < 1.0 || value > 4.0 || std::floor(value) != value)) {
        return false;
    }
    checkpoint();
    obj = findObject(objId);
    found = std::find_if(obj->modifierStack.begin(), obj->modifierStack.end(),
                         [modId](const Modifier& modifier) { return modifier.id == modId; });
    found->parameters[std::move(parameter)] = value;
    evaluateObject(*obj);
    return true;
}

bool StudioDocument::moveModifier(ObjectId objId, ModifierId modId, std::size_t newIndex) {
    auto* obj = findObject(objId);
    if (!obj || newIndex >= obj->modifierStack.size()) return false;
    const auto found = std::find_if(obj->modifierStack.begin(), obj->modifierStack.end(),
                                    [modId](const Modifier& modifier) { return modifier.id == modId; });
    if (found == obj->modifierStack.end()) return false;
    const std::size_t oldIndex = static_cast<std::size_t>(std::distance(obj->modifierStack.begin(), found));
    if (oldIndex == newIndex) return true;
    checkpoint();
    obj = findObject(objId);
    Modifier moved = std::move(obj->modifierStack[oldIndex]);
    obj->modifierStack.erase(obj->modifierStack.begin() + static_cast<std::ptrdiff_t>(oldIndex));
    obj->modifierStack.insert(obj->modifierStack.begin() + static_cast<std::ptrdiff_t>(newIndex),
                              std::move(moved));
    evaluateObject(*obj);
    return true;
}

bool StudioDocument::removeModifier(ObjectId objId, ModifierId modId) {
    auto* obj = findObject(objId);
    if (!obj) return false;
    const auto found = std::find_if(obj->modifierStack.begin(), obj->modifierStack.end(),
                                    [modId](const Modifier& modifier) { return modifier.id == modId; });
    if (found == obj->modifierStack.end()) return false;
    const std::size_t index = static_cast<std::size_t>(std::distance(obj->modifierStack.begin(), found));
    checkpoint();
    obj = findObject(objId);
    obj->modifierStack.erase(obj->modifierStack.begin() + static_cast<std::ptrdiff_t>(index));
    evaluateObject(*obj);
    return true;
}

bool StudioDocument::removeObject(ObjectId objId) {
    const auto found = std::find_if(objects_.begin(), objects_.end(),
                                    [objId](const StudioObject& object) { return object.id == objId; });
    if (found == objects_.end()) return false;
    const std::size_t index = static_cast<std::size_t>(std::distance(objects_.begin(), found));
    checkpoint();
    objects_.erase(objects_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool StudioDocument::setPrimitiveParameter(ObjectId objId, std::string parameter, double value) {
    if (parameter.empty() || !finitePositive(value)) return false;
    auto* obj = findObject(objId);
    if (!obj || !obj->primitiveParams.contains(parameter)) return false;
    if ((parameter == "sides" && (value < 3.0 || std::floor(value) != value)) ||
        (parameter == "segments" && (value < 4.0 || std::floor(value) != value))) {
        return false;
    }
    checkpoint();
    obj = findObject(objId);
    obj->primitiveParams[std::move(parameter)] = value;
    evaluateObject(*obj);
    return true;
}

MeshGeometry StudioDocument::generateBaseMesh(PrimitiveKind prim,
                                              const std::unordered_map<std::string, double>& params) {
    MeshGeometry mesh;
    if (prim == PrimitiveKind::Box) {
        const double l = params.count("length") ? params.at("length") : 1.0;
        const double w = params.count("width") ? params.at("width") : 1.0;
        const double h = params.count("height") ? params.at("height") : 1.0;

        // 8 box vertices centered at base
        const double hx = l * 0.5, hy = w * 0.5;
        mesh.vertices = {
            {-hx, -hy, 0.0}, { hx, -hy, 0.0}, { hx,  hy, 0.0}, {-hx,  hy, 0.0}, // bottom
            {-hx, -hy,   h}, { hx, -hy,   h}, { hx,  hy,   h}, {-hx,  hy,   h}  // top
        };
        mesh.triangles = {
            // bottom
            {0, 2, 1}, {0, 3, 2},
            // top
            {4, 5, 6}, {4, 6, 7},
            // front
            {0, 1, 5}, {0, 5, 4},
            // back
            {2, 3, 7}, {2, 7, 6},
            // left
            {3, 0, 4}, {3, 4, 7},
            // right
            {1, 2, 6}, {1, 6, 5}
        };
    } else if (prim == PrimitiveKind::Cylinder) {
        const double r = params.count("radius") ? params.at("radius") : 1.0;
        const double h = params.count("height") ? params.at("height") : 1.0;
        const int sides = params.count("sides") ? std::max(3, static_cast<int>(params.at("sides"))) : 16;

        // Bottom center (index 0), Top center (index 1)
        mesh.vertices.push_back({0.0, 0.0, 0.0});
        mesh.vertices.push_back({0.0, 0.0, h});

        const std::size_t sideCount = static_cast<std::size_t>(sides);
        for (std::size_t i = 0; i < sideCount; ++i) {
            const double theta = (2.0 * kPi * static_cast<double>(i)) /
                                 static_cast<double>(sideCount);
            const double x = r * std::cos(theta);
            const double y = r * std::sin(theta);
            mesh.vertices.push_back({x, y, 0.0}); // bottom rim (2 + 2*i)
            mesh.vertices.push_back({x, y, h});   // top rim (3 + 2*i)
        }

        for (std::size_t i = 0; i < sideCount; ++i) {
            const std::size_t nextI = (i + 1) % sideCount;
            const std::size_t bCur = 2 + 2 * i;
            const std::size_t tCur = 3 + 2 * i;
            const std::size_t bNext = 2 + 2 * nextI;
            const std::size_t tNext = 3 + 2 * nextI;

            // Bottom cap
            mesh.triangles.push_back({0, bNext, bCur});
            // Top cap
            mesh.triangles.push_back({1, tCur, tNext});
            // Side quad (two triangles)
            mesh.triangles.push_back({bCur, tNext, tCur});
            mesh.triangles.push_back({bCur, bNext, tNext});
        }
    } else if (prim == PrimitiveKind::Sphere) {
        const double r = params.at("radius");
        const int segs = std::max(4, static_cast<int>(params.at("segments")));

        // Simple latitude-longitude sphere mesh
        const std::size_t segmentCount = static_cast<std::size_t>(segs);
        for (std::size_t lat = 0; lat <= segmentCount; ++lat) {
            const double theta = (kPi * static_cast<double>(lat)) /
                                 static_cast<double>(segmentCount);
            const double sinTheta = std::sin(theta);
            const double cosTheta = std::cos(theta);

            for (std::size_t lon = 0; lon <= segmentCount; ++lon) {
                const double phi = (2.0 * kPi * static_cast<double>(lon)) /
                                   static_cast<double>(segmentCount);
                mesh.vertices.push_back({
                    r * sinTheta * std::cos(phi),
                    r * sinTheta * std::sin(phi),
                    r * cosTheta
                });
            }
        }

        for (std::size_t lat = 0; lat < segmentCount; ++lat) {
            for (std::size_t lon = 0; lon < segmentCount; ++lon) {
                const std::size_t first = lat * (segmentCount + 1) + lon;
                const std::size_t second = first + segmentCount + 1;
                mesh.triangles.push_back({first, second, first + 1});
                mesh.triangles.push_back({second, second + 1, first + 1});
            }
        }
    }
    return mesh;
}

void StudioDocument::applyModifier(MeshGeometry& mesh, const Modifier& mod) {
    if (mesh.vertices.empty()) return;

    const auto bbox = mesh.computeBoundingBox();
    const double minZ = bbox[2], maxZ = bbox[5];
    const double height = std::max(maxZ - minZ, 1e-6);

    if (mod.kind == ModifierKind::Bend) {
        const double angle = mod.parameters.count("angle") ? mod.parameters.at("angle") : 0.0;
        const double angleRad = angle * (kPi / 180.0);
        if (std::abs(angleRad) > 1e-6) {
            const double radius = height / angleRad;
            for (auto& v : mesh.vertices) {
                const double normZ = (v.z - minZ) / height;
                const double theta = normZ * angleRad;
                const double newY = v.y + (radius - radius * std::cos(theta));
                const double newZ = minZ + radius * std::sin(theta);
                v.y = newY;
                v.z = newZ;
            }
        }
    } else if (mod.kind == ModifierKind::Taper) {
        const double amount = mod.parameters.count("amount") ? mod.parameters.at("amount") : 0.0;
        for (auto& v : mesh.vertices) {
            const double normZ = (v.z - minZ) / height;
            const double factor = 1.0 + amount * normZ;
            v.x *= factor;
            v.y *= factor;
        }
    } else if (mod.kind == ModifierKind::Twist) {
        const double angle = mod.parameters.count("angle") ? mod.parameters.at("angle") : 0.0;
        const double angleRad = angle * (kPi / 180.0);
        for (auto& v : mesh.vertices) {
            const double normZ = (v.z - minZ) / height;
            const double theta = normZ * angleRad;
            const double cosT = std::cos(theta);
            const double sinT = std::sin(theta);
            const double newX = v.x * cosT - v.y * sinT;
            const double newY = v.x * sinT + v.y * cosT;
            v.x = newX;
            v.y = newY;
        }
    } else if (mod.kind == ModifierKind::TurboSmooth) {
        const int iterations = std::clamp(
            static_cast<int>(mod.parameters.count("iterations") ?
                             mod.parameters.at("iterations") : 1.0), 1, 4);
        for (int iteration = 0; iteration < iterations; ++iteration) {
            const std::vector<Triangle> originalTriangles = mesh.triangles;
            std::vector<Triangle> newTris;
            newTris.reserve(originalTriangles.size() * 4);

            for (const Triangle& tri : originalTriangles) {
                const Vec3 v0 = mesh.vertices[tri.v0];
                const Vec3 v1 = mesh.vertices[tri.v1];
                const Vec3 v2 = mesh.vertices[tri.v2];

                const std::size_t m01 = mesh.vertices.size();
                mesh.vertices.push_back({(v0.x + v1.x) * 0.5, (v0.y + v1.y) * 0.5,
                                         (v0.z + v1.z) * 0.5});
                const std::size_t m12 = mesh.vertices.size();
                mesh.vertices.push_back({(v1.x + v2.x) * 0.5, (v1.y + v2.y) * 0.5,
                                         (v1.z + v2.z) * 0.5});
                const std::size_t m20 = mesh.vertices.size();
                mesh.vertices.push_back({(v2.x + v0.x) * 0.5, (v2.y + v0.y) * 0.5,
                                         (v2.z + v0.z) * 0.5});

                newTris.push_back({tri.v0, m01, m20});
                newTris.push_back({m01, tri.v1, m12});
                newTris.push_back({m20, m12, tri.v2});
                newTris.push_back({m01, m12, m20});
            }
            mesh.triangles = std::move(newTris);
        }
    }
}

void StudioDocument::evaluateObject(StudioObject& obj) {
    MeshGeometry geom = generateBaseMesh(obj.primitive, obj.primitiveParams);
    for (const auto& mod : obj.modifierStack) {
        if (mod.enabled) {
            applyModifier(geom, mod);
        }
    }
    obj.evaluatedMesh = std::move(geom);
}

void StudioDocument::evaluateAll() {
    for (auto& obj : objects_) {
        evaluateObject(obj);
    }
}

std::size_t StudioDocument::addMaterial(PbrMaterial mat) {
    if (mat.name.empty() || !std::isfinite(mat.metallic) || !std::isfinite(mat.roughness) ||
        mat.metallic < 0.0f || mat.metallic > 1.0f ||
        mat.roughness < 0.0f || mat.roughness > 1.0f) {
        throw std::invalid_argument("material name, metallic, and roughness values are invalid");
    }
    checkpoint();
    const std::size_t idx = materials_.size();
    materials_.push_back(std::move(mat));
    return idx;
}

std::string StudioDocument::exportObj(ObjectId objId) const {
    const auto* obj = findObject(objId);
    if (!obj) return {};

    std::ostringstream ss;
    ss << "# Exported by MyDesk Studio (Autodesk 3ds Max replacement)\n";
    ss << "o " << obj->name << "\n";
    for (const auto& v : obj->evaluatedMesh.vertices) {
        ss << "v " << v.x << " " << v.y << " " << v.z << "\n";
    }
    for (const auto& tri : obj->evaluatedMesh.triangles) {
        // OBJ is 1-indexed
        ss << "f " << (tri.v0 + 1) << " " << (tri.v1 + 1) << " " << (tri.v2 + 1) << "\n";
    }
    return ss.str();
}

std::string StudioDocument::serializeNative() const {
    using nlohmann::json;
    json root = {
        {"format", "mydesk-studio"},
        {"schema_version", 2},
        {"name", name_},
        {"next_object_id", nextObjId_},
        {"next_modifier_id", nextModId_},
        {"objects", json::array()},
        {"materials", json::array()}
    };
    for (const StudioObject& object : objects_) {
        json modifiers = json::array();
        for (const Modifier& modifier : object.modifierStack) {
            modifiers.push_back({
                {"id", modifier.id},
                {"kind", static_cast<int>(modifier.kind)},
                {"name", modifier.name},
                {"enabled", modifier.enabled},
                {"parameters", modifier.parameters}
            });
        }
        root["objects"].push_back({
            {"id", object.id},
            {"name", object.name},
            {"primitive", static_cast<int>(object.primitive)},
            {"parameters", object.primitiveParams},
            {"modifiers", std::move(modifiers)}
        });
    }
    for (const PbrMaterial& material : materials_) {
        root["materials"].push_back({
            {"name", material.name},
            {"base_color", {material.baseColor[0], material.baseColor[1], material.baseColor[2]}},
            {"metallic", material.metallic},
            {"roughness", material.roughness},
            {"emissive", {material.emissive[0], material.emissive[1], material.emissive[2]}}
        });
    }
    return root.dump(2) + "\n";
}

StudioDocument StudioDocument::deserializeNative(std::string_view text, std::string* error) {
    StudioDocument doc;
    if (error) error->clear();
    try {
        using nlohmann::json;
        const json root = json::parse(text.begin(), text.end());
        if (!root.is_object() || root.value("format", std::string{}) != "mydesk-studio") {
            throw std::invalid_argument("not a mydesk-studio document");
        }
        const int schema = root.value("schema_version", 1);
        if (schema < 1 || schema > 2) throw std::invalid_argument("unsupported schema version");
        doc.name_ = root.at("name").get<std::string>();
        if (doc.name_.empty()) throw std::invalid_argument("scene name cannot be empty");

        const json& objects = root.at("objects");
        if (!objects.is_array()) throw std::invalid_argument("objects must be an array");
        ObjectId maximumObjectId = 0;
        ModifierId maximumModifierId = 0;
        for (const json& storedObject : objects) {
            StudioObject object;
            object.id = storedObject.at("id").get<ObjectId>();
            object.name = storedObject.at("name").get<std::string>();
            const int primitive = storedObject.contains("primitive") ?
                                  storedObject.at("primitive").get<int>() :
                                  storedObject.at("prim").get<int>();
            if (object.id == 0 || object.name.empty() || !validPrimitive(primitive) ||
                doc.findObject(object.id)) {
                throw std::invalid_argument("invalid or duplicate object identity");
            }
            object.primitive = static_cast<PrimitiveKind>(primitive);
            const char* parameterKey = storedObject.contains("parameters") ? "parameters" : "params";
            object.primitiveParams = storedObject.at(parameterKey)
                                         .get<std::unordered_map<std::string, double>>();
            for (const auto& [key, value] : object.primitiveParams) {
                if (key.empty() || !finitePositive(value)) {
                    throw std::invalid_argument("invalid primitive parameter");
                }
            }
            if (storedObject.contains("modifiers")) {
                for (const json& storedModifier : storedObject.at("modifiers")) {
                    Modifier modifier;
                    modifier.id = storedModifier.at("id").get<ModifierId>();
                    const int kind = storedModifier.at("kind").get<int>();
                    if (modifier.id == 0 || !validModifier(kind)) {
                        throw std::invalid_argument("invalid modifier identity or kind");
                    }
                    const auto duplicate = std::find_if(object.modifierStack.begin(),
                        object.modifierStack.end(), [&modifier](const Modifier& candidate) {
                            return candidate.id == modifier.id;
                        });
                    if (duplicate != object.modifierStack.end()) {
                        throw std::invalid_argument("duplicate modifier identity");
                    }
                    modifier.kind = static_cast<ModifierKind>(kind);
                    modifier.name = storedModifier.at("name").get<std::string>();
                    modifier.enabled = storedModifier.value("enabled", true);
                    parameterKey = storedModifier.contains("parameters") ? "parameters" : "params";
                    modifier.parameters = storedModifier.at(parameterKey)
                                              .get<std::unordered_map<std::string, double>>();
                    for (const auto& [key, value] : modifier.parameters) {
                        if (key.empty() || !std::isfinite(value)) {
                            throw std::invalid_argument("invalid modifier parameter");
                        }
                    }
                    maximumModifierId = std::max(maximumModifierId, modifier.id);
                    object.modifierStack.push_back(std::move(modifier));
                }
            }
            doc.evaluateObject(object);
            maximumObjectId = std::max(maximumObjectId, object.id);
            doc.objects_.push_back(std::move(object));
        }

        if (schema >= 2 && root.contains("materials")) {
            for (const json& storedMaterial : root.at("materials")) {
                PbrMaterial material;
                material.name = storedMaterial.at("name").get<std::string>();
                const auto base = storedMaterial.at("base_color").get<std::array<float, 3>>();
                const auto emissive = storedMaterial.at("emissive").get<std::array<float, 3>>();
                std::copy(base.begin(), base.end(), material.baseColor);
                std::copy(emissive.begin(), emissive.end(), material.emissive);
                material.metallic = storedMaterial.at("metallic").get<float>();
                material.roughness = storedMaterial.at("roughness").get<float>();
                if (material.name.empty() || !std::isfinite(material.metallic) ||
                    !std::isfinite(material.roughness) || material.metallic < 0.0f ||
                    material.metallic > 1.0f || material.roughness < 0.0f ||
                    material.roughness > 1.0f) {
                    throw std::invalid_argument("invalid material");
                }
                doc.materials_.push_back(std::move(material));
            }
        }
        doc.nextObjId_ = root.value("next_object_id", maximumObjectId + 1);
        doc.nextModId_ = root.value("next_modifier_id", maximumModifierId + 1);
        if (doc.nextObjId_ <= maximumObjectId || doc.nextModId_ <= maximumModifierId) {
            throw std::invalid_argument("stored next identity is not monotonic");
        }
    } catch (const std::exception& exception) {
        doc = StudioDocument{};
        if (error) *error = exception.what();
    }
    return doc;
}

bool StudioDocument::saveNativeFile(const std::filesystem::path& path, std::string* error) const {
    if (error) error->clear();
    if (path.empty()) {
        if (error) *error = "save path is empty";
        return false;
    }
    const std::filesystem::path temporary = path.string() + ".tmp";
    const std::filesystem::path backup = path.string() + ".bak";
    try {
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) throw std::runtime_error("cannot open temporary save file");
            output << serializeNative();
            output.flush();
            if (!output) throw std::runtime_error("cannot write temporary save file");
        }
        std::string validationError;
        (void)loadNativeFile(temporary, &validationError);
        if (!validationError.empty()) {
            throw std::runtime_error("temporary save validation failed: " + validationError);
        }
        std::error_code ec;
        if (std::filesystem::exists(path)) {
            std::filesystem::copy_file(path, backup,
                std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) throw std::runtime_error("cannot create save backup: " + ec.message());
        }
        std::filesystem::rename(temporary, path, ec);
        if (ec) {
            std::filesystem::remove(path, ec);
            ec.clear();
            std::filesystem::rename(temporary, path, ec);
        }
        if (ec) throw std::runtime_error("cannot replace project file: " + ec.message());
        return true;
    } catch (const std::exception& exception) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        if (error) *error = exception.what();
        return false;
    }
}

StudioDocument StudioDocument::loadNativeFile(const std::filesystem::path& path,
                                               std::string* error) {
    if (error) error->clear();
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        if (error) *error = "cannot open project file";
        return {};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.good() && !input.eof()) {
        if (error) *error = "cannot read project file";
        return {};
    }
    return deserializeNative(contents.str(), error);
}

} // namespace mydesk::studio
