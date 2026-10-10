#include "../test_assert.hpp"

#include "mydesk/commands/session.hpp"
#include "mydesk/document/document.hpp"
#include "mydesk/exchange/native_format.hpp"

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

void test_layer_freeze_thaw_plot_roundtrip() {
    mydesk::commands::Session session;
    expect(session.executeLine("LAYER NEW Walls").ok, "create Walls");
    auto freezeCurrent = session.executeLine("LAYER FREEZE 0");
    expect(!freezeCurrent.ok, freezeCurrent.message);

    expect(session.executeLine("LAYER MAKE Walls").ok, "Walls current");
    expect(session.executeLine("L 0,0 10,0").ok, "line on Walls");
    const auto id = session.document().entities().front().id;
    expect(session.executeLine("LAYER SET 0").ok, "current back to 0");

    expect(session.executeLine("LAYER FREEZE Walls").ok, "freeze Walls");
    expect(session.document().findLayerByName("Walls")->frozen, "frozen flag");
    expect(session.document().hitTestTop({5, 0}, 1.0) == mydesk::document::kInvalidEntityId,
           "frozen layer is not selectable");
    expect(!session.document().isEntityVisible(session.document().entities().front()),
           "frozen layer is not visible");

    expect(session.executeLine("LAYER ON Walls").ok, "on does not thaw");
    expect(session.document().hitTestTop({5, 0}, 1.0) == mydesk::document::kInvalidEntityId,
           "frozen layer stays hidden while on");

    expect(session.executeLine("LAYER THAW Walls").ok, "thaw");
    expect(session.document().hitTestTop({5, 0}, 1.0) == id, "thawed layer is selectable");

    expect(session.executeLine("LAYER OFF Walls").ok, "off");
    expect(session.executeLine("LAYER THAW Walls").ok, "thaw while off");
    expect(!session.document().findLayerByName("Walls")->visible, "off is independent of thaw");
    expect(session.document().hitTestTop({5, 0}, 1.0) == mydesk::document::kInvalidEntityId,
           "off layer stays hidden");

    expect(session.executeLine("LAYER ON Walls").ok, "on");
    expect(session.executeLine("LAYER NOPLOT Walls").ok, "noplot");
    expect(!session.document().findLayerByName("Walls")->plottable, "plot flag cleared");
    expect(session.document().hitTestTop({5, 0}, 1.0) == id, "noplot entities stay editable");
    expect(session.executeLine("LAYER FREEZE Walls").ok, "freeze for save");

    const auto json = mydesk::exchange::serializeNative(session.document());
    mydesk::document::Document loaded;
    auto report = mydesk::exchange::deserializeNative(loaded, json);
    expect(report.ok, "native load");
    const auto* walls = loaded.findLayerByName("Walls");
    expect(walls != nullptr && walls->frozen && walls->visible && !walls->plottable,
           "freeze, visibility, and plot flag round-trip");
    expect(!loaded.isEntityVisible(loaded.entities().front()), "reloaded entity stays hidden");
}

void test_layer_duplicate_new_rejected() {
    mydesk::commands::Session session;
    expect(session.executeLine("LAYER NEW A").ok, "first NEW");
    auto dup = session.executeLine("LAYER NEW a");
    expect(!dup.ok, "case-insensitive duplicate rejected");
}
