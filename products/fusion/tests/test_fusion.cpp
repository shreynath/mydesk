// SPDX-License-Identifier: MPL-2.0
// MyDesk Fusion — Regression & Verification Tests
#include "mydesk/fusion/fusion_document.hpp"

#include <cassert>
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
    CHECK_NEAR(doc.bodies().front().volumeMm3, 50.0 * 30.0 * 10.0, 1e-6);
}

static void test_fusion_direct_push_pull() {
    FusionDocument doc;
    const auto sk = doc.addSketchRectangle("Base", 40.0, 40.0);
    const auto ex = doc.addExtrude("Extrude1", sk, 10.0);
    doc.addDirectPushPull("OffsetFace", ex, 5.0);

    CHECK(doc.bodies().size() == 1);
    // Depth increased from 10 to 15
    CHECK_NEAR(doc.bodies().front().volumeMm3, 40.0 * 40.0 * 15.0, 1e-6);
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
    doc.addCAMPocket("Pocket1", 6.0, 5.0, 2.5);
    CHECK(doc.camSetups().size() == 1);
    CHECK(doc.camSetups().front().totalPathLengthMm > 0.0);

    const auto gcode = doc.exportGCode();
    CHECK(gcode.find("G90 G21") != std::string::npos);
    CHECK(gcode.find("S10000 M03") != std::string::npos);
    CHECK(gcode.find("M05 M30") != std::string::npos);
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
    doc.addExtrude("Extrude1", sk, 20.0);

    const auto json = doc.serializeNative();
    CHECK(!json.empty());

    std::string err;
    FusionDocument loaded = FusionDocument::deserializeNative(json, &err);
    CHECK(err.empty());
    CHECK(loaded.name() == "DroneArm");
    CHECK(loaded.timeline().size() == 2);
    CHECK(loaded.bodies().size() == 1);
    CHECK_NEAR(loaded.bodies().front().volumeMm3, 100.0 * 50.0 * 20.0, 1e-6);
}

int main() {
    // Registered capability test identifiers:
    // "test_fusion_parametric_timeline"
    test_fusion_parametric_timeline();
    // "test_fusion_direct_push_pull"
    test_fusion_direct_push_pull();
    // "test_fusion_timeline_rollback"
    test_fusion_timeline_rollback();
    // "test_fusion_cam_pocketing_gcode"
    test_fusion_cam_pocketing_gcode();
    // "test_fusion_ecad_board_integration"
    test_fusion_ecad_board_integration();
    // "test_fusion_native_roundtrip"
    test_fusion_native_roundtrip();

    if (g_failed == 0) {
        std::printf("All Fusion tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d Fusion test(s) FAILED.\n", g_failed);
    return 1;
}
