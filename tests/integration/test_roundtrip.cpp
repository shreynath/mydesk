#include "../test_assert.hpp"

#include "mydesk/commands/session.hpp"
#include "mydesk/exchange/dxf_writer.hpp"
#include "mydesk/exchange/native_format.hpp"

#include <filesystem>

void test_native_roundtrip() {
    mydesk::commands::Session session;
    expect(session.executeLine("LINE 0,0 20,10").ok, "line");
    expect(session.executeLine("CIRCLE 5,5 3").ok, "circle");

    const auto dir = std::filesystem::temp_directory_path() / "mydesk_tests";
    std::filesystem::create_directories(dir);
    const auto path = dir / "roundtrip.mydesk";

    auto saveReport = mydesk::exchange::saveNative(session.document(), path);
    expect(saveReport.ok, "save ok");
    expect(saveReport.entityCount == 2, "saved 2");

    mydesk::document::Document loaded;
    auto loadReport = mydesk::exchange::loadNative(loaded, path);
    expect(loadReport.ok, "load ok");
    expect(loaded.entities().size() == 2, "loaded 2");

    const auto* line = std::get_if<mydesk::document::LineEntity>(&loaded.entities()[0].geometry);
    const auto* circle = std::get_if<mydesk::document::CircleEntity>(&loaded.entities()[1].geometry);
    expect(line != nullptr, "first is line");
    expect(circle != nullptr, "second is circle");
    expect(line->end.nearlyEqual({20, 10}), "line end preserved");
    expect(circle->radius == 3.0, "radius preserved");
}

void test_dxf_export_smoke() {
    mydesk::document::Document doc;
    doc.addLine({0, 0}, {1, 1});
    doc.addCircle({2, 2}, 4);
    const auto path = std::filesystem::temp_directory_path() / "mydesk_tests" / "smoke.dxf";
    std::filesystem::create_directories(path.parent_path());
    auto report = mydesk::exchange::exportDxfR12(doc, path);
    expect(report.ok, "dxf export ok");
    expect(std::filesystem::file_size(path) > 50, "dxf non-empty");
}
