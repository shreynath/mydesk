// SPDX-License-Identifier: MPL-2.0
// MyDesk Studio — Regression & Verification Tests
#include "mydesk/studio/studio_document.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

using namespace mydesk::studio;

static int g_failed = 0;
#define CHECK(cond) \
    do { if (!(cond)) { \
        std::fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, #cond); \
        ++g_failed; \
    } } while(0)

#define CHECK_NEAR(a, b, eps) \
    do { double _a = (a), _b = (b), _e = (eps); \
         if (std::abs(_a - _b) > _e) { \
             std::fprintf(stderr, "FAIL [line %d]: |%.6g - %.6g| > %.6g\n", \
                          __LINE__, _a, _b, _e); \
             ++g_failed; \
    } } while(0)

static void test_studio_box_primitive() {
    StudioDocument doc;
    const auto id = doc.addBox("Box1", 10.0, 20.0, 30.0);
    const auto* obj = doc.findObject(id);
    CHECK(obj != nullptr);
    CHECK(obj->evaluatedMesh.vertices.size() == 8);
    CHECK(obj->evaluatedMesh.triangles.size() == 12);
    // Box surface area: 2*(10*20 + 10*30 + 20*30) = 2*(200 + 300 + 600) = 2200
    CHECK_NEAR(obj->evaluatedMesh.computeSurfaceArea(), 2200.0, 1e-4);
}

static void test_studio_cylinder_primitive() {
    StudioDocument doc;
    const auto id = doc.addCylinder("Cyl1", 5.0, 20.0, 16);
    const auto* obj = doc.findObject(id);
    CHECK(obj != nullptr);
    CHECK(obj->evaluatedMesh.vertices.size() == 2 + 16 * 2);
    CHECK(obj->evaluatedMesh.triangles.size() == 16 * 4); // 16 bottom + 16 top + 32 sides
    const auto bbox = obj->evaluatedMesh.computeBoundingBox();
    CHECK_NEAR(bbox[5] - bbox[2], 20.0, 1e-6); // height
}

static void test_studio_bend_modifier() {
    StudioDocument doc;
    const auto id = doc.addBox("Column", 2.0, 2.0, 10.0);
    const auto modId = doc.addBendModifier(id, 90.0, 0.0);
    CHECK(modId > 0);

    const auto* obj = doc.findObject(id);
    CHECK(obj->modifierStack.size() == 1);
    const auto bbox = obj->evaluatedMesh.computeBoundingBox();
    // After 90 deg bend along Y, max Y coordinate increases significantly
    CHECK(bbox[4] > 2.0);
}

static void test_studio_taper_modifier() {
    StudioDocument doc;
    const auto id = doc.addBox("ConeFrustum", 10.0, 10.0, 10.0);
    doc.addTaperModifier(id, -0.5, 0.0); // top shrinks by 50%

    const auto* obj = doc.findObject(id);
    const auto bbox = obj->evaluatedMesh.computeBoundingBox();
    CHECK(bbox[3] <= 5.0); // max X at top is smaller
}

static void test_studio_twist_modifier() {
    StudioDocument doc;
    const auto id = doc.addBox("TwistedBeam", 4.0, 4.0, 20.0);
    doc.addTwistModifier(id, 180.0);

    const auto* obj = doc.findObject(id);
    CHECK(!obj->evaluatedMesh.vertices.empty());
    CHECK(obj->evaluatedMesh.computeSurfaceArea() > 0.0);
}

static void test_studio_turbosmooth_subdivision() {
    StudioDocument doc;
    const auto id = doc.addBox("SubdividedBox", 10.0, 10.0, 10.0);
    const auto* obj = doc.findObject(id);
    const auto baseTris = obj->evaluatedMesh.triangles.size();
    CHECK(baseTris == 12);

    doc.addTurboSmoothModifier(id, 2);
    CHECK(obj->evaluatedMesh.triangles.size() == baseTris * 16);
}

static void test_studio_obj_export() {
    StudioDocument doc;
    const auto id = doc.addBox("ExportMesh", 2.0, 2.0, 2.0);
    const auto objStr = doc.exportObj(id);
    CHECK(objStr.find("o ExportMesh") != std::string::npos);
    CHECK(objStr.find("v ") != std::string::npos);
    CHECK(objStr.find("f ") != std::string::npos);
}

static void test_studio_native_roundtrip() {
    StudioDocument doc;
    doc.setName("StudioScene");
    const auto id = doc.addBox("BoxA", 5.0, 5.0, 15.0);
    doc.addTwistModifier(id, 45.0);

    const auto json = doc.serializeNative();
    CHECK(!json.empty());

    std::string err;
    StudioDocument loaded = StudioDocument::deserializeNative(json, &err);
    CHECK(err.empty());
    CHECK(loaded.name() == "StudioScene");
    CHECK(loaded.objects().size() == 1);
    CHECK(loaded.objects().front().name == "BoxA");
}

