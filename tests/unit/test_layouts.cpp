#include "../test_assert.hpp"

#include "mydesk/commands/session.hpp"
#include "mydesk/document/document.hpp"
#include "mydesk/exchange/plotter.hpp"

#include <filesystem>
#include <fstream>

void test_layouts_viewports_and_plotting() {
    using namespace mydesk;

    // 1. Initial document state
    document::Document doc;
    expect(doc.layouts().size() == 2, "initial doc has Model and Layout1");
    expect(doc.activeLayoutName() == "Model", "initial active layout is Model");
    expect(doc.tileMode(), "tileMode is true initially");
    expect(!doc.isPaperSpace(), "paperSpace is false initially");

    // 2. Add and manage layouts
    document::PageSetup setupD;
    setupD.paperName = "ARCH D (24 x 36 in)";
    setupD.paperWidthMm = 914.4;
    setupD.paperHeightMm = 609.6;
    setupD.printableWidthMm = 880.0;
    setupD.printableHeightMm = 580.0;
    expect(doc.addLayout("SHEET_D", setupD), "addLayout SHEET_D");
    expect(doc.layouts().size() == 3, "3 layouts total");

    expect(doc.setActiveLayout("SHEET_D"), "switch to SHEET_D");
    expect(doc.activeLayoutName() == "SHEET_D", "active layout is SHEET_D");
    expect(!doc.tileMode(), "tileMode is false in paper layout");
    expect(doc.isPaperSpace(), "isPaperSpace is true in paper layout");

    expect(doc.renameLayout("SHEET_D", "A101_PLANS"), "rename SHEET_D to A101_PLANS");
    expect(doc.activeLayoutName() == "A101_PLANS", "active layout name updated after rename");

    expect(doc.removeLayout("Layout1"), "remove Layout1");
    expect(doc.layouts().size() == 2, "2 layouts remain: Model and A101_PLANS");

    // 3. Viewports and Viewport layer freeze
    auto* lPlans = doc.findLayout("A101_PLANS");
    expect(lPlans != nullptr, "found layout A101_PLANS");
    lPlans->viewports.clear(); // Clear default viewport to test single custom viewport

    document::Viewport vp;
    vp.center = {400.0, 300.0};
    vp.width = 700.0;
    vp.height = 500.0;
    vp.viewCenter = {50.0, 50.0};
    vp.viewHeight = 100.0;
    auto vpId = doc.addViewportToLayout("A101_PLANS", vp);
    expect(vpId > 0, "viewport created with valid ID");

    auto layerNotes = doc.addLayer("NOTES");
    auto layerDim = doc.addLayer("DIMS");
    expect(doc.setViewportFrozen("A101_PLANS", vpId, layerNotes, true), "freeze NOTES in viewport");

    const auto* layoutPlans = doc.findLayout("A101_PLANS");
    expect(layoutPlans != nullptr, "found layout A101_PLANS");
    expect(layoutPlans->viewports.size() >= 1, "layout has viewports");
    bool foundVp = false;
    for (const auto& v : layoutPlans->viewports) {
        if (v.id == vpId) {
            foundVp = true;
            expect(v.frozenLayers.count(layerNotes) == 1, "NOTES layer is VP-frozen");
            expect(v.frozenLayers.count(layerDim) == 0, "DIMS layer is not VP-frozen");
        }
    }
    expect(foundVp, "viewport found in layout");

    // 4. Add model entities and test plotting
    doc.setActiveLayer(doc.activeLayerId()); // layer "0"
    doc.addLine({0.0, 0.0}, {100.0, 0.0});
    doc.addCircle({50.0, 50.0}, 25.0);

    doc.setActiveLayer(layerNotes);
    doc.addText({50.0, 80.0}, 5.0, "DO NOT PLOT IN VP");

    doc.setActiveLayer(layerDim);
    doc.addLine({0.0, 5.0}, {100.0, 5.0});

    const auto tempDir = std::filesystem::temp_directory_path() / "mydesk_tests";
    std::filesystem::create_directories(tempDir);
    const auto svgPath = tempDir / "plot_sheet_a101.svg";

    auto plotReport = exchange::plotToSvg(doc, "A101_PLANS", svgPath);
    expect(plotReport.ok, plotReport.message);
    expect(plotReport.renderedViewports >= 1, "rendered viewports > 0");
    expect(plotReport.renderedEntities >= 3, "rendered entities in viewport");
    expect(std::filesystem::exists(svgPath), "SVG file generated");

    // Verify SVG contents
    std::ifstream svgFile(svgPath);
    std::string svgContent((std::istreambuf_iterator<char>(svgFile)), std::istreambuf_iterator<char>());
    expect(svgContent.find("<svg") != std::string::npos, "contains svg tag");
    expect(svgContent.find("clipPath") != std::string::npos, "contains viewport clipPath");
    expect(svgContent.find("<circle") != std::string::npos, "contains plotted circle");
    expect(svgContent.find("<line") != std::string::npos, "contains plotted line");
    // Verify that the VP-frozen layer text is NOT in the SVG
    expect(svgContent.find("DO NOT PLOT IN VP") == std::string::npos, "VP-frozen layer entity excluded from plot");

    // 5. Test layer plottable flag
    doc.setLayerPlottable(layerDim, false);
    auto plotReport2 = exchange::plotToSvg(doc, "A101_PLANS", svgPath);
    expect(plotReport2.ok, plotReport2.message);
    expect(plotReport2.renderedEntities == plotReport.renderedEntities - 1, "unplottable layer entity excluded");

    // 6. Interactive commands via Session
    commands::Session session;
    auto resList = session.executeLine("LAYOUT LIST");
    expect(resList.ok, resList.message);
    expect(resList.message.find("Model") != std::string::npos, "LAYOUT LIST lists Model");

    auto resNew = session.executeLine("LAYOUT NEW DETAILS");
    expect(resNew.ok, resNew.message);
    expect(session.document().findLayout("DETAILS") != nullptr, "LAYOUT NEW created DETAILS");

    auto resSet = session.executeLine("LAYOUT SET DETAILS");
    expect(resSet.ok, resSet.message);
    expect(session.document().activeLayoutName() == "DETAILS", "LAYOUT SET switched to DETAILS");
    expect(!session.document().tileMode(), "tileMode is 0 in DETAILS");

    auto resMv = session.executeLine("MVIEW 100,100 120 80 50");
    expect(resMv.ok, resMv.message);
    expect(!session.document().findLayout("DETAILS")->viewports.empty(), "MVIEW added viewport");

    auto resPs = session.executeLine("PSPACE");
    expect(resPs.ok, resPs.message);
    expect(session.document().isPaperSpace(), "PSPACE activated");

    auto resMs = session.executeLine("MSPACE");
    expect(resMs.ok, resMs.message);
    expect(!session.document().isPaperSpace(), "MSPACE activated");

    const auto plotCmdPath = tempDir / "plot_details.svg";
    auto resPlot = session.executeLine("PLOT DETAILS " + plotCmdPath.string());
    expect(resPlot.ok, resPlot.message);
    expect(std::filesystem::exists(plotCmdPath), "PLOT command generated file");

    auto resTm = session.executeLine("TILEMODE 1");
    expect(resTm.ok, resTm.message);
    expect(session.document().tileMode(), "TILEMODE 1 returned to model space");
    expect(session.document().activeLayoutName() == "Model", "returned to Model layout");
}
