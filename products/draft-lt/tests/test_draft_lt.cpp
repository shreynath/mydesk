// SPDX-License-Identifier: MPL-2.0
// MyDesk Draft LT — Regression & Verification Tests
#include "mydesk/commands/session.hpp"
#include "mydesk/document/document.hpp"
#include "mydesk/exchange/native_format.hpp"
#include "mydesk/exchange/plotter.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace mydesk::document;
using namespace mydesk::commands;

static int g_failed = 0;
#define CHECK(cond) \
    do { if (!(cond)) { \
        std::fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, #cond); \
        ++g_failed; \
    } } while(0)

static void test_draft_lt_2d_drafting() {
    Session session;
    auto newLayer = session.executeLine("LAYER NEW WALLS");
    CHECK(newLayer.ok);

    auto makeLayer = session.executeLine("LA MAKE WALLS");
    CHECK(makeLayer.ok);

    auto lineCmd = session.executeLine("L 0,0 100,0");
    CHECK(lineCmd.ok);

    auto circCmd = session.executeLine("C 50,50 25");
    CHECK(circCmd.ok);

    CHECK(session.document().entities().size() == 2);
}

static void test_draft_lt_linetypes() {
    Session session;
    CHECK(session.executeLine("LAYER NEW CENTERLINES").ok);
    CHECK(session.executeLine("LAYER LTYPE CENTERLINES CENTER").ok);
    CHECK(session.executeLine("LAYER LWEIGHT CENTERLINES 0.50").ok);

    const auto* layer = session.document().findLayerByName("CENTERLINES");
    CHECK(layer != nullptr);
    CHECK(layer->linetype == Linetype::Center);
    CHECK(layer->lineweightMm == 0.50);
}

static void test_draft_lt_roundtrip() {
    Session session;
    session.document().setName("TestLTProject");
    CHECK(session.executeLine("LAYER MAKE HVAC_2D").ok);
    CHECK(session.executeLine("L 10,20 30,40").ok);

    const auto json = mydesk::exchange::serializeNative(session.document());
    CHECK(!json.empty());

    Document loaded;
    auto report = mydesk::exchange::deserializeNative(loaded, json);
    CHECK(report.ok);
    CHECK(loaded.name() == "TestLTProject");
    CHECK(loaded.layers().size() >= 1);
    CHECK(loaded.entities().size() == 1);
}

static void test_draft_lt_plot() {
    Session session;
    session.document().setName("DraftLT_Plot_Test");
    CHECK(session.executeLine("L 0,0 200,100").ok);
    CHECK(session.executeLine("C 100,50 30").ok);

    mydesk::document::PageSetup setup;
    setup.paperName = "ISO A4 (210 x 297 mm)";
    setup.paperWidthMm = 297.0;
    setup.paperHeightMm = 210.0;
    setup.printableWidthMm = 287.0;
    setup.printableHeightMm = 200.0;
    CHECK(session.document().addLayout("SHEET_A4", setup));
    CHECK(session.document().setActiveLayout("SHEET_A4"));

    mydesk::document::Viewport vp;
    vp.center = {148.5, 105.0};
    vp.width = 250.0;
    vp.height = 160.0;
    vp.viewCenter = {100.0, 50.0};
    vp.viewHeight = 120.0;
    auto vpId = session.document().addViewportToLayout("SHEET_A4", vp);
    CHECK(vpId > 0);

    const auto tempDir = std::filesystem::temp_directory_path() / "mydesk_tests";
    std::filesystem::create_directories(tempDir);
    const auto svgPath = tempDir / "draft_lt_sheet_a4.svg";

    auto plotReport = mydesk::exchange::plotToSvg(session.document(), "SHEET_A4", svgPath);
    CHECK(plotReport.ok);
    CHECK(plotReport.renderedEntities >= 2);
    CHECK(std::filesystem::exists(svgPath));

    std::ifstream file(svgPath);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    CHECK(content.find("<svg") != std::string::npos);
    CHECK(content.find("<circle") != std::string::npos);
}

static void test_draft_lt_no_3d_solids() {
    Session session;
    auto r1 = session.executeLine("EXTRUDE 10");
    CHECK(!r1.ok);
    auto r2 = session.executeLine("PRESSPULL");
    CHECK(!r2.ok);
}

static void test_draft_lt_layer_states() {
    Session session;
    CHECK(session.executeLine("LAYER MAKE ANNOTATION").ok);
    const auto annotationId = session.document().activeLayerId();
    CHECK(session.executeLine("LAYER COLOR ANNOTATION 00AAFF").ok);
    CHECK(session.executeLine("LAYER LTYPE ANNOTATION DASHED").ok);
    CHECK(session.executeLine("LAYERSTATE SAVE SHEET").ok);
    CHECK(session.executeLine("LAYER SET 0").ok);
    CHECK(session.executeLine("LAYER OFF ANNOTATION").ok);
    CHECK(session.executeLine("LAYER LOCK ANNOTATION").ok);
    CHECK(session.executeLine("LAYERSTATE RESTORE SHEET").ok);
    const auto* restored = session.document().findLayer(annotationId);
    CHECK(restored != nullptr);
    CHECK(restored->visible);
    CHECK(!restored->locked);
    CHECK(restored->colorArgb == 0xFF00AAFFu);
    CHECK(restored->linetype == Linetype::Dashed);
    CHECK(session.document().activeLayerId() == annotationId);
    CHECK(session.executeLine("UNDO").ok);
    CHECK(!session.document().findLayer(annotationId)->visible);
    CHECK(session.executeLine("REDO").ok);

    const auto json = mydesk::exchange::serializeNative(session.document());
    Document loaded;
    CHECK(mydesk::exchange::deserializeNative(loaded, json).ok);
    CHECK(loaded.layerStates().size() == 1);
    CHECK(loaded.findLayerState("sheet") != nullptr);
}

int main() {
    // Registered capability test identifiers:
    // "test_draft_lt_2d_drafting"
    test_draft_lt_2d_drafting();
    // "test_draft_lt_linetypes"
    test_draft_lt_linetypes();
    // "test_draft_lt_roundtrip"
    test_draft_lt_roundtrip();
    // "test_draft_lt_plot"
    test_draft_lt_plot();
    // "test_draft_lt_no_3d_solids"
    test_draft_lt_no_3d_solids();
    // "test_draft_lt_layer_states"
    test_draft_lt_layer_states();

    if (g_failed == 0) {
        std::printf("All Draft LT tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d Draft LT test(s) FAILED.\n", g_failed);
    return 1;
}