static void test_modifier_stack_transactions() {
    StudioDocument doc;
    const ObjectId id = doc.addBox("Transactional", 4.0, 6.0, 12.0);
    const ModifierId twist = doc.addTwistModifier(id, 30.0);
    const ModifierId taper = doc.addTaperModifier(id, 0.25, 0.0);
    CHECK(doc.moveModifier(id, taper, 0));
    CHECK(doc.findObject(id)->modifierStack[0].id == taper);
    CHECK(doc.findObject(id)->modifierStack[1].id == twist);
    CHECK(doc.setModifierParameter(id, twist, "angle", 90.0));
    CHECK_NEAR(doc.findObject(id)->modifierStack[1].parameters.at("angle"), 90.0, 1e-9);
    CHECK(doc.undo());
    CHECK_NEAR(doc.findObject(id)->modifierStack[1].parameters.at("angle"), 30.0, 1e-9);
    CHECK(doc.undo());
    CHECK(doc.findObject(id)->modifierStack[0].id == twist);
    CHECK(doc.redo());
    CHECK(doc.findObject(id)->modifierStack[0].id == taper);
    CHECK(doc.removeModifier(id, twist));
    CHECK(doc.findObject(id)->modifierStack.size() == 1);
    CHECK(doc.undo());
    CHECK(doc.findObject(id)->modifierStack.size() == 2);
    CHECK(doc.setPrimitiveParameter(id, "height", 20.0));
    CHECK_NEAR(doc.findObject(id)->primitiveParams.at("height"), 20.0, 1e-9);
}

static void test_schema_v2_materials_and_atomic_file_roundtrip() {
    StudioDocument doc;
    doc.setName("Scene \\\"A\\\"");
    const ObjectId box = doc.addBox("Box \\\"one\\\"", 2.0, 3.0, 4.0);
    const ObjectId sphere = doc.addSphere("Sphere", 5.0, 8);
    const ModifierId smooth = doc.addTurboSmoothModifier(sphere, 2);
    CHECK(doc.setModifierEnabled(sphere, smooth, false));
    PbrMaterial material;
    material.name = "Paint \\\"red\\\"";
    material.baseColor[0] = 0.8f;
    material.baseColor[1] = 0.1f;
    material.baseColor[2] = 0.05f;
    material.metallic = 0.2f;
    material.roughness = 0.35f;
    doc.addMaterial(material);

    const std::string native = doc.serializeNative();
    CHECK(native.find("\"schema_version\": 2") != std::string::npos);
    std::string error;
    StudioDocument loaded = StudioDocument::deserializeNative(native, &error);
    CHECK(error.empty());
    CHECK(loaded.objects().size() == 2);
    CHECK(loaded.materials().size() == 1);
    CHECK(loaded.findObject(box) != nullptr);
    CHECK(loaded.findObject(sphere)->modifierStack.size() == 1);
    CHECK(!loaded.findObject(sphere)->modifierStack.front().enabled);
    CHECK(loaded.materials().front().name == material.name);
    CHECK_NEAR(loaded.materials().front().roughness, 0.35, 1e-6);
    const ObjectId next = loaded.addCylinder("AfterLoad", 1.0, 2.0, 12);
    CHECK(next > sphere);

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("mydesk-studio-" + std::to_string(stamp) + ".mystudio");
    CHECK(doc.saveNativeFile(path, &error));
    CHECK(error.empty());
    StudioDocument fromFile = StudioDocument::loadNativeFile(path, &error);
    CHECK(error.empty());
    CHECK(fromFile.objects().size() == 2);
    doc.addBox("BackupTrigger", 1.0, 1.0, 1.0);
    CHECK(doc.saveNativeFile(path, &error));
    CHECK(std::filesystem::exists(path.string() + ".bak"));
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(path.string() + ".bak", ignored);
}

static void test_native_rejects_malformed_and_unknown_schema() {
    std::string error;
    (void)StudioDocument::deserializeNative("{not-json", &error);
    CHECK(!error.empty());
    StudioDocument migrated = StudioDocument::deserializeNative(
        R"({"format":"mydesk-studio","name":"legacy","objects":[{"id":4,"name":"LegacyBox","prim":0,"params":{"length":2,"width":3,"height":4},"modifiers":[{"id":7,"kind":2,"name":"Twist","enabled":true,"params":{"angle":45}}]}]})",
        &error);
    CHECK(error.empty());
    CHECK(migrated.objects().size() == 1);
    CHECK(migrated.findObject(4) != nullptr);
    CHECK(migrated.findObject(4)->modifierStack.front().id == 7);
    CHECK(migrated.addSphere("AfterMigration", 1.0, 8) > 4);
    (void)StudioDocument::deserializeNative(
        R"({"format":"mydesk-studio","schema_version":99,"name":"x","objects":[]})", &error);
    CHECK(error.find("unsupported") != std::string::npos);
}

int main() {
    // Registered capability test identifiers:
    // "test_studio_box_primitive"
    test_studio_box_primitive();
    // "test_studio_cylinder_primitive"
    test_studio_cylinder_primitive();
    // "test_studio_bend_modifier"
    test_studio_bend_modifier();
    // "test_studio_taper_modifier"
    test_studio_taper_modifier();
    // "test_studio_twist_modifier"
    test_studio_twist_modifier();
    // "test_studio_turbosmooth_subdivision"
    test_studio_turbosmooth_subdivision();
    // "test_studio_obj_export"
    test_studio_obj_export();
    // "test_studio_native_roundtrip"
    test_studio_native_roundtrip();
    // "test_modifier_stack_transactions"
    test_modifier_stack_transactions();
    // "test_schema_v2_materials_and_atomic_file_roundtrip"
    test_schema_v2_materials_and_atomic_file_roundtrip();
    // "test_native_rejects_malformed_and_unknown_schema"
    test_native_rejects_malformed_and_unknown_schema();

    if (g_failed == 0) {
        std::printf("All Studio tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d Studio test(s) FAILED.\n", g_failed);
    return 1;
}
