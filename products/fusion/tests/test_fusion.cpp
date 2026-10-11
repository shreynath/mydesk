// SPDX-License-Identifier: MPL-2.0
// MyDesk Fusion — Regression & Verification Tests
#include "mydesk/fusion/fusion_document.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using namespace mydesk::fusion;

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

static void test_fusion_parametric_timeline() {
    FusionDocument doc;
    const auto sk = doc.addSketchRectangle("Base", 50.0, 30.0);
    const auto ex = doc.addExtrude("Extrude1", sk, 10.0);
    CHECK(sk > 0);
    CHECK(ex > 0);
    CHECK(doc.bodies().size() == 1);
    const auto& body = doc.bodies().front();
    CHECK(!body.shape.IsNull());
    CHECK(body.isValid);
    CHECK(body.solidCount == 1);
    CHECK(body.shellCount == 1);
    CHECK(body.faceCount == 6);
    CHECK(body.edgeCount == 12);
    CHECK(body.vertexCount == 8);
    CHECK_NEAR(body.volumeMm3, 50.0 * 30.0 * 10.0, 1e-6);
    CHECK_NEAR(body.surfaceAreaMm2, 2.0 * (50.0 * 30.0 + 50.0 * 10.0 + 30.0 * 10.0), 1e-6);
}

static void test_fusion_direct_push_pull() {
    FusionDocument doc;
    const auto sk = doc.addSketchRectangle("Base", 40.0, 40.0);
    const auto ex = doc.addExtrude("Extrude1", sk, 10.0);
    const auto pushed = doc.addDirectPushPull("OffsetFace", ex, 5.0);

    CHECK(doc.bodies().size() == 1);
    // Depth increased from 10 to 15
    const auto& body = doc.bodies().front();
    CHECK(body.featureId == pushed);
    CHECK(body.isValid);
    CHECK(body.faceCount == 6);
    CHECK_NEAR(body.bboxMax[2], 15.0, 1e-6);
    CHECK_NEAR(body.volumeMm3, 40.0 * 40.0 * 15.0, 1e-6);
}

static void test_fusion_timeline_rollback() {
    FusionDocument doc;
    const auto sk = doc.addSketchRectangle("Base", 20.0, 20.0);
    doc.addExtrude("Extrude1", sk, 10.0);
    CHECK(doc.bodies().size() == 1);

    // Rollback cursor to sketch only
    doc.setTimelineCursor(1);
    CHECK(doc.bodies().empty());

    // Roll forward to end
    doc.setTimelineCursor(2);
    CHECK(doc.bodies().size() == 1);
}

static void test_fusion_cam_pocketing_gcode() {
    FusionDocument doc;
    const auto sketch = doc.addSketchRectangle("PocketBoundary", 40.0, 20.0);
    const auto solid = doc.addExtrude("Stock", sketch, 10.0);
    const auto otherSketch = doc.addSketchRectangle("UnselectedBoundary", 100.0, 100.0);
    doc.addExtrude("UnselectedStock", otherSketch, 8.0);
    doc.addCAMPocket("Pocket1", solid, 6.0, 5.0, 5.0, 2.0);
    CHECK(doc.camSetups().size() == 1);
    const auto& setup = doc.camSetups().front();
    CHECK(setup.targetBodyFeatureId == solid);
    CHECK_NEAR(setup.boundaryMin[0], 3.0, 1e-6);
    CHECK_NEAR(setup.boundaryMin[1], 3.0, 1e-6);
    CHECK_NEAR(setup.boundaryMax[0], 37.0, 1e-6);
    CHECK_NEAR(setup.boundaryMax[1], 17.0, 1e-6);
    CHECK_NEAR(setup.stepoverMm, 5.0, 1e-6);
    CHECK_NEAR(setup.stepdownMm, 2.0, 1e-6);
    // Four raster rows (34 mm each, 14 mm between rows) at three Z levels.
    CHECK_NEAR(setup.totalPathLengthMm, 450.0, 1e-6);
    CHECK(!setup.toolpath.empty());

    const auto gcode = doc.exportGCode();
    CHECK(gcode.find("G90 G21") != std::string::npos);
    CHECK(gcode.find("S10000 M03") != std::string::npos);
    CHECK(gcode.find("X37.000 Y3.000 Z8.000") != std::string::npos);
    CHECK(gcode.find("X3.000 Y8.000 Z8.000") != std::string::npos);
    CHECK(gcode.find("Z6.000") != std::string::npos);
    CHECK(gcode.find("Z5.000") != std::string::npos);
    CHECK(gcode.find("M05 M30") != std::string::npos);
}

