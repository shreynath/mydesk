#include "../test_assert.hpp"

#include "mydesk/document/document.hpp"
#include "mydesk/exchange/dxf_reader.hpp"
#include "mydesk/exchange/dxf_writer.hpp"

#include <filesystem>

void test_dxf_line_circle_pline_roundtrip() {
    mydesk::document::Document doc;
    doc.addLine({0, 0}, {10, 0});
    doc.addCircle({5, 5}, 2);
    doc.addPolyline({{0, 0}, {1, 0}, {1, 1}}, true);

    const auto dir = std::filesystem::temp_directory_path() / "mydesk_tests";
    std::filesystem::create_directories(dir);
    const auto path = dir / "roundtrip.dxf";

    auto exp = mydesk::exchange::exportDxfR12(doc, path);
    expect(exp.ok, "export ok");

    mydesk::document::Document loaded;
    auto imp = mydesk::exchange::importDxf(loaded, path);
    expect(imp.ok, "import ok");
    expect(loaded.entities().size() == 3, "three entities");
}
