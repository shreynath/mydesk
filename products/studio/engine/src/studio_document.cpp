// SPDX-License-Identifier: MPL-2.0
#include "mydesk/studio/studio_document.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace mydesk::studio {

namespace {

constexpr double kPi = 3.14159265358979323846;

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

ObjectId StudioDocument::addBox(std::string name, double length, double width, double height) {
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
    if (!obj) return 0;
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
    if (!obj) return 0;
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
    if (!obj) return 0;
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
    if (!obj) return 0;
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
            mod.enabled = enabled;
            evaluateObject(*obj);
            return true;
        }
    }
    return false;
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

        for (int i = 0; i < sides; ++i) {
            const double theta = (2.0 * kPi * i) / sides;
            const double x = r * std::cos(theta);
            const double y = r * std::sin(theta);
            mesh.vertices.push_back({x, y, 0.0}); // bottom rim (2 + 2*i)
            mesh.vertices.push_back({x, y, h});   // top rim (3 + 2*i)
        }

        for (int i = 0; i < sides; ++i) {
            const std::size_t nextI = (i + 1) % sides;
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
        for (int lat = 0; lat <= segs; ++lat) {
            const double theta = (kPi * lat) / segs;
            const double sinTheta = std::sin(theta);
            const double cosTheta = std::cos(theta);

            for (int lon = 0; lon <= segs; ++lon) {
                const double phi = (2.0 * kPi * lon) / segs;
                mesh.vertices.push_back({
                    r * sinTheta * std::cos(phi),
                    r * sinTheta * std::sin(phi),
                    r * cosTheta
                });
            }
        }

        for (int lat = 0; lat < segs; ++lat) {
            for (int lon = 0; lon < segs; ++lon) {
                const std::size_t first = lat * (segs + 1) + lon;
                const std::size_t second = first + segs + 1;
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
        // 1-to-4 triangle midpoint subdivision
        std::vector<Triangle> newTris;
        newTris.reserve(mesh.triangles.size() * 4);

        for (const auto& tri : mesh.triangles) {
            const auto& v0 = mesh.vertices[tri.v0];
            const auto& v1 = mesh.vertices[tri.v1];
            const auto& v2 = mesh.vertices[tri.v2];

            const std::size_t m01 = mesh.vertices.size();
            mesh.vertices.push_back({(v0.x + v1.x) * 0.5, (v0.y + v1.y) * 0.5, (v0.z + v1.z) * 0.5});
            const std::size_t m12 = mesh.vertices.size();
            mesh.vertices.push_back({(v1.x + v2.x) * 0.5, (v1.y + v2.y) * 0.5, (v1.z + v2.z) * 0.5});
            const std::size_t m20 = mesh.vertices.size();
            mesh.vertices.push_back({(v2.x + v0.x) * 0.5, (v2.y + v0.y) * 0.5, (v2.z + v0.z) * 0.5});

            newTris.push_back({tri.v0, m01, m20});
            newTris.push_back({m01, tri.v1, m12});
            newTris.push_back({m20, m12, tri.v2});
            newTris.push_back({m01, m12, m20});
        }
        mesh.triangles = std::move(newTris);
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
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"format\": \"mydesk-studio\",\n";
    ss << "  \"name\": \"" << escapeJson(name_) << "\",\n";
    ss << "  \"objects\": [\n";
    for (std::size_t i = 0; i < objects_.size(); ++i) {
        const auto& obj = objects_[i];
        ss << "    {\"id\": " << obj.id << ", \"name\": \"" << escapeJson(obj.name)
           << "\", \"prim\": " << static_cast<int>(obj.primitive) << ", \"params\": {";
        bool first = true;
        for (const auto& [k, v] : obj.primitiveParams) {
            if (!first) ss << ", ";
            first = false;
            ss << "\"" << escapeJson(k) << "\": " << v;
        }
        ss << "}, \"modifiers\": [";
        for (std::size_t m = 0; m < obj.modifierStack.size(); ++m) {
            const auto& mod = obj.modifierStack[m];
            ss << "{\"id\": " << mod.id << ", \"kind\": " << static_cast<int>(mod.kind)
               << ", \"name\": \"" << escapeJson(mod.name) << "\", \"enabled\": "
               << (mod.enabled ? "true" : "false") << ", \"params\": {";
            bool pFirst = true;
            for (const auto& [pk, pv] : mod.parameters) {
                if (!pFirst) ss << ", ";
                pFirst = false;
                ss << "\"" << escapeJson(pk) << "\": " << pv;
            }
            ss << "}}";
            if (m + 1 < obj.modifierStack.size()) ss << ", ";
        }
        ss << "]}";
        if (i + 1 < objects_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n}\n";
    return ss.str();
}

StudioDocument StudioDocument::deserializeNative(std::string_view json, std::string* error) {
    StudioDocument doc;
    std::string format;
    if (!extractString(json, "format", format) || format != "mydesk-studio") {
        if (error) *error = "not a mydesk-studio document";
        return doc;
    }
    extractString(json, "name", doc.name_);

    std::size_t search = json.find("\"objects\"");
    while (search != std::string_view::npos) {
        const auto idPos = json.find("\"id\":", search);
        if (idPos == std::string_view::npos) break;
        StudioObject obj;
        double id = 0, prim = 0;
        extractNumber(json.substr(idPos, 30), "id", id);
        obj.id = static_cast<ObjectId>(id);
        extractString(json.substr(idPos, 120), "name", obj.name);
        const auto primPos = json.find("\"prim\":", idPos);
        if (primPos != std::string_view::npos) {
            extractNumber(json.substr(primPos, 30), "prim", prim);
            obj.primitive = static_cast<PrimitiveKind>(prim);
        }

        const auto paramsPos = json.find("\"params\":", idPos);
        if (paramsPos != std::string_view::npos) {
            const auto lb = json.find('{', paramsPos);
            const auto rb = json.find('}', lb);
            if (lb != std::string_view::npos && rb != std::string_view::npos) {
                std::string body(json.substr(lb + 1, rb - lb - 1));
                std::stringstream ss(body);
                std::string token;
                while (std::getline(ss, token, ',')) {
                    const auto q1 = token.find('"');
                    const auto q2 = token.find('"', q1 + 1);
                    const auto colon = token.find(':');
                    if (q1 != std::string_view::npos && q2 != std::string_view::npos && colon != std::string_view::npos) {
                        const std::string key = token.substr(q1 + 1, q2 - q1 - 1);
                        const double val = std::stod(token.substr(colon + 1));
                        obj.primitiveParams[key] = val;
                    }
                }
            }
        }

        // Parse modifiers array if present
        const auto modPos = json.find("\"modifiers\":", idPos);
        const auto objEnd = json.find("]}", idPos);
        if (modPos != std::string_view::npos && objEnd != std::string_view::npos && modPos < objEnd) {
            std::size_t mSearch = modPos;
            while (true) {
                const auto mIdPos = json.find("\"id\":", mSearch);
                if (mIdPos == std::string_view::npos || mIdPos >= objEnd) break;
                Modifier mod;
                double mId = 0, mKind = 0;
                extractNumber(json.substr(mIdPos, 30), "id", mId);
                mod.id = static_cast<ModifierId>(mId);
                const auto mKindPos = json.find("\"kind\":", mIdPos);
                if (mKindPos != std::string_view::npos && mKindPos < objEnd) {
                    extractNumber(json.substr(mKindPos, 30), "kind", mKind);
                    mod.kind = static_cast<ModifierKind>(mKind);
                }
                extractString(json.substr(mIdPos, 120), "name", mod.name);
                const auto mParamPos = json.find("\"params\":", mIdPos);
                if (mParamPos != std::string_view::npos && mParamPos < objEnd) {
                    const auto mlb = json.find('{', mParamPos);
                    const auto mrb = json.find('}', mlb);
                    if (mlb != std::string_view::npos && mrb != std::string_view::npos && mrb < objEnd) {
                        std::string mBody(json.substr(mlb + 1, mrb - mlb - 1));
                        std::stringstream mss(mBody);
                        std::string mToken;
                        while (std::getline(mss, mToken, ',')) {
                            const auto mq1 = mToken.find('"');
                            const auto mq2 = mToken.find('"', mq1 + 1);
                            const auto mcol = mToken.find(':');
                            if (mq1 != std::string_view::npos && mq2 != std::string_view::npos && mcol != std::string_view::npos) {
                                const std::string mkey = mToken.substr(mq1 + 1, mq2 - mq1 - 1);
                                const double mval = std::stod(mToken.substr(mcol + 1));
                                mod.parameters[mkey] = mval;
                            }
                        }
                    }
                }
                obj.modifierStack.push_back(mod);
                mSearch = mIdPos + 10;
            }
        }

        doc.evaluateObject(obj);
        doc.objects_.push_back(std::move(obj));

        if (objEnd != std::string_view::npos) {
            search = objEnd + 2;
        } else {
            search = idPos + 10;
        }
    }

    return doc;
}

} // namespace mydesk::studio
