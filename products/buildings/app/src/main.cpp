#include "mydesk/buildings/project_document.hpp"

#include <iostream>

int main() {
    mydesk::buildings::ProjectDocument project;
    project.name = "Office";
    const auto l1 = project.addLevel("Level 1", 0.0);
    const auto l2 = project.addLevel("Level 2", 3.5);
    const auto wt = project.addWallType("Generic - 200mm", 0.2, 3.0);
    const auto dt = project.addDoorType("Single 900", 0.9, 2.1);
    const auto w1 = project.addWall(wt, l1, 0, 0, 10, 0);
    project.addWall(wt, l1, 10, 0, 10, 8);
    project.addWall(wt, l2, 0, 0, 10, 0);
    project.addDoor(dt, w1, 0.3);

    const auto schedule = project.wallScheduleByType();
    std::cout << "MyDesk Buildings\nProject: " << project.name << '\n';
    std::cout << "Levels: " << project.levels.size() << " Walls: " << project.walls.size()
              << " Doors: " << project.doors.size() << '\n';
    for (const auto& row : schedule) {
        std::cout << "Schedule " << row.key << " count=" << row.count
                  << " length=" << row.totalLength << " area=" << row.totalArea << '\n';
    }
    return 0;
}
