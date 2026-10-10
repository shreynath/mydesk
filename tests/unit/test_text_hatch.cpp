#include "../test_assert.hpp"

#include "mydesk/commands/session.hpp"

void test_text_and_hatch() {
    mydesk::commands::Session session;
    auto text = session.executeLine("TEXT 10,10 2.5 Hello World");
    expect(text.ok, text.message);
    expect(session.document().entities().front().kind() == mydesk::document::EntityKind::Text,
           "text kind");
    const auto* te =
        std::get_if<mydesk::document::TextEntity>(&session.document().entities().front().geometry);
    expect(te != nullptr, "text entity");
    expect(te->contents == "Hello World", "contents");

    expect(session.executeLine("REC 0,0 20,20").ok, "rect");
    const auto rectId = session.document().entities().back().id;
    session.document().clearSelection();
    session.document().select(rectId);
    auto hatch = session.executeLine("HATCH");
    expect(hatch.ok, hatch.message);
    bool foundHatch = false;
    for (const auto& e : session.document().entities()) {
        if (e.kind() == mydesk::document::EntityKind::Hatch) {
            foundHatch = true;
            break;
        }
    }
    expect(foundHatch, "hatch created");
}
