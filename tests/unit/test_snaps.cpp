#include "../test_assert.hpp"

#include "mydesk/document/document.hpp"
#include "mydesk/draft/snaps.hpp"

void test_snap_endpoint() {
    mydesk::document::Document doc;
    doc.addLine({0, 0}, {100, 0});
    mydesk::draft::SnapSettings settings;
    settings.aperture = 5.0;
    auto snap = mydesk::draft::findSnap(doc, {1.5, 0.5}, settings);
    expect(snap.has_value(), "snap found");
    expect(snap->kind == "endpoint", "endpoint kind");
    expect(snap->point.nearlyEqual({0, 0}), "snapped to start");
}
