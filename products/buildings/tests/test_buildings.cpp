#include "mydesk/buildings/project_document.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

void expect(bool cond, const std::string& msg) {
    if (!cond) {
        throw std::runtime_error(msg);
    }
}

void test_multistory_schedule_ifc_roundtrip() {
    mydesk::buildings::ProjectDocument project;
    const auto l1 = project.addLevel("L1", 0.0);
    const auto l2 = project.addLevel("L2", 3.2);
    const auto wt = project.addWallType("WallA", 0.2, 3.0);
    const auto dt = project.addDoorType("DoorA", 0.9, 2.1);
    const auto w1 = project.addWall(wt, l1, 0, 0, 5, 0);
    project.addWall(wt, l2, 0, 0, 5, 0);
    project.addDoor(dt, w1, 0.25);

    expect(std::abs(project.wallBaseElevation(*project.findWall(w1)) - 0.0) < 1e-9, "base 0");
    expect(std::abs(project.wallTopElevation(*project.findWall(w1)) - 3.0) < 1e-9, "top 3");

    expect(project.setLevelElevation(l1, 1.0), "move level");
    expect(std::abs(project.wallBaseElevation(*project.findWall(w1)) - 1.0) < 1e-9,
           "wall follows level");
    expect(std::abs(project.wallTopElevation(*project.findWall(w1)) - 4.0) < 1e-9, "top follows");

    const auto schedule = project.wallScheduleByType();
    expect(schedule.size() == 1, "one type row");
    expect(schedule.front().count == 2, "two walls");
    expect(std::abs(schedule.front().totalLength - 10.0) < 1e-9, "total length");

    const auto json = project.serializeNative();
    std::string error;
    auto loaded = mydesk::buildings::ProjectDocument::deserializeNative(json, &error);
    expect(loaded.levels.size() == 2, "levels roundtrip");
    expect(loaded.walls.size() == 2, "walls roundtrip");
    expect(loaded.doors.size() == 1, "doors roundtrip");
    expect(loaded.doors.front().hostWallId == w1, "host preserved");

    const auto ifc = loaded.exportIfc4();
    expect(ifc.find("IFCPROJECT") != std::string::npos, "ifc project");
    expect(ifc.find("IFCBUILDINGSTOREY") != std::string::npos, "ifc storey");
    expect(ifc.find("IFCWALLSTANDARDCASE") != std::string::npos, "ifc wall");
    expect(ifc.find("IFCDOOR") != std::string::npos, "ifc door");
    expect(ifc.find("IFCRELCONTAINEDINSPATIALSTRUCTURE") != std::string::npos, "spatial rel");
    const auto wallCount = std::count(ifc.begin(), ifc.end(), '\n'); // non-zero file
    expect(wallCount > 20, "ifc substantive");
    expect(std::count(ifc.begin(), ifc.end(), '#') >= 10, "many ifc entities");
}

void test_windows_rooms_and_net_area() {
    mydesk::buildings::ProjectDocument project;
    const auto l1 = project.addLevel("Ground", 0.0);
    const auto wt = project.addWallType("ExteriorWall", 0.25, 3.0);
    const auto dt = project.addDoorType("EntryDoor", 1.0, 2.0); // 2.0 m2
    const auto winT = project.addWindowType("StdWindow", 1.5, 1.2, 0.9); // 1.8 m2

    const auto w1 = project.addWall(wt, l1, 0.0, 0.0, 10.0, 0.0); // 10m length * 3m = 30 m2 gross
    project.addDoor(dt, w1, 0.2);
    project.addWindow(winT, w1, 0.7);

    const auto* wallPtr = project.findWall(w1);
    expect(wallPtr != nullptr, "wall found");
    const double gross = project.wallArea(*wallPtr);
    expect(std::abs(gross - 30.0) < 1e-9, "gross area 30");

    const double net = project.wallNetArea(*wallPtr);
    // deductions: door 2.0 m2 + window 1.8 m2 = 3.8 m2; net = 26.2 m2
    expect(std::abs(net - 26.2) < 1e-9, "net area 26.2 after door and window opening deduction");

    // Add room with 4 corners (5m x 4m rectangle = 20 m2)
    std::vector<std::pair<double, double>> boundary = {
        {0.0, 0.0}, {5.0, 0.0}, {5.0, 4.0}, {0.0, 4.0}
    };
    const auto r1 = project.addRoom(l1, "Living Room", boundary);
    const auto* roomPtr = project.findRoom(r1);
    expect(roomPtr != nullptr, "room found");
    const double rArea = project.roomArea(*roomPtr);
    expect(std::abs(rArea - 20.0) < 1e-9, "shoelace room area 20");
    const double rVol = project.roomVolume(*roomPtr, 3.0);
    expect(std::abs(rVol - 60.0) < 1e-9, "room volume 60");

    const auto roomSched = project.roomScheduleByLevel();
    expect(roomSched.size() == 1, "one room in schedule");
    expect(roomSched.front().roomName == "Living Room", "room name in schedule");
    expect(std::abs(roomSched.front().area - 20.0) < 1e-9, "scheduled room area");

    // Serialization roundtrip
    const auto json = project.serializeNative();
    std::string err;
    auto reloaded = mydesk::buildings::ProjectDocument::deserializeNative(json, &err);
    expect(reloaded.windowTypes.size() == 1, "windowTypes reloaded");
    expect(reloaded.windows.size() == 1, "windows reloaded");
    expect(reloaded.windows.front().hostWallId == w1, "window host preserved");
    expect(reloaded.rooms.size() == 1, "rooms reloaded");
    expect(reloaded.rooms.front().boundary.size() == 4, "room boundary reloaded");

    // IFC4 export check
    const auto ifc = reloaded.exportIfc4();
    expect(ifc.find("IFCWINDOW") != std::string::npos, "IFCWINDOW present in IFC4");
    expect(ifc.find("IFCSPACE") != std::string::npos, "IFCSPACE present in IFC4");
    expect(ifc.find("GrossFloorArea") != std::string::npos, "room gross floor area in IFC4");
    expect(ifc.find("NetArea") != std::string::npos, "wall net area property in IFC4");
}

} // namespace

int main() {
    try {
        const char* kRegistered = "test_multistory_schedule_ifc_roundtrip";
        (void)kRegistered;
        test_multistory_schedule_ifc_roundtrip();
        std::cout << "[PASS] test_multistory_schedule_ifc_roundtrip\n";

        const char* kRegistered2 = "test_windows_rooms_and_net_area";
        (void)kRegistered2;
        test_windows_rooms_and_net_area();
        std::cout << "[PASS] test_windows_rooms_and_net_area\n";

        std::cout << "2 passed, 0 failed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cout << "[FAIL] " << ex.what() << "\n0 passed, 1 failed\n";
        return EXIT_FAILURE;
    }
}
