#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mydesk::buildings {

using ElementId = std::uint64_t;
using TypeId = std::uint64_t;
using LevelId = std::uint64_t;

struct Parameter {
    std::string name;
    std::string value;
    std::string formula; // optional simple formula, e.g. "Height*Width"
};

struct Level {
    LevelId id{0};
    std::string name;
    double elevation{0.0};
};

struct WallType {
    TypeId id{0};
    std::string name;
    double thickness{0.2};
    double defaultHeight{3.0};
};

struct Wall {
    ElementId id{0};
    TypeId typeId{0};
    LevelId baseLevelId{0};
    double x0{0.0};
    double y0{0.0};
    double x1{0.0};
    double y1{0.0};
    double heightOverride{0.0}; // 0 => type default
    double baseOffset{0.0};
};

struct DoorType {
    TypeId id{0};
    std::string name;
    double width{0.9};
    double height{2.1};
};

struct Door {
    ElementId id{0};
    TypeId typeId{0};
    ElementId hostWallId{0};
    double offsetAlongWall{0.0}; // 0..1
};

struct WindowType {
    TypeId id{0};
    std::string name;
    double width{1.2};
    double height{1.5};
    double sillHeight{0.9};
};

struct Window {
    ElementId id{0};
    TypeId typeId{0};
    ElementId hostWallId{0};
    double offsetAlongWall{0.0}; // 0..1
};

struct Room {
    ElementId id{0};
    LevelId levelId{0};
    std::string name;
    std::vector<std::pair<double, double>> boundary;
};

struct ScheduleRow {
    std::string key;
    std::size_t count{0};
    double totalLength{0.0};
    double totalArea{0.0};
};

struct RoomScheduleRow {
    std::string roomName;
    std::string levelName;
    double area{0.0};
    double volume{0.0};
};

struct ProjectDocument {
    std::string name{"Project1"};
    std::vector<Level> levels;
    std::vector<WallType> wallTypes;
    std::vector<Wall> walls;
    std::vector<DoorType> doorTypes;
    std::vector<Door> doors;
    std::vector<WindowType> windowTypes;
    std::vector<Window> windows;
    std::vector<Room> rooms;

    LevelId addLevel(std::string name, double elevation);
    TypeId addWallType(std::string name, double thickness, double defaultHeight);
    TypeId addDoorType(std::string name, double width, double height);
    TypeId addWindowType(std::string name, double width, double height, double sillHeight = 0.9);
    ElementId addWall(TypeId typeId, LevelId levelId, double x0, double y0, double x1, double y1);
    ElementId addDoor(TypeId typeId, ElementId hostWallId, double offsetAlongWall);
    ElementId addWindow(TypeId typeId, ElementId hostWallId, double offsetAlongWall);
    ElementId addRoom(LevelId levelId, std::string name,
                      std::vector<std::pair<double, double>> boundary);

    bool setLevelElevation(LevelId id, double elevation);
    [[nodiscard]] const Level* findLevel(LevelId id) const;
    [[nodiscard]] const Wall* findWall(ElementId id) const;
    [[nodiscard]] const WallType* findWallType(TypeId id) const;
    [[nodiscard]] const DoorType* findDoorType(TypeId id) const;
    [[nodiscard]] const WindowType* findWindowType(TypeId id) const;
    [[nodiscard]] const Room* findRoom(ElementId id) const;

    [[nodiscard]] double wallLength(const Wall& wall) const;
    [[nodiscard]] double wallHeight(const Wall& wall) const;
    [[nodiscard]] double wallArea(const Wall& wall) const;
    [[nodiscard]] double wallNetArea(const Wall& wall) const;
    [[nodiscard]] double wallBaseElevation(const Wall& wall) const;
    [[nodiscard]] double wallTopElevation(const Wall& wall) const;

    [[nodiscard]] double roomArea(const Room& room) const;
    [[nodiscard]] double roomVolume(const Room& room, double height = 3.0) const;

    [[nodiscard]] std::vector<ScheduleRow> wallScheduleByType() const;
    [[nodiscard]] std::vector<RoomScheduleRow> roomScheduleByLevel() const;

    [[nodiscard]] std::string serializeNative() const;
    static ProjectDocument deserializeNative(std::string_view json, std::string* error = nullptr);
    [[nodiscard]] std::string exportIfc4() const;

private:
    LevelId nextLevelId_{1};
    TypeId nextTypeId_{1};
    ElementId nextElementId_{1};
};

} // namespace mydesk::buildings
