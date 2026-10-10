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
}
