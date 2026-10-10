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
