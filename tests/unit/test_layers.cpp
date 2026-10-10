#include "../test_assert.hpp"

#include "mydesk/commands/session.hpp"
#include "mydesk/document/document.hpp"

void test_layer_command_new_set_off() {
    mydesk::commands::Session session;
    auto created = session.executeLine("LAYER NEW Walls");
    expect(created.ok, created.message);
    expect(session.document().layers().size() == 2, "default + Walls");

    auto make = session.executeLine("LA MAKE Doors");
    expect(make.ok, make.message);
    const auto* doors = session.document().findLayerByName("Doors");
    expect(doors != nullptr, "Doors exists");
    expect(session.document().activeLayerId() == doors->id, "Doors current");

    auto line = session.executeLine("L 0,0 10,0");
    expect(line.ok, line.message);
    expect(session.document().entities().front().layerId == doors->id, "entity on Doors");

    auto set0 = session.executeLine("LAYER SET 0");
    expect(set0.ok, set0.message);
    auto off = session.executeLine("LAYER OFF Doors");
    expect(off.ok, off.message);
    expect(!session.document().findLayerByName("Doors")->visible, "Doors off");

    const auto id = session.document().entities().front().id;
    const auto hit = session.document().hitTestTop({5, 0}, 1.0);
    expect(hit == mydesk::document::kInvalidEntityId, "hidden layer not hit-testable");
    (void)id;

    auto on = session.executeLine("LAYER ON Doors");
    expect(on.ok, on.message);
    auto lock = session.executeLine("LAYER LOCK Doors");
    expect(lock.ok, lock.message);
    expect(session.document().findLayerByName("Doors")->locked, "locked");
    expect(session.document().hitTestTop({5, 0}, 1.0) == mydesk::document::kInvalidEntityId,
           "locked layer not selectable");

    auto unlock = session.executeLine("LAYER UNLOCK Doors");
    expect(unlock.ok, unlock.message);
    expect(session.document().hitTestTop({5, 0}, 1.0) == id, "unlocked selectable");

    auto color = session.executeLine("LAYER COLOR Doors FF0000");
    expect(color.ok, color.message);
    expect((session.document().findLayerByName("Doors")->colorArgb & 0xFFFFFFu) == 0xFF0000u,
           "red color");

    auto list = session.executeLine("LAYER ?");
    expect(list.ok, list.message);
    expect(list.message.find("Doors") != std::string::npos, "list mentions Doors");
}

void test_layer_duplicate_new_rejected() {
    mydesk::commands::Session session;
    expect(session.executeLine("LAYER NEW A").ok, "first NEW");
    auto dup = session.executeLine("LAYER NEW a");
    expect(!dup.ok, "case-insensitive duplicate rejected");
}
