#include "../test_assert.hpp"

#include "mydesk/commands/session.hpp"
#include "mydesk/modify/modify_ops.hpp"

void test_pline_and_trim() {
    mydesk::commands::Session session;
    expect(session.executeLine("PL 0,0 10,0 10,10 END").ok, "pline");
    expect(session.document().entities().size() == 1, "one pline");
    const auto* pl = std::get_if<mydesk::document::PolylineEntity>(
        &session.document().entities().front().geometry);
    expect(pl && pl->vertices.size() == 3 && !pl->closed, "open 3-vertex pline");

    session.document().addLine({5, -5}, {5, 5});
    // Vertical cutter crosses horizontal first segment of... wait pline is one entity.
    // TRIM on LINE: two crossing lines
    mydesk::commands::Session s2;
    s2.document().addLine({0, 0}, {10, 0});
    s2.document().addLine({5, -5}, {5, 5});
    auto trim = mydesk::modify::trimAtPoint(s2.document(), {2, 0}, {});
    expect(trim.ok, trim.message);
    // Horizontal should become 0,0-5,0 (left of pick) and/or 5,0-10,0
    expect(s2.document().entities().size() >= 2, "kept pieces + cutter");
}

void test_extend_offset_fillet_chamfer() {
    mydesk::document::Document doc;
    // Horizontal short of vertical boundary
    const auto idA = doc.addLine({0, 0}, {4, 0});
    doc.addLine({10, -5}, {10, 5});
    auto ext = mydesk::modify::extendAtPoint(doc, {3.5, 0}, {});
    expect(ext.ok, ext.message);
    const auto* line = std::get_if<mydesk::document::LineEntity>(&doc.findEntity(idA)->geometry);
    expect(line && line->end.nearlyEqual({10, 0}, 1e-6), "extended to x=10");

    mydesk::document::Document doc2;
    const auto lid = doc2.addLine({0, 0}, {10, 0});
    auto off = mydesk::modify::offsetEntity(doc2, lid, 5.0, {5, 5});
    expect(off.ok, off.message);
    expect(doc2.entities().size() == 2, "offset created");

    mydesk::document::Document doc3;
    const auto a = doc3.addLine({0, 10}, {0, 0});
    const auto b = doc3.addLine({0, 0}, {10, 0});
    auto fillet = mydesk::modify::filletLines(doc3, a, b, 2.0);
    expect(fillet.ok, fillet.message);
    bool foundArc = false;
    for (const auto& e : doc3.entities()) {
        if (std::holds_alternative<mydesk::document::ArcEntity>(e.geometry)) {
            foundArc = true;
        }
    }
    expect(foundArc, "fillet created arc");

    mydesk::document::Document doc4;
    const auto c = doc4.addLine({0, 10}, {0, 0});
    const auto d = doc4.addLine({0, 0}, {10, 0});
    auto chamfer = mydesk::modify::chamferLines(doc4, c, d, 2.0, 2.0);
    expect(chamfer.ok, chamfer.message);
    expect(doc4.entities().size() == 3, "two trimmed lines + chamfer");
}

void test_selection_window_crossing() {
    mydesk::document::Document doc;
    doc.addLine({0, 0}, {1, 1});
    doc.addLine({10, 10}, {11, 11});
    mydesk::geometry::BBox window{0.5, 0.5, 2, 2};
    // Window (not crossing): first line not fully inside
    doc.selectInRect(window, false, false);
    expect(doc.selection().empty(), "window requires full containment");

    mydesk::geometry::BBox big{-1, -1, 2, 2};
    doc.selectInRect(big, false, false);
    expect(doc.selection().size() == 1, "fully inside selected");

    doc.clearSelection();
    mydesk::geometry::BBox cross{0.2, 0.2, 0.8, 0.8};
    doc.selectInRect(cross, true, false);
    expect(doc.selection().size() == 1, "crossing selects intersecting");
}
