#include "../test_assert.hpp"

#include "mydesk/commands/session.hpp"
#include "mydesk/exchange/native_format.hpp"

void test_block_and_insert() {
    mydesk::commands::Session session;
    expect(session.executeLine("L 0,0 10,0").ok, "line");
    expect(session.executeLine("C 5,5 2").ok, "circle");
    session.document().select(session.document().entities()[0].id);
    session.document().select(session.document().entities()[1].id);
    auto block = session.executeLine("BLOCK Widget 0,0");
    expect(block.ok, block.message);
    expect(session.document().blocks().size() == 1, "one block def");
    expect(session.document().entities().size() == 1, "replaced by insert");
    expect(session.document().entities().front().kind() == mydesk::document::EntityKind::Insert,
           "is insert");

    auto again = session.executeLine("INSERT Widget 50,0 2 0");
    expect(again.ok, again.message);
    expect(session.document().entities().size() == 2, "two inserts");

    const auto json = mydesk::exchange::serializeNative(session.document());
    mydesk::document::Document loaded;
    auto report = mydesk::exchange::deserializeNative(loaded, json);
    expect(report.ok, "roundtrip ok");
    expect(loaded.blocks().size() == 1, "block restored");
    expect(loaded.entities().size() == 2, "inserts restored");
}
