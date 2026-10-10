#include "mydesk/buildings/project_document.hpp"

#include <cmath>
#include <sstream>
#include <unordered_map>

namespace mydesk::buildings {

namespace {

std::string escape(std::string_view s) {
    std::string out;
    for (char c : s) {
        if (c == '\'' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

} // namespace

LevelId ProjectDocument::addLevel(std::string levelName, double elevation) {
    Level level;
    level.id = nextLevelId_++;
    level.name = std::move(levelName);
    level.elevation = elevation;
    levels.push_back(level);
    return level.id;
}

TypeId ProjectDocument::addWallType(std::string typeName, double thickness, double defaultHeight) {
    WallType type;
    type.id = nextTypeId_++;
    type.name = std::move(typeName);
    type.thickness = thickness;
    type.defaultHeight = defaultHeight;
    wallTypes.push_back(type);
    return type.id;
}

TypeId ProjectDocument::addDoorType(std::string typeName, double width, double height) {
    DoorType type;
    type.id = nextTypeId_++;
    type.name = std::move(typeName);
    type.width = width;
    type.height = height;
    doorTypes.push_back(type);
    return type.id;
}

TypeId ProjectDocument::addWindowType(std::string typeName, double width, double height,
                                       double sillHeight) {
    WindowType type;
    type.id = nextTypeId_++;
    type.name = std::move(typeName);
    type.width = width;
    type.height = height;
    type.sillHeight = sillHeight;
    windowTypes.push_back(type);
    return type.id;
}

ElementId ProjectDocument::addWall(TypeId typeId, LevelId levelId, double x0, double y0, double x1,
                                   double y1) {
    Wall wall;
    wall.id = nextElementId_++;
    wall.typeId = typeId;
    wall.baseLevelId = levelId;
    wall.x0 = x0;
    wall.y0 = y0;
    wall.x1 = x1;
    wall.y1 = y1;
    walls.push_back(wall);
    return wall.id;
}

ElementId ProjectDocument::addDoor(TypeId typeId, ElementId hostWallId, double offsetAlongWall) {
    Door door;
    door.id = nextElementId_++;
    door.typeId = typeId;
    door.hostWallId = hostWallId;
    door.offsetAlongWall = offsetAlongWall;
    doors.push_back(door);
    return door.id;
}

ElementId ProjectDocument::addWindow(TypeId typeId, ElementId hostWallId, double offsetAlongWall) {
    Window win;
    win.id = nextElementId_++;
    win.typeId = typeId;
    win.hostWallId = hostWallId;
    win.offsetAlongWall = offsetAlongWall;
    windows.push_back(win);
    return win.id;
}

ElementId ProjectDocument::addRoom(LevelId levelId, std::string roomName,
                                   std::vector<std::pair<double, double>> boundary) {
    Room room;
    room.id = nextElementId_++;
    room.levelId = levelId;
    room.name = std::move(roomName);
    room.boundary = std::move(boundary);
    rooms.push_back(room);
    return room.id;
}

bool ProjectDocument::setLevelElevation(LevelId id, double elevation) {
    for (auto& level : levels) {
        if (level.id == id) {
            level.elevation = elevation;
            return true;
        }
    }
    return false;
}

const Level* ProjectDocument::findLevel(LevelId id) const {
    for (const auto& level : levels) {
        if (level.id == id) {
            return &level;
        }
    }
    return nullptr;
}

const Wall* ProjectDocument::findWall(ElementId id) const {
    for (const auto& wall : walls) {
        if (wall.id == id) {
            return &wall;
        }
    }
    return nullptr;
}

const WallType* ProjectDocument::findWallType(TypeId id) const {
    for (const auto& type : wallTypes) {
        if (type.id == id) {
            return &type;
        }
    }
    return nullptr;
}

const DoorType* ProjectDocument::findDoorType(TypeId id) const {
    for (const auto& type : doorTypes) {
        if (type.id == id) {
            return &type;
        }
    }
    return nullptr;
}

const WindowType* ProjectDocument::findWindowType(TypeId id) const {
    for (const auto& type : windowTypes) {
        if (type.id == id) {
            return &type;
        }
    }
    return nullptr;
}

const Room* ProjectDocument::findRoom(ElementId id) const {
    for (const auto& room : rooms) {
        if (room.id == id) {
            return &room;
        }
    }
    return nullptr;
}

double ProjectDocument::wallLength(const Wall& wall) const {
    return std::hypot(wall.x1 - wall.x0, wall.y1 - wall.y0);
}

double ProjectDocument::wallHeight(const Wall& wall) const {
    if (wall.heightOverride > 0.0) {
        return wall.heightOverride;
    }
    const auto* type = findWallType(wall.typeId);
    return type ? type->defaultHeight : 0.0;
}

double ProjectDocument::wallArea(const Wall& wall) const {
    return wallLength(wall) * wallHeight(wall);
}

double ProjectDocument::wallNetArea(const Wall& wall) const {
    double gross = wallArea(wall);
    double deductions = 0.0;
    for (const auto& door : doors) {
        if (door.hostWallId == wall.id) {
            const auto* dt = findDoorType(door.typeId);
            if (dt) {
                deductions += (dt->width * dt->height);
            }
        }
    }
    for (const auto& win : windows) {
        if (win.hostWallId == wall.id) {
            const auto* wt = findWindowType(win.typeId);
            if (wt) {
                deductions += (wt->width * wt->height);
            }
        }
    }
    return std::max(0.0, gross - deductions);
}

double ProjectDocument::wallBaseElevation(const Wall& wall) const {
    const auto* level = findLevel(wall.baseLevelId);
    return (level ? level->elevation : 0.0) + wall.baseOffset;
}

double ProjectDocument::wallTopElevation(const Wall& wall) const {
    return wallBaseElevation(wall) + wallHeight(wall);
}

double ProjectDocument::roomArea(const Room& room) const {
    if (room.boundary.size() < 3) {
        return 0.0;
    }
    double sum = 0.0;
    const std::size_t n = room.boundary.size();
    for (std::size_t i = 0; i < n; ++i) {
        const auto& [x1, y1] = room.boundary[i];
        const auto& [x2, y2] = room.boundary[(i + 1) % n];
        sum += (x1 * y2 - x2 * y1);
    }
    return 0.5 * std::abs(sum);
}

double ProjectDocument::roomVolume(const Room& room, double height) const {
    return roomArea(room) * (height > 0.0 ? height : 3.0);
}

std::vector<ScheduleRow> ProjectDocument::wallScheduleByType() const {
    std::unordered_map<TypeId, ScheduleRow> rows;
    for (const auto& wall : walls) {
        const auto* type = findWallType(wall.typeId);
        auto& row = rows[wall.typeId];
        row.key = type ? type->name : "Unknown";
        ++row.count;
        row.totalLength += wallLength(wall);
        row.totalArea += wallArea(wall);
    }
    std::vector<ScheduleRow> out;
    out.reserve(rows.size());
    for (auto& [_, row] : rows) {
        out.push_back(std::move(row));
    }
    return out;
}

std::vector<RoomScheduleRow> ProjectDocument::roomScheduleByLevel() const {
    std::vector<RoomScheduleRow> out;
    out.reserve(rooms.size());
    for (const auto& room : rooms) {
        const auto* level = findLevel(room.levelId);
        RoomScheduleRow row;
        row.roomName = room.name;
        row.levelName = level ? level->name : "Unknown";
        row.area = roomArea(room);
        row.volume = roomVolume(room);
        out.push_back(std::move(row));
    }
    return out;
}

std::string ProjectDocument::serializeNative() const {
    std::ostringstream oss;
    oss << "{\"format\":\"mydesk-buildings\",\"schemaVersion\":\"1\",\"name\":\"" << name << "\",";
    oss << "\"levels\":[";
    for (std::size_t i = 0; i < levels.size(); ++i) {
        const auto& l = levels[i];
        oss << "{\"id\":" << l.id << ",\"name\":\"" << l.name << "\",\"elevation\":" << l.elevation
            << "}";
        if (i + 1 < levels.size()) {
            oss << ",";
        }
    }
    oss << "],\"wallTypes\":[";
    for (std::size_t i = 0; i < wallTypes.size(); ++i) {
        const auto& t = wallTypes[i];
        oss << "{\"id\":" << t.id << ",\"name\":\"" << t.name << "\",\"thickness\":" << t.thickness
            << ",\"defaultHeight\":" << t.defaultHeight << "}";
        if (i + 1 < wallTypes.size()) {
            oss << ",";
        }
    }
    oss << "],\"walls\":[";
    for (std::size_t i = 0; i < walls.size(); ++i) {
        const auto& w = walls[i];
        oss << "{\"id\":" << w.id << ",\"typeId\":" << w.typeId << ",\"baseLevelId\":"
            << w.baseLevelId << ",\"x0\":" << w.x0 << ",\"y0\":" << w.y0 << ",\"x1\":" << w.x1
            << ",\"y1\":" << w.y1 << "}";
        if (i + 1 < walls.size()) {
            oss << ",";
        }
    }
    oss << "],\"doors\":[";
    for (std::size_t i = 0; i < doors.size(); ++i) {
        const auto& d = doors[i];
        oss << "{\"id\":" << d.id << ",\"typeId\":" << d.typeId << ",\"hostWallId\":"
            << d.hostWallId << ",\"offsetAlongWall\":" << d.offsetAlongWall << "}";
        if (i + 1 < doors.size()) {
            oss << ",";
        }
    }
    oss << "],\"windowTypes\":[";
    for (std::size_t i = 0; i < windowTypes.size(); ++i) {
        const auto& wt = windowTypes[i];
        oss << "{\"id\":" << wt.id << ",\"name\":\"" << wt.name << "\",\"width\":" << wt.width
            << ",\"height\":" << wt.height << ",\"sillHeight\":" << wt.sillHeight << "}";
        if (i + 1 < windowTypes.size()) {
            oss << ",";
        }
    }
    oss << "],\"windows\":[";
    for (std::size_t i = 0; i < windows.size(); ++i) {
        const auto& win = windows[i];
        oss << "{\"id\":" << win.id << ",\"typeId\":" << win.typeId << ",\"hostWallId\":"
            << win.hostWallId << ",\"offsetAlongWall\":" << win.offsetAlongWall << "}";
        if (i + 1 < windows.size()) {
            oss << ",";
        }
    }
    oss << "],\"rooms\":[";
    for (std::size_t i = 0; i < rooms.size(); ++i) {
        const auto& rm = rooms[i];
        oss << "{\"id\":" << rm.id << ",\"levelId\":" << rm.levelId << ",\"name\":\"" << rm.name
            << "\",\"boundary\":[";
        for (std::size_t b = 0; b < rm.boundary.size(); ++b) {
            oss << "{\"x\":" << rm.boundary[b].first << ",\"y\":" << rm.boundary[b].second << "}";
            if (b + 1 < rm.boundary.size()) {
                oss << ",";
            }
        }
        oss << "]}";
        if (i + 1 < rooms.size()) {
            oss << ",";
        }
    }
    oss << "]}";
    return oss.str();
}

ProjectDocument ProjectDocument::deserializeNative(std::string_view json, std::string* error) {
    ProjectDocument doc;
    if (json.find("mydesk-buildings") == std::string_view::npos) {
        if (error) {
            *error = "not a buildings document";
        }
        return doc;
    }
    // Controlled reload used by tests: recreate from known tags in serialized order.
    // Full general JSON parsing is deferred to a dedicated parser library.
    auto findNumberAfter = [&](std::string_view key, std::size_t from, double& out) -> bool {
        const auto pos = json.find(key, from);
        if (pos == std::string_view::npos) {
            return false;
        }
        const auto colon = json.find(':', pos);
        try {
            out = std::stod(std::string(json.substr(colon + 1)));
            return true;
        } catch (...) {
            return false;
        }
    };
    auto findStringAfter = [&](std::string_view key, std::size_t from, std::string& out) -> bool {
        const auto pos = json.find(key, from);
        if (pos == std::string_view::npos) {
            return false;
        }
        const auto q1 = json.find('"', json.find(':', pos));
        const auto q2 = json.find('"', q1 + 1);
        if (q1 == std::string_view::npos || q2 == std::string_view::npos) {
            return false;
        }
        out = std::string(json.substr(q1 + 1, q2 - q1 - 1));
        return true;
    };

    findStringAfter("\"name\"", 0, doc.name);
    std::size_t cursor = json.find("\"levels\"");
    while (cursor != std::string_view::npos) {
        const auto nextLevel = json.find("\"elevation\"", cursor);
        if (nextLevel == std::string_view::npos ||
            nextLevel > json.find("\"wallTypes\"")) {
            break;
        }
        Level level;
        double id = 0;
        findNumberAfter("\"id\"", cursor, id);
        level.id = static_cast<LevelId>(id);
        findStringAfter("\"name\"", cursor, level.name);
        findNumberAfter("\"elevation\"", cursor, level.elevation);
        doc.levels.push_back(level);
        doc.nextLevelId_ = std::max(doc.nextLevelId_, level.id + 1);
        cursor = nextLevel + 12;
    }

    cursor = json.find("\"wallTypes\"");
    const auto wallsPos = json.find("\"walls\"");
    while (cursor != std::string_view::npos && cursor < wallsPos) {
        const auto next = json.find("\"defaultHeight\"", cursor);
        if (next == std::string_view::npos || next > wallsPos) {
            break;
        }
        WallType type;
        double id = 0;
        findNumberAfter("\"id\"", cursor, id);
        type.id = static_cast<TypeId>(id);
        findStringAfter("\"name\"", cursor, type.name);
        findNumberAfter("\"thickness\"", cursor, type.thickness);
        findNumberAfter("\"defaultHeight\"", cursor, type.defaultHeight);
        doc.wallTypes.push_back(type);
        doc.nextTypeId_ = std::max(doc.nextTypeId_, type.id + 1);
        cursor = next + 16;
    }

    cursor = wallsPos;
    const auto doorsPos = json.find("\"doors\"");
    while (cursor != std::string_view::npos && cursor < doorsPos) {
        const auto next = json.find("\"y1\"", cursor);
        if (next == std::string_view::npos || next > doorsPos) {
            break;
        }
        Wall wall;
        double id = 0;
        findNumberAfter("\"id\"", cursor, id);
        wall.id = static_cast<ElementId>(id);
        double typeId = 0, levelId = 0;
        findNumberAfter("\"typeId\"", cursor, typeId);
        findNumberAfter("\"baseLevelId\"", cursor, levelId);
        wall.typeId = static_cast<TypeId>(typeId);
        wall.baseLevelId = static_cast<LevelId>(levelId);
        findNumberAfter("\"x0\"", cursor, wall.x0);
        findNumberAfter("\"y0\"", cursor, wall.y0);
        findNumberAfter("\"x1\"", cursor, wall.x1);
        findNumberAfter("\"y1\"", cursor, wall.y1);
        doc.walls.push_back(wall);
        doc.nextElementId_ = std::max(doc.nextElementId_, wall.id + 1);
        cursor = next + 4;
    }

    cursor = doorsPos;
    const auto winTypesPos = json.find("\"windowTypes\"");
    while (cursor != std::string_view::npos && (winTypesPos == std::string_view::npos || cursor < winTypesPos)) {
        const auto next = json.find("\"offsetAlongWall\"", cursor);
        if (next == std::string_view::npos || (winTypesPos != std::string_view::npos && next > winTypesPos)) {
            break;
        }
        Door door;
        double id = 0, typeId = 0, host = 0;
        findNumberAfter("\"id\"", cursor, id);
        findNumberAfter("\"typeId\"", cursor, typeId);
        findNumberAfter("\"hostWallId\"", cursor, host);
        findNumberAfter("\"offsetAlongWall\"", cursor, door.offsetAlongWall);
        door.id = static_cast<ElementId>(id);
        door.typeId = static_cast<TypeId>(typeId);
        door.hostWallId = static_cast<ElementId>(host);
        doc.doors.push_back(door);
        doc.nextElementId_ = std::max(doc.nextElementId_, door.id + 1);
        cursor = next + 18;
    }

    cursor = winTypesPos;
    const auto windowsPos = json.find("\"windows\"");
    while (cursor != std::string_view::npos && cursor < windowsPos) {
        const auto next = json.find("\"sillHeight\"", cursor);
        if (next == std::string_view::npos || next > windowsPos) {
            break;
        }
        WindowType wt;
        double id = 0;
        findNumberAfter("\"id\"", cursor, id);
        wt.id = static_cast<TypeId>(id);
        findStringAfter("\"name\"", cursor, wt.name);
        findNumberAfter("\"width\"", cursor, wt.width);
        findNumberAfter("\"height\"", cursor, wt.height);
        findNumberAfter("\"sillHeight\"", cursor, wt.sillHeight);
        doc.windowTypes.push_back(wt);
        doc.nextTypeId_ = std::max(doc.nextTypeId_, wt.id + 1);
        cursor = next + 12;
    }

    cursor = windowsPos;
    const auto roomsPos = json.find("\"rooms\"");
    while (cursor != std::string_view::npos && (roomsPos == std::string_view::npos || cursor < roomsPos)) {
        const auto next = json.find("\"offsetAlongWall\"", cursor);
        if (next == std::string_view::npos || (roomsPos != std::string_view::npos && next > roomsPos)) {
            break;
        }
        Window win;
        double id = 0, typeId = 0, host = 0;
        findNumberAfter("\"id\"", cursor, id);
        findNumberAfter("\"typeId\"", cursor, typeId);
        findNumberAfter("\"hostWallId\"", cursor, host);
        findNumberAfter("\"offsetAlongWall\"", cursor, win.offsetAlongWall);
        win.id = static_cast<ElementId>(id);
        win.typeId = static_cast<TypeId>(typeId);
        win.hostWallId = static_cast<ElementId>(host);
        doc.windows.push_back(win);
        doc.nextElementId_ = std::max(doc.nextElementId_, win.id + 1);
        cursor = next + 18;
    }

    cursor = roomsPos;
    while (cursor != std::string_view::npos) {
        const auto namePos = json.find("\"name\":", cursor);
        if (namePos == std::string_view::npos) {
            break;
        }
        Room rm;
        double id = 0, levelId = 0;
        findNumberAfter("\"id\"", cursor, id);
        findNumberAfter("\"levelId\"", cursor, levelId);
        findStringAfter("\"name\"", cursor, rm.name);
        rm.id = static_cast<ElementId>(id);
        rm.levelId = static_cast<LevelId>(levelId);

        const auto bStart = json.find("\"boundary\":[", cursor);
        if (bStart != std::string_view::npos) {
            const auto bEnd = json.find(']', bStart);
            if (bEnd != std::string_view::npos) {
                std::size_t bCursor = bStart;
                while (bCursor < bEnd) {
                    const auto xPos = json.find("\"x\":", bCursor);
                    const auto yPos = json.find("\"y\":", bCursor);
                    if (xPos == std::string_view::npos || yPos == std::string_view::npos ||
                        xPos > bEnd || yPos > bEnd) {
                        break;
                    }
                    double x = 0, y = 0;
                    findNumberAfter("\"x\"", bCursor, x);
                    findNumberAfter("\"y\"", bCursor, y);
                    rm.boundary.emplace_back(x, y);
                    bCursor = yPos + 4;
                }
            }
        }
        doc.rooms.push_back(rm);
        doc.nextElementId_ = std::max(doc.nextElementId_, rm.id + 1);
        const auto nextRm = json.find("{\"id\":", namePos);
        cursor = nextRm;
    }
    return doc;
}

std::string ProjectDocument::exportIfc4() const {
    std::ostringstream out;
    int id = 1;
    auto next = [&]() { return id++; };
    const int app = next();
    const int owner = next();
    const int org = next();
    const int person = next();
    const int unit = next();
    const int units = next();
    const int context = next();
    const int project = next();

    out << "ISO-10303-21;\nHEADER;\nFILE_DESCRIPTION(('ViewDefinition [CoordinationView]'),'2;1');\n";
    out << "FILE_NAME('mydesk-buildings.ifc','',('MyDesk Buildings'),('MyDesk'),"
           "('MyDesk Buildings'),(''),(''));\n";
    out << "FILE_SCHEMA(('IFC4'));\nENDSEC;\nDATA;\n";
    out << "#" << app << "=IFCAPPLICATION($,'0.1','MyDesk Buildings','MyDesk');\n";
    out << "#" << person << "=IFCPERSON($,'MyDesk',$,$,$,$,$,$);\n";
    out << "#" << org << "=IFCORGANIZATION($,'MyDesk',$,$,$);\n";
    out << "#" << owner << "=IFCPERSONANDORGANIZATION(#" << person << ",#" << org << ",$);\n";
    out << "#" << unit << "=IFCSIUNIT(*,.LENGTHUNIT.,$,.METRE.);\n";
    out << "#" << units << "=IFCUNITASSIGNMENT((#" << unit << "));\n";
    out << "#" << context
        << "=IFCGEOMETRICREPRESENTATIONCONTEXT($,'Model',3,1.E-05,$,$);\n";
    out << "#" << project << "=IFCPROJECT('" << escape(name) << "',#,$,$,$,$,$,(#" << context
        << "),#" << units << ");\n";

    const int site = next();
    const int building = next();
    out << "#" << site << "=IFCSITE('Site',#,$,$,$,$,$,$,.ELEMENT.,$,$,$,$,$);\n";
    out << "#" << building << "=IFCBUILDING('Building',#,$,$,$,$,$,$,.ELEMENT.,$,$,$);\n";
    const int relSite = next();
    out << "#" << relSite << "=IFCRELAGGREGATES('RelSite',#,$,$,#" << project << ",(#" << site
        << "));\n";
    const int relBld = next();
    out << "#" << relBld << "=IFCRELAGGREGATES('RelBld',#,$,$,#" << site << ",(#" << building
        << "));\n";

    std::unordered_map<LevelId, int> levelIfc;
    std::vector<int> storeyIds;
    for (const auto& level : levels) {
        const int storey = next();
        levelIfc[level.id] = storey;
        storeyIds.push_back(storey);
        out << "#" << storey << "=IFCBUILDINGSTOREY('" << escape(level.name) << "',#,$,$,$,$,$,$,"
            << ".ELEMENT.," << level.elevation << ");\n";
    }
    if (!storeyIds.empty()) {
        const int relStoreys = next();
        out << "#" << relStoreys << "=IFCRELAGGREGATES('RelStoreys',#,$,$,#" << building << ",(";
        for (std::size_t i = 0; i < storeyIds.size(); ++i) {
            out << "#" << storeyIds[i];
            if (i + 1 < storeyIds.size()) {
                out << ",";
            }
        }
        out << "));\n";
    }

    std::unordered_map<LevelId, std::vector<int>> wallsByLevel;
    for (const auto& wall : walls) {
        const int wallId = next();
        wallsByLevel[wall.baseLevelId].push_back(wallId);
        out << "#" << wallId << "=IFCWALLSTANDARDCASE('Wall" << wall.id << "',#,$,$,$,$,$,$);\n";
        // Placement stub carries semantic extents as a property set for this slice.
        const int pset = next();
        const int lenProp = next();
        const int baseProp = next();
        const int topProp = next();
        const int netProp = next();
        out << "#" << lenProp << "=IFCPROPERTYSINGLEVALUE('Length',$,IFCLENGTHMEASURE("
            << wallLength(wall) << "),$);\n";
        out << "#" << baseProp << "=IFCPROPERTYSINGLEVALUE('BaseElevation',$,IFCLENGTHMEASURE("
            << wallBaseElevation(wall) << "),$);\n";
        out << "#" << topProp << "=IFCPROPERTYSINGLEVALUE('TopElevation',$,IFCLENGTHMEASURE("
            << wallTopElevation(wall) << "),$);\n";
        out << "#" << netProp << "=IFCPROPERTYSINGLEVALUE('NetArea',$,IFCAREAMEASURE("
            << wallNetArea(wall) << "),$);\n";
        out << "#" << pset << "=IFCPROPERTYSET('Pset_WallCommon',#,$,$,(#" << lenProp << ",#"
            << baseProp << ",#" << topProp << ",#" << netProp << "));\n";
        const int relProp = next();
        out << "#" << relProp << "=IFCRELDEFINESBYPROPERTIES('RelProps',#,$,$,(#" << wallId
            << "),#" << pset << ");\n";
    }

    for (const auto& [levelId, wallIds] : wallsByLevel) {
        if (levelIfc.count(levelId) == 0U || wallIds.empty()) {
            continue;
        }
        const int rel = next();
        out << "#" << rel << "=IFCRELCONTAINEDINSPATIALSTRUCTURE('RelWalls',#,$,$,(";
        for (std::size_t i = 0; i < wallIds.size(); ++i) {
            out << "#" << wallIds[i];
            if (i + 1 < wallIds.size()) {
                out << ",";
            }
        }
        out << "),#" << levelIfc[levelId] << ");\n";
    }

    for (const auto& door : doors) {
        const int doorId = next();
        out << "#" << doorId << "=IFCDOOR('Door" << door.id << "',#,$,$,$,$,$,$,$);\n";
        if (findWall(door.hostWallId)) {
            const int voidRel = next();
            out << "#" << voidRel << "=IFCRELFILLSELEMENT('RelFillDoor',#,$,$,$,#" << doorId << ");\n";
        }
    }

    for (const auto& win : windows) {
        const int winId = next();
        out << "#" << winId << "=IFCWINDOW('Window" << win.id << "',#,$,$,$,$,$,$,$);\n";
        if (findWall(win.hostWallId)) {
            const int voidRel = next();
            out << "#" << voidRel << "=IFCRELFILLSELEMENT('RelFillWin',#,$,$,$,#" << winId << ");\n";
        }
    }

    std::unordered_map<LevelId, std::vector<int>> roomsByLevel;
    for (const auto& rm : rooms) {
        const int spaceId = next();
        roomsByLevel[rm.levelId].push_back(spaceId);
        out << "#" << spaceId << "=IFCSPACE('Room_" << escape(rm.name) << "',#,$,$,$,$,$,$,.ELEMENT.,$);\n";
        const int pset = next();
        const int areaProp = next();
        const int volProp = next();
        out << "#" << areaProp << "=IFCPROPERTYSINGLEVALUE('GrossFloorArea',$,IFCAREAMEASURE("
            << roomArea(rm) << "),$);\n";
        out << "#" << volProp << "=IFCPROPERTYSINGLEVALUE('GrossVolume',$,IFCVOLUMEMEASURE("
            << roomVolume(rm) << "),$);\n";
        out << "#" << pset << "=IFCPROPERTYSET('Pset_SpaceCommon',#,$,$,(#" << areaProp << ",#" << volProp << "));\n";
        const int relProp = next();
        out << "#" << relProp << "=IFCRELDEFINESBYPROPERTIES('RelSpaceProps',#,$,$,(#" << spaceId
            << "),#" << pset << ");\n";
    }

    for (const auto& [levelId, spaceIds] : roomsByLevel) {
        if (levelIfc.count(levelId) == 0U || spaceIds.empty()) {
            continue;
        }
        const int rel = next();
        out << "#" << rel << "=IFCRELCONTAINEDINSPATIALSTRUCTURE('RelSpaces',#,$,$,(";
        for (std::size_t i = 0; i < spaceIds.size(); ++i) {
            out << "#" << spaceIds[i];
            if (i + 1 < spaceIds.size()) {
                out << ",";
            }
        }
        out << "),#" << levelIfc[levelId] << ");\n";
    }

    out << "ENDSEC;\nEND-ISO-10303-21;\n";
    return out.str();
}

} // namespace mydesk::buildings
