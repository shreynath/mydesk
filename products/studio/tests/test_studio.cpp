// SPDX-License-Identifier: MPL-2.0
// MyDesk Studio — Regression & Verification Tests
#include "mydesk/studio/studio_document.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
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

    doc.addTurboSmoothModifier(id, 1);
    CHECK(obj->evaluatedMesh.triangles.size() == baseTris * 4);
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

    if (g_failed == 0) {
        std::printf("All Studio tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d Studio test(s) FAILED.\n", g_failed);
    return 1;
}
