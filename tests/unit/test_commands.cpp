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
