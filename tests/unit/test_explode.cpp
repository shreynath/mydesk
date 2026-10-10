#include "../test_assert.hpp"

#include "mydesk/commands/session.hpp"
#include "mydesk/document/entity.hpp"

#include <cmath>

void test_explode_insert_and_polyline() {
    mydesk::commands::Session session;
    expect(session.executeLine("L 0,0 10,0").ok, "line");
    expect(session.executeLine("C 5,5 2").ok, "circle");
    session.document().select(session.document().entities()[0].id);
    session.document().select(session.document().entities()[1].id);
    auto block = session.executeLine("BLOCK Widget 0,0");
    expect(block.ok, block.message);
    auto inserted = session.executeLine("INSERT Widget 20,10 2 90");
    expect(inserted.ok, inserted.message);
    expect(session.document().entities().size() == 2, "two inserts before explode");

    const auto second = session.document().entities()[1].id;
    session.document().clearSelection();
    session.document().select(second);
    auto exploded = session.executeLine("EXPLODE");
    expect(exploded.ok, exploded.message);
    expect(session.document().blocks().size() == 1, "block definition kept");

    int inserts = 0;
    const mydesk::document::LineEntity* line = nullptr;
    const mydesk::document::CircleEntity* circle = nullptr;
    for (const auto& entity : session.document().entities()) {
        if (entity.kind() == mydesk::document::EntityKind::Insert) {
            ++inserts;
        } else if (const auto* lineEntity =
                       std::get_if<mydesk::document::LineEntity>(&entity.geometry)) {
            line = lineEntity;
        } else if (const auto* circleEntity =
                       std::get_if<mydesk::document::CircleEntity>(&entity.geometry)) {
            circle = circleEntity;
        }
    }
    expect(inserts == 1, "original insert remains");
    expect(line != nullptr, "exploded line");
    expect(circle != nullptr, "exploded circle");
    expect(line->start.nearlyEqual({20, 10}, 1e-6), "rotated line start");
    expect(line->end.nearlyEqual({20, 30}, 1e-6), "rotated line end");
    expect(circle->center.nearlyEqual({10, 20}, 1e-6), "rotated circle center");
    expect(std::abs(circle->radius - 4.0) < 1e-6, "scaled radius");
    expect(session.document().selection().size() == 2, "exploded geometry selected");

    expect(session.executeLine("U").ok, "undo explode");
    expect(session.document().entities().size() == 2, "insert restored");
    expect(session.document().entities()[1].kind() == mydesk::document::EntityKind::Insert,
           "second entity is insert again");

    expect(session.executeLine("RECTANG 0,0 10,10").ok, "rectangle");
    const auto rectId = session.document().entities().back().id;
    session.document().clearSelection();
    session.document().select(rectId);
    auto edges = session.executeLine("X");
    expect(edges.ok, edges.message);

    int lines = 0;
    bool polylineRemains = false;
    mydesk::document::EntityId lineId = mydesk::document::kInvalidEntityId;
    for (const auto& entity : session.document().entities()) {
        if (entity.kind() == mydesk::document::EntityKind::Line) {
            ++lines;
            lineId = entity.id;
        }
        if (entity.kind() == mydesk::document::EntityKind::Polyline) {
            polylineRemains = true;
        }
    }
    expect(!polylineRemains, "polyline replaced");
    expect(lines == 4, "closed rectangle becomes four lines");

    session.document().clearSelection();
    session.document().select(lineId);
    const auto before = session.document().entities().size();
    auto rejected = session.executeLine("EXPLODE");
    expect(!rejected.ok, rejected.message);
    expect(session.document().entities().size() == before, "line explode changes nothing");
}
