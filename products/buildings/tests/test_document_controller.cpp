#include "mydesk/buildings/document_controller.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_buildings_document_controller() {
    mydesk::buildings::DocumentController controller;
    expect(controller.document().levels.size() == 1, "new project has an authoring level");
    const auto level = controller.document().levels.front().id;
    const auto wall = controller.createWall(level, 0.0, 0.0, 8.0, 0.0);
    expect(wall != 0 && controller.isModified(), "interactive wall creation changes model");
    const auto door = controller.createDoor(wall, 0.25);
    const auto window = controller.createWindow(wall, 0.75);
    expect(door != 0 && window != 0, "hosted openings created through controller");

    expect(controller.setOpeningOffset(mydesk::buildings::SelectionKind::Door, door, 0.4),
           "opening edited");
    expect(std::abs(controller.document().doors.front().offsetAlongWall - 0.4) < 1e-9,
           "opening model updated");
    expect(controller.undo(), "undo opening edit");
    expect(std::abs(controller.document().doors.front().offsetAlongWall - 0.25) < 1e-9,
           "undo restores serialized BIM graph");
    expect(controller.redo(), "redo opening edit");
    expect(std::abs(controller.document().doors.front().offsetAlongWall - 0.4) < 1e-9,
           "redo restores BIM graph");

    const auto root = std::filesystem::temp_directory_path() / "mydesk-buildings-controller-test";
    std::filesystem::create_directories(root);
    const auto native = root / "office.mydesk-bim";
    const auto ifc = root / "office.ifc";
    std::string error;
    expect(controller.saveAs(native, &error), "native save succeeds");
    expect(!controller.isModified(), "successful save clears modified state");
    expect(controller.exportIfc(ifc, &error), "IFC export succeeds");

    mydesk::buildings::DocumentController reopened;
    expect(reopened.open(native, &error), "native reopen succeeds");
    expect(reopened.document().walls.size() == 1 && reopened.document().doors.size() == 1 &&
               reopened.document().windows.size() == 1,
           "editable model round-trips through native file");
    std::ifstream ifcInput(ifc);
    const std::string ifcText((std::istreambuf_iterator<char>(ifcInput)), {});
    expect(ifcText.find("IFCWALLSTANDARDCASE") != std::string::npos,
           "IFC export contains authored wall");
    std::filesystem::remove_all(root);
}

} // namespace

int main() {
    try {
        test_buildings_document_controller();
        std::cout << "MyDesk Buildings controller tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
