#include "../test_assert.hpp"

#include "mydesk/commands/session.hpp"
#include "mydesk/document/entity.hpp"

void test_relative_and_polar_coordinates() {
    mydesk::commands::Session session;
    auto line = session.executeLine("L 0,0 @10,0");
    expect(line.ok, line.message);
    const auto* first =
        std::get_if<mydesk::document::LineEntity>(&session.document().entities()[0].geometry);
    expect(first != nullptr, "first line");
    expect(first->start.nearlyEqual({0, 0}), "absolute start");
    expect(first->end.nearlyEqual({10, 0}), "relative cartesian end");

    auto second = session.executeLine("L @0,5 @10<90");
    expect(second.ok, second.message);
    const auto* chained =
        std::get_if<mydesk::document::LineEntity>(&session.document().entities()[1].geometry);
    expect(chained != nullptr, "chained line");
    expect(chained->start.nearlyEqual({10, 5}, 1e-6), "chained relative start");
    expect(chained->end.nearlyEqual({10, 15}, 1e-6), "relative polar end");

    auto polar = session.executeLine("L 10<0 10<90");
    expect(polar.ok, polar.message);
    const auto* absolute =
        std::get_if<mydesk::document::LineEntity>(&session.document().entities()[2].geometry);
    expect(absolute != nullptr, "polar line");
    expect(absolute->start.nearlyEqual({10, 0}, 1e-6), "absolute polar start");
    expect(absolute->end.nearlyEqual({0, 10}, 1e-6), "absolute polar end");

    auto forced = session.executeLine("L #1,2 #3,4");
    expect(forced.ok, forced.message);
    const auto* hashed =
        std::get_if<mydesk::document::LineEntity>(&session.document().entities()[3].geometry);
    expect(hashed != nullptr, "forced absolute line");
    expect(hashed->start.nearlyEqual({1, 2}), "hash start ignores last point");
    expect(hashed->end.nearlyEqual({3, 4}), "hash end");

    auto rejected = session.executeLine("L 1,2,3 4,5");
    expect(!rejected.ok, "3D coordinate rejected");
    expect(session.document().entities().size() == 4, "rejected coordinate adds nothing");

    expect(session.executeLine("NEW").ok, "new drawing");
    auto missing = session.executeLine("L @1,0 2,0");
    expect(!missing.ok, missing.message);
    expect(session.document().entities().empty(), "relative without LASTPOINT creates nothing");

    expect(session.executeLine("LINE").ok, "start interactive line");
    expect(session.hasActiveCommand(), "line stays active");
    auto start = session.executeLine("5,5");
    expect(start.ok, start.message);
    auto end = session.executeLine("@4,-1");
    expect(end.ok, end.message);
    expect(!session.hasActiveCommand(), "line completed");
    const auto* interactive =
        std::get_if<mydesk::document::LineEntity>(&session.document().entities().front().geometry);
    expect(interactive != nullptr, "interactive line");
    expect(interactive->start.nearlyEqual({5, 5}, 1e-6), "interactive start");
    expect(interactive->end.nearlyEqual({9, 4}, 1e-6), "interactive relative end");
}
