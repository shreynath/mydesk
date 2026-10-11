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

void test_mirror_and_stretch() {
    mydesk::document::Document doc;
    const auto l1 = doc.addLine({0, 0}, {10, 0});
    // Mirror across y = 5 (axis (0,5) to (10,5)) with eraseSource = false
    auto m1 = mydesk::modify::mirrorEntities(doc, {l1}, {0, 5}, {10, 5}, false);
    expect(m1.ok, m1.message);
    expect(doc.entities().size() == 2, "source + mirrored copy");
    const auto* l2 = std::get_if<mydesk::document::LineEntity>(&doc.entities().back().geometry);
    expect(l2 != nullptr, "mirrored is line");
    expect(l2->start.nearlyEqual({0, 10}, 1e-6) && l2->end.nearlyEqual({10, 10}, 1e-6),
           "mirrored line position");

    // Mirror with eraseSource = true
    auto m2 = mydesk::modify::mirrorEntities(doc, {l1}, {0, 0}, {0, 10}, true);
    expect(m2.ok, m2.message);
    // l1 erased, new mirrored copy created across y-axis
    expect(doc.findEntity(l1) == nullptr, "source erased");

    // Mirror Arc
    mydesk::document::Document docArc;
    const auto arcId = docArc.addArc({5, 5}, 3.0, 0.0, 90.0);
    auto mArc = mydesk::modify::mirrorEntities(docArc, {arcId}, {0, 0}, {10, 0}, false);
    expect(mArc.ok, mArc.message);
    expect(docArc.entities().size() == 2, "arc mirrored");

    // STRETCH
    mydesk::document::Document docStretch;
    const auto sLine = docStretch.addLine({0, 0}, {10, 0});
    // Window from (8, -2) to (12, 2) covers endpoint (10, 0)
    auto resStr = mydesk::modify::stretchEntities(docStretch, {sLine}, {8, -2}, {12, 2}, {5, 3});
    expect(resStr.ok, resStr.message);
    const auto* strLine = std::get_if<mydesk::document::LineEntity>(&docStretch.findEntity(sLine)->geometry);
    expect(strLine != nullptr, "stretched line");
    expect(strLine->start.nearlyEqual({0, 0}, 1e-6), "start unmoved");
    expect(strLine->end.nearlyEqual({15, 3}, 1e-6), "end stretched to (15, 3)");
}

void test_array_rectangular_and_polar() {
    mydesk::document::Document doc;
    const auto c = doc.addCircle({10, 10}, 2.0);
    // 3 rows, 4 cols, rowSpacing 20, colSpacing 30
    auto rArr = mydesk::modify::arrayRectangular(doc, {c}, 3, 4, 20.0, 30.0);
    expect(rArr.ok, rArr.message);
    expect(doc.entities().size() == 12, "3x4 = 12 total entities");

    // Polar array
    mydesk::document::Document docPolar;
    const auto l = docPolar.addLine({10, 0}, {20, 0});
    // 4 items around (0,0), 360 degrees
    auto pArr = mydesk::modify::arrayPolar(docPolar, {l}, {0, 0}, 4, 360.0);
    expect(pArr.ok, pArr.message);
    expect(docPolar.entities().size() == 4, "4 items in 360 polar array");
}

