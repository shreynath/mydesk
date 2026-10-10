#include "../test_assert.hpp"

#include "mydesk/commands/session.hpp"
#include "mydesk/document/document.hpp"
#include "mydesk/draft/snaps.hpp"
#include "mydesk/exchange/native_format.hpp"
#include "mydesk/geometry/vec2.hpp"

#include <cmath>

void test_chprop_and_dimlinear() {
    mydesk::commands::Session session;
    expect(session.executeLine("L 0,0 100,0").ok, "line");
    session.document().select(session.document().entities().front().id);
    auto color = session.executeLine("CHPROP COLOR FF0000");
    expect(color.ok, color.message);
    expect(session.document().entities().front().colorArgb == 0xFFFF0000u, "entity red");

    auto dim = session.executeLine("DIMLINEAR 0,0 100,0 50,20");
    expect(dim.ok, dim.message);
    expect(session.document().entities().size() == 2, "line + dim");
    const auto* d =
        std::get_if<mydesk::document::DimensionLinearEntity>(&session.document().entities().back().geometry);
    expect(d != nullptr, "is dim");
    const auto layout = mydesk::document::layoutDimension(*d);
    expect(std::abs(layout.measurement - 100.0) < 1e-9, "measurement 100");

    auto bylayer = session.executeLine("CH COLOR BYLAYER");
    expect(bylayer.ok, bylayer.message);
}

void test_osnap_intersection_perpendicular() {
    mydesk::document::Document doc;
    doc.addLine({0, 0}, {100, 0});
    doc.addLine({50, -50}, {50, 50});
    mydesk::draft::SnapSettings settings;
    settings.aperture = 8.0;
    settings.endpoint = settings.midpoint = settings.center = false;
    settings.intersection = true;
    settings.perpendicular = false;
    auto snap = mydesk::draft::findSnap(doc, {51, 1}, settings);
    expect(snap.has_value(), "intersection found");
    expect(snap->kind == "intersection", "kind int");
    expect(snap->point.nearlyEqual({50, 0}), "at crossing");

    settings.intersection = false;
    settings.perpendicular = true;
    settings.fromPoint = mydesk::geometry::Vec2{50, 40};
    snap = mydesk::draft::findSnap(doc, {50, 2}, settings);
    expect(snap.has_value(), "perp found");
    expect(snap->kind == "perpendicular", "kind per");
    expect(snap->point.nearlyEqual({50, 0}), "foot on horizontal");

    mydesk::commands::Session session;
    expect(session.executeLine("OSNAP INT OFF").ok, "osnap");
    expect(!session.modes().snapIntersection, "int off");
    expect(session.executeLine("OSNAP PER ON").ok, "per on");
    expect(session.modes().snapPerpendicular, "per flag");
}

void test_dim_native_roundtrip() {
    mydesk::document::Document doc;
    doc.addDimensionLinear({0, 0}, {40, 0}, {20, 10});
    const auto json = mydesk::exchange::serializeNative(doc);
    mydesk::document::Document loaded;
    auto report = mydesk::exchange::deserializeNative(loaded, json);
    expect(report.ok, "deserialize ok");
    expect(loaded.entities().size() == 1, "one entity");
    expect(loaded.entities().front().kind() == mydesk::document::EntityKind::DimensionLinear,
           "dim kind");
}

void test_linetypes_and_lineweights() {
    mydesk::commands::Session session;
    
    // 1. Set Layer Properties
    expect(session.executeLine("LAYER NEW Props").ok, "layer Props");
    expect(session.executeLine("LAYER LTYPE Props DASHED").ok, "layer dashed");
    expect(session.executeLine("LAYER LWEIGHT Props 0.5").ok, "layer 0.5mm");
    
    // 2. Create entity on that layer
    session.executeLine("LAYER SET Props");
    session.executeLine("L 0,0 10,0");
    const auto& ent = session.document().entities().front();
    
    // Verify inheritance
    expect(session.document().resolvedLinetype(ent) == mydesk::document::Linetype::Dashed, "inherited dashed");
    expect(std::abs(session.document().resolvedLineweightMm(ent) - 0.5) < 1e-9, "inherited 0.5mm");
    
    // 3. Override properties via CHPROP
    session.document().select(ent.id);
    expect(session.executeLine("CHPROP LTYPE CENTER").ok, "override center");
    expect(session.document().resolvedLinetype(ent) == mydesk::document::Linetype::Center, "override center");
    
    expect(session.executeLine("CHPROP LWEIGHT 0.8").ok, "override 0.8mm");
    expect(std::abs(session.document().resolvedLineweightMm(ent) - 0.8) < 1e-9, "override 0.8mm");
    
    // 4. Revert to ByLayer
    expect(session.executeLine("CHPROP LWEIGHT BYLAYER").ok, "revert weight");
    expect(std::abs(session.document().resolvedLineweightMm(ent) - 0.5) < 1e-9, "reverted to layer");
    
    // 5. Native round-trip
    const auto json = mydesk::exchange::serializeNative(session.document());
    mydesk::document::Document loaded;
    auto report = mydesk::exchange::deserializeNative(loaded, json);
    expect(report.ok, "native load");
    
    const auto& loadedEnt = loaded.entities().front();
    expect(loadedEnt.linetype == mydesk::document::Linetype::Center, "roundtrip linetype");
    expect(loadedEnt.lineweightByLayer == true, "roundtrip bylayer weight");
    expect(std::abs(loaded.resolvedLineweightMm(loadedEnt) - 0.5) < 1e-9, "roundtrip resolved weight");
}

void test_advanced_properties() {
    mydesk::commands::Session session;
    
    // 1. Setup layer
    expect(session.executeLine("LAYER NEW Adv").ok, "layer Adv");
    expect(session.executeLine("LAYER SET Adv").ok, "set Adv");
    
    // 2. Create entity
    session.executeLine("L 0,0 10,0");
    const auto id = session.document().entities().front().id;
    session.document().select(id);
    
    // 3. Test Transparency
    expect(session.executeLine("CHPROP TRANSPARENCY 0.5").ok, "set transparency 0.5");
    expect(std::abs(session.document().resolvedTransparency(session.document().entities().front()) - 0.5f) < 1e-6, "resolved transparency 0.5");
    
    expect(session.executeLine("CHPROP TRANSPARENCY BYLAYER").ok, "transparency bylayer");
    expect(session.document().entities().front().transparencyByLayer == true, "flag bylayer");
    
    // 4. Test Thickness
    expect(session.executeLine("CHPROP THICKNESS 2.0").ok, "set thickness 2.0");
    expect(std::abs(session.document().resolvedThickness(session.document().entities().front()) - 2.0) < 1e-9, "resolved thickness 2.0");
    
    expect(session.executeLine("CHPROP THICKNESS BYLAYER").ok, "thickness bylayer");
    expect(session.document().entities().front().thicknessByLayer == true, "flag bylayer");
}
