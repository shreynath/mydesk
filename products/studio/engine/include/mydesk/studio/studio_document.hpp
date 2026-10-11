// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mydesk::studio {

using ObjectId = std::uint64_t;
using ModifierId = std::uint64_t;

struct Vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

struct Triangle {
    std::size_t v0{0};
    std::size_t v1{0};
    std::size_t v2{0};
};

struct MeshGeometry {
    std::vector<Vec3> vertices;
    std::vector<Triangle> triangles;

    [[nodiscard]] double computeSurfaceArea() const;
    [[nodiscard]] std::array<double, 6> computeBoundingBox() const; // minX, minY, minZ, maxX, maxY, maxZ
};

enum class PrimitiveKind {
    Box,
    Cylinder,
    Sphere
};

enum class ModifierKind {
    Bend,
    Taper,
    Twist,
    TurboSmooth
};

struct Modifier {
    ModifierId id{0};
    ModifierKind kind{ModifierKind::Bend};
    std::string name;
    bool enabled{true};
    std::unordered_map<std::string, double> parameters;
};

struct StudioObject {
    ObjectId id{0};
    std::string name;
    PrimitiveKind primitive{PrimitiveKind::Box};
    std::unordered_map<std::string, double> primitiveParams;
    std::vector<Modifier> modifierStack;
    MeshGeometry evaluatedMesh;
};

struct PbrMaterial {
    std::string name;
    float baseColor[3]{0.8f, 0.8f, 0.8f};
    float metallic{0.0f};
    float roughness{0.5f};
    float emissive[3]{0.0f, 0.0f, 0.0f};
};

class StudioDocument {
public:
    StudioDocument();

    [[nodiscard]] const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    // Primitive creation
    ObjectId addBox(std::string name, double length, double width, double height);
    ObjectId addCylinder(std::string name, double radius, double height, int sides = 16);
    ObjectId addSphere(std::string name, double radius, int segments = 12);

    [[nodiscard]] const std::vector<StudioObject>& objects() const { return objects_; }
    [[nodiscard]] StudioObject* findObject(ObjectId id);
    [[nodiscard]] const StudioObject* findObject(ObjectId id) const;

    // Modifier stack operations
    ModifierId addBendModifier(ObjectId objId, double angleDeg, double directionDeg);
    ModifierId addTaperModifier(ObjectId objId, double amount, double curve);
    ModifierId addTwistModifier(ObjectId objId, double angleDeg);
    ModifierId addTurboSmoothModifier(ObjectId objId, int iterations = 1);
    bool setModifierEnabled(ObjectId objId, ModifierId modId, bool enabled);

    // Evaluation
    void evaluateObject(StudioObject& obj);
    void evaluateAll();

    // Materials
    std::size_t addMaterial(PbrMaterial mat);
    [[nodiscard]] const std::vector<PbrMaterial>& materials() const { return materials_; }

    // Interoperability
    [[nodiscard]] std::string exportObj(ObjectId objId) const;

    // Native persistence
    [[nodiscard]] std::string serializeNative() const;
    static StudioDocument deserializeNative(std::string_view json, std::string* error = nullptr);

private:
    std::string name_{"Untitled_Studio"};
    std::vector<StudioObject> objects_;
    std::vector<PbrMaterial> materials_;
    ObjectId nextObjId_{1};
    ModifierId nextModId_{1};

    static MeshGeometry generateBaseMesh(PrimitiveKind prim, const std::unordered_map<std::string, double>& params);
    static void applyModifier(MeshGeometry& mesh, const Modifier& mod);
};

} // namespace mydesk::studio
