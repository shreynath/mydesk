#include "../test_assert.hpp"

#include "mydesk/commands/session.hpp"
#include "mydesk/document/document.hpp"

void test_rectangle_four_lines() {
    mydesk::document::Document doc;
    const auto id = doc.addRectangle({0, 0}, {10, 5});
    expect(doc.entities().size() == 1, "one polyline entity");
    const auto* pl =
        std::get_if<mydesk::document::PolylineEntity>(&doc.findEntity(id)->geometry);
    expect(pl != nullptr, "is polyline");
    expect(pl->closed, "closed");
    expect(pl->vertices.size() == 4, "four vertices");
    expect(doc.undo(), "undo rect");
    expect(doc.entities().empty(), "rect undone atomically");
}

void test_move_copy_rotate_scale() {
    mydesk::commands::Session session;
    expect(session.executeLine("L 0,0 10,0").ok, "line");
    session.document().select(session.document().entities().front().id);

    expect(session.executeLine("M 0,0 0,5").ok, "move");
    const auto* line = std::get_if<mydesk::document::LineEntity>(
        &session.document().entities().front().geometry);
    expect(line != nullptr, "still line");
    expect(line->start.nearlyEqual({0, 5}), "moved start");
    expect(line->end.nearlyEqual({10, 5}), "moved end");

    expect(session.executeLine("CO 0,0 0,10").ok, "copy");
    expect(session.document().entities().size() == 2, "copy added entity");

    session.document().clearSelection();
    session.document().select(session.document().entities().front().id);
    expect(session.executeLine("RO 0,5 90").ok, "rotate");
    const auto* rotated = std::get_if<mydesk::document::LineEntity>(
        &session.document().entities().front().geometry);
    expect(rotated != nullptr, "rotated line");
    expect(rotated->start.nearlyEqual({0, 5}, 1e-6), "base stays");
    expect(rotated->end.nearlyEqual({0, 15}, 1e-6), "end after 90deg");

    expect(session.executeLine("SC 0,5 2").ok, "scale");
    const auto* scaled = std::get_if<mydesk::document::LineEntity>(
        &session.document().entities().front().geometry);
    expect(scaled != nullptr, "scaled line");
    expect(scaled->end.nearlyEqual({0, 25}, 1e-6), "scaled end");
}

void test_ortho_mode() {
    mydesk::commands::Session session;
    session.modes().ortho = true;
    expect(session.executeLine("L 0,0").ok, "start line");
    auto r = session.providePoint({4, 3});
    expect(r.ok, r.message);
    expect(session.document().entities().size() == 1, "line completed");
    const auto* line =
        std::get_if<mydesk::document::LineEntity>(&session.document().entities().front().geometry);
    expect(line != nullptr, "line");
    expect(line->end.nearlyEqual({4, 0}, 1e-9), "ortho constrained");
}
