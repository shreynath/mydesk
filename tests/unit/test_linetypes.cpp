#include "../test_assert.hpp"

#include "mydesk/commands/session.hpp"
#include "mydesk/document/entity.hpp"
#include "mydesk/exchange/dxf_reader.hpp"
#include "mydesk/exchange/dxf_writer.hpp"
#include "mydesk/exchange/native_format.hpp"

#include <filesystem>

void test_linetype_lineweight_commands_and_roundtrip() {
    mydesk::commands::Session session;
    expect(session.executeLine("LAYER NEW Hidden").ok, "layer");
    expect(session.executeLine("LAYER LTYPE Hidden DASHED").ok, "layer ltype");
    expect(session.executeLine("LAYER LWEIGHT Hidden 0.50").ok, "layer weight");
    expect(session.document().findLayerByName("Hidden")->linetype ==
               mydesk::document::Linetype::Dashed,
           "layer dashed");
    expect(session.document().findLayerByName("Hidden")->lineweightMm == 0.50, "layer 0.50");

    expect(session.executeLine("LAYER MAKE Hidden").ok, "current");
    expect(session.executeLine("L 0,0 100,0").ok, "line");
    const auto id = session.document().entities().front().id;
    expect(session.document().resolvedLinetype(session.document().entities().front()) ==
               mydesk::document::Linetype::Dashed,
           "entity inherits dashed");
    expect(session.document().resolvedLineweightMm(session.document().entities().front()) == 0.50,
           "entity inherits weight");

    session.document().select(id);
    expect(session.executeLine("CHPROP LTYPE CENTER").ok, "override ltype");
    expect(session.executeLine("CHPROP LWEIGHT 0.70").ok, "override weight");
    expect(session.executeLine("LTSCALE 2").ok, "ltscale");
    expect(session.document().linetypeScale() == 2.0, "scale");
    expect(session.document().entities().front().linetype == mydesk::document::Linetype::Center,
           "entity center");
    expect(!session.document().entities().front().lineweightByLayer, "override flag");
    expect(session.document().entities().front().lineweightMm == 0.70, "entity weight");

    const auto pattern = mydesk::document::linetypeDashPattern(mydesk::document::Linetype::Center);
    expect(pattern.size() == 4, "center pattern segments");

    const auto json = mydesk::exchange::serializeNative(session.document());
    mydesk::document::Document loaded;
    auto report = mydesk::exchange::deserializeNative(loaded, json);
    expect(report.ok, report.errors.empty() ? "native ok" : report.errors.front());
    expect(loaded.linetypeScale() == 2.0, "scale roundtrip");
    expect(loaded.findLayerByName("Hidden")->linetype == mydesk::document::Linetype::Dashed,
           "layer ltype roundtrip");
    expect(loaded.findLayerByName("Hidden")->lineweightMm == 0.50, "layer weight roundtrip");
    expect(loaded.entities().front().linetype == mydesk::document::Linetype::Center,
           "entity ltype roundtrip");
    expect(loaded.entities().front().lineweightMm == 0.70, "entity weight roundtrip");

    expect(session.executeLine("CHPROP LTYPE BYLAYER").ok, "bylayer ltype");
    expect(session.executeLine("CHPROP LWEIGHT BYLAYER").ok, "bylayer weight");
    expect(session.document().resolvedLinetype(session.document().entities().front()) ==
               mydesk::document::Linetype::Dashed,
           "back to layer dashed");

    expect(session.executeLine("L 10,10 20,20").ok, "second line");
    const auto id2 = session.document().entities().back().id;
    session.document().clearSelection();
    session.document().select(id2);
    expect(session.executeLine("CHPROP LTYPE CENTER").ok, "override ltype second");
    expect(session.executeLine("CHPROP LWEIGHT 0.70").ok, "override weight second");
    expect(session.document().entities().back().linetype == mydesk::document::Linetype::Center,
           "second line center");
    expect(session.document().entities().back().lineweightMm == 0.70, "second line weight");

    // Verify undo and redo on entity properties
    expect(session.executeLine("UNDO").ok, "undo chprop weight");
    expect(session.document().entities().back().lineweightByLayer, "undone weight to bylayer");
    expect(session.executeLine("REDO").ok, "redo chprop weight");
    expect(session.document().entities().back().lineweightMm == 0.70, "redone weight 0.70");

    const auto dir = std::filesystem::temp_directory_path() / "mydesk_tests";
    std::filesystem::create_directories(dir);
    const auto dxf = dir / "linetypes.dxf";
    auto exportReport = mydesk::exchange::exportDxfR12(session.document(), dxf);
    expect(exportReport.ok, "dxf export");
    mydesk::document::Document fromDxf;
    auto importReport = mydesk::exchange::importDxf(fromDxf, dxf);
    expect(importReport.ok, "dxf import");
    expect(fromDxf.findLayerByName("Hidden")->linetype == mydesk::document::Linetype::Dashed,
           "dxf layer linetype");
    expect(fromDxf.findLayerByName("Hidden")->lineweightMm == 0.50, "dxf layer lineweight");
    expect(fromDxf.linetypeScale() == 2.0, "dxf ltscale roundtrip");
    expect(fromDxf.entities().size() == 2, "dxf entity count");
    expect(fromDxf.entities().front().linetype == mydesk::document::Linetype::ByLayer,
           "dxf entity 1 bylayer linetype");
    expect(fromDxf.resolvedLinetype(fromDxf.entities().front()) ==
               mydesk::document::Linetype::Dashed,
           "dxf entity 1 resolved linetype dashed");
    expect(fromDxf.entities().front().lineweightByLayer, "dxf entity 1 bylayer weight");
    expect(fromDxf.resolvedLineweightMm(fromDxf.entities().front()) == 0.50,
           "dxf entity 1 resolved weight 0.50");
    expect(fromDxf.entities().back().linetype == mydesk::document::Linetype::Center,
           "dxf entity 2 center linetype");
    expect(!fromDxf.entities().back().lineweightByLayer, "dxf entity 2 explicit weight flag");
    expect(fromDxf.entities().back().lineweightMm == 0.70, "dxf entity 2 weight 0.70");
}