static void test_fusion_cam_rejects_invalid_geometry_or_cut_parameters() {
    FusionDocument doc;
    const auto sketch = doc.addSketchRectangle("Tiny", 5.0, 5.0);
    const auto solid = doc.addExtrude("TinyStock", sketch, 2.0);
    doc.addCAMPocket("ImpossiblePocket", solid, 6.0, 3.0, 2.0, 1.0);
    CHECK(doc.camSetups().empty());
    CHECK(!doc.lastError().empty());
}

static void test_fusion_ecad_board_integration() {
    FusionDocument doc;
    doc.addECADBoard("MainController", 120.0, 80.0, 1.6);
    CHECK(doc.ecadBoard().has_value());
    CHECK_NEAR(doc.ecadBoard()->widthMm, 120.0, 1e-6);
    CHECK_NEAR(doc.ecadBoard()->thicknessMm, 1.6, 1e-6);
}

static void test_fusion_native_roundtrip() {
    FusionDocument doc;
    doc.setName("DroneArm");
    const auto sk = doc.addSketchRectangle("Sketch1", 100.0, 50.0);
    const auto solid = doc.addExtrude("Extrude1", sk, 20.0);
    doc.addCAMPocket("FinishPocket", solid, 10.0, 5.0, 10.0, 2.5);

    const auto json = doc.serializeNative();
    CHECK(!json.empty());

    std::string err;
    FusionDocument loaded = FusionDocument::deserializeNative(json, &err);
    CHECK(err.empty());
    CHECK(loaded.name() == "DroneArm");
    CHECK(loaded.timeline().size() == 3);
    CHECK(loaded.bodies().size() == 1);
    CHECK(loaded.bodies().front().isValid);
    CHECK(loaded.bodies().front().faceCount == 6);
    CHECK_NEAR(loaded.bodies().front().volumeMm3, 100.0 * 50.0 * 20.0, 1e-6);
    CHECK(loaded.camSetups().size() == 1);
    CHECK(loaded.camSetups().front().targetBodyFeatureId == solid);
    CHECK_NEAR(loaded.camSetups().front().stepdownMm, 2.5, 1e-6);
}

static void test_fusion_feature_edit_undo_redo() {
    FusionDocument doc;
    const auto sketch = doc.addSketchRectangle("Profile", 20.0, 10.0);
    const auto extrude = doc.addExtrude("Pad", sketch, 5.0);
    doc.clearHistory();
    CHECK(doc.setFeatureParameter(extrude, "depth", 8.0));
    CHECK_NEAR(doc.bodies().front().volumeMm3, 1600.0, 1e-6);
    CHECK(doc.canUndo());
    CHECK(doc.undo());
    CHECK_NEAR(doc.bodies().front().volumeMm3, 1000.0, 1e-6);
    CHECK(doc.redo());
    CHECK_NEAR(doc.bodies().front().volumeMm3, 1600.0, 1e-6);
    CHECK(!doc.setFeatureParameter(extrude, "depth", -1.0));
    CHECK_NEAR(doc.bodies().front().volumeMm3, 1600.0, 1e-6);
}

int main() {
    // Registered capability test identifiers:
    // "test_fusion_desktop_workflow" is implemented by products/fusion/app/tests/test_fusion_ui.cpp.
    // "test_fusion_parametric_timeline"
    test_fusion_parametric_timeline();
    // "test_fusion_direct_push_pull"
    test_fusion_direct_push_pull();
    // "test_fusion_timeline_rollback"
    test_fusion_timeline_rollback();
    // "test_fusion_cam_pocketing_gcode"
    test_fusion_cam_pocketing_gcode();
    // "test_fusion_cam_rejects_invalid_geometry_or_cut_parameters"
    test_fusion_cam_rejects_invalid_geometry_or_cut_parameters();
    // "test_fusion_ecad_board_integration"
    test_fusion_ecad_board_integration();
    // "test_fusion_native_roundtrip"
    test_fusion_native_roundtrip();
    // "test_fusion_feature_edit_undo_redo"
    test_fusion_feature_edit_undo_redo();

    if (g_failed == 0) {
        std::printf("All Fusion tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d Fusion test(s) FAILED.\n", g_failed);
    return 1;
}
