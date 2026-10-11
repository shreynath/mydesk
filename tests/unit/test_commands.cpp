#include "../test_assert.hpp"

#include "mydesk/commands/session.hpp"

void test_command_line_alias() {
    mydesk::commands::Session session;
    auto r1 = session.executeLine("L 0,0 10,5");
    expect(r1.ok, r1.message);
    expect(session.document().entities().size() == 1, "line created via alias");
    auto r2 = session.executeLine("U");
    expect(r2.ok, r2.message);
    expect(session.document().entities().empty(), "undone");
}

void test_command_circle_radius() {
    mydesk::commands::Session session;
    auto r = session.executeLine("CIRCLE 0,0 5");
    expect(r.ok, r.message);
    expect(session.document().entities().size() == 1, "circle created");
    const auto* entity = &session.document().entities().front();
    const auto* circle = std::get_if<mydesk::document::CircleEntity>(&entity->geometry);
    expect(circle != nullptr, "is circle");
    expect(circle->radius == 5.0, "radius 5");
}

void test_draft_audit_purge_matchprop_qselect() {
    mydesk::commands::Session session;
    // AUDIT
    auto rAudit = session.executeLine("AUDIT");
    expect(rAudit.ok, rAudit.message);
    expect(rAudit.message.find("AUDIT") != std::string::npos, "audit message");

    // Create block and test PURGE
    session.executeLine("L 0,0 1,1");
    session.document().select(session.document().entities().front().id);
    session.executeLine("BLOCK BLK1 0,0");
    expect(session.document().blocks().size() == 1, "block defined");
    // While block is inserted (in-use), PURGE must not remove it
    session.executeLine("PURGE");
    expect(session.document().blocks().size() == 1, "in-use block preserved");
    // Remove the insert entity to make BLK1 unused
    session.document().removeEntity(session.document().entities().front().id);
    auto rPurge = session.executeLine("PURGE");
    expect(rPurge.ok, rPurge.message);
    expect(session.document().blocks().empty(), "unused block purged");

    // MATCHPROP
    auto id1 = session.document().addLine({0, 0}, {5, 0});
    auto id2 = session.document().addLine({10, 0}, {15, 0});
    auto layRed = session.document().addLayer("RED_LAYER");
    session.document().setLayerColor(layRed, 0xFFFF0000);
    session.document().setEntityLayer(id1, layRed);
    session.document().setEntityLinetype(id1, mydesk::document::Linetype::Dashed);

    session.document().clearSelection();
    session.document().select(id1);
    session.document().select(id2);
    auto rMatch = session.executeLine("MATCHPROP");
    expect(rMatch.ok, rMatch.message);
    const auto* e2 = session.document().findEntity(id2);
    expect(e2 != nullptr && e2->layerId == layRed, "layer copied");
    expect(e2->linetype == mydesk::document::Linetype::Dashed, "linetype copied");

    // QSELECT
    session.document().addCircle({50, 50}, 10.0);
    session.document().clearSelection();
    auto rQsel = session.executeLine("QSELECT LINE");
    expect(rQsel.ok, rQsel.message);
    expect(session.document().selection().size() == 2, "only lines selected");
}

