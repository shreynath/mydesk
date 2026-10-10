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

} // namespace

int main() {
    try {
        const char* kRegistered = "test_multistory_schedule_ifc_roundtrip";
        (void)kRegistered;
        test_multistory_schedule_ifc_roundtrip();
        std::cout << "[PASS] test_multistory_schedule_ifc_roundtrip\n1 passed, 0 failed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cout << "[FAIL] " << ex.what() << "\n0 passed, 1 failed\n";
        return EXIT_FAILURE;
    }
}
