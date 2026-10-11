#include "mydesk/buildings/project_document.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <iomanip>
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

// Simple recursive descent parser for formula expressions like "A * 2 + B / 3"
class FormulaParser {
public:
    FormulaParser(std::string_view expr, const std::unordered_map<std::string, double>& ctx)
        : expr_(expr), ctx_(ctx) {}

    double parse() {
        skipWhitespace();
        return parseExpression();
    }

private:
    std::string_view expr_;
    const std::unordered_map<std::string, double>& ctx_;
    std::size_t pos_{0};

    void skipWhitespace() {
        while (pos_ < expr_.size() && std::isspace(static_cast<unsigned char>(expr_[pos_]))) {
            ++pos_;
        }
    }

    char peek() {
        skipWhitespace();
        return pos_ < expr_.size() ? expr_[pos_] : '\0';
    }

    char get() {
        skipWhitespace();
        return pos_ < expr_.size() ? expr_[pos_++] : '\0';
    }

    double parseExpression() {
        double val = parseTerm();
        while (true) {
            char op = peek();
            if (op == '+' || op == '-') {
                get();
                double rhs = parseTerm();
                if (op == '+') val += rhs;
                else val -= rhs;
            } else {
                break;
            }
        }
        return val;
    }

    double parseTerm() {
        double val = parseFactor();
        while (true) {
            char op = peek();
            if (op == '*' || op == '/') {
                get();
                double rhs = parseFactor();
                if (op == '*') val *= rhs;
                else if (std::abs(rhs) > 1e-12) val /= rhs;
                else val = 0.0;
            } else {
                break;
            }
        }
        return val;
    }

    double parseFactor() {
        skipWhitespace();
        char c = peek();
        if (c == '(') {
            get();
            double val = parseExpression();
            if (peek() == ')') get();
            return val;
        }
        if (c == '-' || c == '+') {
            get();
            double val = parseFactor();
            return (c == '-') ? -val : val;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            std::size_t start = pos_;
            while (pos_ < expr_.size() &&
                   (std::isdigit(static_cast<unsigned char>(expr_[pos_])) || expr_[pos_] == '.')) {
                ++pos_;
            }
            std::string numStr(expr_.substr(start, pos_ - start));
            return std::stod(numStr);
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::size_t start = pos_;
            while (pos_ < expr_.size() &&
                   (std::isalnum(static_cast<unsigned char>(expr_[pos_])) || expr_[pos_] == '_')) {
                ++pos_;
            }
            std::string varName(expr_.substr(start, pos_ - start));
            auto it = ctx_.find(varName);
            if (it != ctx_.end()) {
                return it->second;
            }
            return 0.0;
        }
        return 0.0;
    }
};

} // namespace

double ProjectDocument::computePolygonArea(const std::vector<std::pair<double, double>>& boundary) {
    if (boundary.size() < 3) {
        return 0.0;
    }
    double sum = 0.0;
    const std::size_t n = boundary.size();
    for (std::size_t i = 0; i < n; ++i) {
        const auto& [x1, y1] = boundary[i];
        const auto& [x2, y2] = boundary[(i + 1) % n];
        sum += (x1 * y2 - x2 * y1);
    }
    return 0.5 * std::abs(sum);
}

// -----------------------------------------------------------------------------
// Factory Implementations
// -----------------------------------------------------------------------------
LevelId ProjectDocument::addLevel(std::string levelName, double elevation) {
    Level level;
    level.id = nextLevelId_++;
    level.name = std::move(levelName);
    level.elevation = elevation;
    levels.push_back(level);
    return level.id;
}

ElementId ProjectDocument::addGrid(std::string gridName, double x0, double y0, double x1, double y1) {
    Grid g;
    g.id = nextElementId_++;
    g.name = std::move(gridName);
    g.x0 = x0;
    g.y0 = y0;
    g.x1 = x1;
    g.y1 = y1;
    grids.push_back(g);
    return g.id;
}

TypeId ProjectDocument::addMaterial(std::string matName, double density, double thermalK,
                                    double youngsGPa, std::string hex) {
    Material m;
    m.id = nextTypeId_++;
    m.name = std::move(matName);
    m.densityKgM3 = density;
    m.thermalConductivityW_mK = thermalK;
    m.youngsModulusGPa = youngsGPa;
    m.colorHex = std::move(hex);
    materials.push_back(m);
    return m.id;
}

TypeId ProjectDocument::addWallType(std::string typeName, double thickness, double defaultHeight,
                                    TypeId materialId) {
    WallType type;
    type.id = nextTypeId_++;
    type.name = std::move(typeName);
    type.thickness = thickness;
    type.defaultHeight = defaultHeight;
    type.materialId = materialId;
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

TypeId ProjectDocument::addFloorType(std::string typeName, double thickness, TypeId materialId) {
    FloorType ft;
    ft.id = nextTypeId_++;
    ft.name = std::move(typeName);
    ft.thickness = thickness;
    ft.materialId = materialId;
    floorTypes.push_back(ft);
    return ft.id;
}

TypeId ProjectDocument::addRoofType(std::string typeName, double thickness, double defaultSlope,
                                    TypeId materialId) {
    RoofType rt;
    rt.id = nextTypeId_++;
    rt.name = std::move(typeName);
    rt.thickness = thickness;
    rt.defaultSlopeDeg = defaultSlope;
    rt.materialId = materialId;
    roofTypes.push_back(rt);
    return rt.id;
}

TypeId ProjectDocument::addCeilingType(std::string typeName, double thickness) {
    CeilingType ct;
    ct.id = nextTypeId_++;
    ct.name = std::move(typeName);
    ct.thickness = thickness;
    ceilingTypes.push_back(ct);
    return ct.id;
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

ElementId ProjectDocument::addFloor(TypeId typeId, LevelId levelId,
                                    std::vector<std::pair<double, double>> boundary, double offset) {
    Floor fl;
    fl.id = nextElementId_++;
    fl.typeId = typeId;
    fl.levelId = levelId;
    fl.boundary = std::move(boundary);
    fl.elevationOffset = offset;
    floors.push_back(fl);
    return fl.id;
}

ElementId ProjectDocument::addRoof(TypeId typeId, LevelId levelId,
                                   std::vector<std::pair<double, double>> boundary, double slope,
                                   double overhang) {
    Roof rf;
    rf.id = nextElementId_++;
    rf.typeId = typeId;
    rf.levelId = levelId;
    rf.boundary = std::move(boundary);
    rf.slopeDeg = slope;
    rf.overhang = overhang;
    roofs.push_back(rf);
    return rf.id;
}

ElementId ProjectDocument::addCeiling(TypeId typeId, LevelId levelId,
                                      std::vector<std::pair<double, double>> boundary,
                                      double heightAboveLevel) {
    Ceiling cl;
    cl.id = nextElementId_++;
    cl.typeId = typeId;
    cl.levelId = levelId;
    cl.boundary = std::move(boundary);
    cl.heightAboveLevel = heightAboveLevel;
    ceilings.push_back(cl);
    return cl.id;
}

ElementId ProjectDocument::addStair(LevelId baseLevelId, LevelId topLevelId, int riserCount,
                                    double treadDepth, double width) {
    Stair st;
    st.id = nextElementId_++;
    st.baseLevelId = baseLevelId;
    st.topLevelId = topLevelId;
    st.riserCount = std::max(1, riserCount);
    st.treadDepth = treadDepth;
    st.width = width;
    stairs.push_back(st);
    return st.id;
}

ElementId ProjectDocument::addRailing(LevelId levelId, std::vector<std::pair<double, double>> path,
                                      double height, double balusterSpacing) {
    Railing rl;
    rl.id = nextElementId_++;
    rl.levelId = levelId;
    rl.path = std::move(path);
    rl.height = height;
    rl.balusterSpacing = balusterSpacing;
    railings.push_back(rl);
    return rl.id;
}

ElementId ProjectDocument::addCurtainWall(LevelId levelId, double x0, double y0, double x1, double y1,
                                          double height, double spacingU, double spacingV) {
    CurtainWall cw;
    cw.id = nextElementId_++;
    cw.baseLevelId = levelId;
    cw.x0 = x0;
    cw.y0 = y0;
    cw.x1 = x1;
    cw.y1 = y1;
    cw.height = height;
    cw.gridSpacingU = spacingU;
    cw.gridSpacingV = spacingV;
    curtainWalls.push_back(cw);
    return cw.id;
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

TypeId ProjectDocument::addStructuralType(std::string typeName, std::string category, double width,
                                          double depth, TypeId materialId) {
    StructuralType st;
    st.id = nextTypeId_++;
    st.name = std::move(typeName);
    st.category = std::move(category);
    st.width = width;
    st.depth = depth;
    st.materialId = materialId;
    structuralTypes.push_back(st);
    return st.id;
}

ElementId ProjectDocument::addStructuralElement(TypeId typeId, LevelId levelId, double x0, double y0,
                                                double z0, double x1, double y1, double z1,
                                                bool isColumn) {
    StructuralElement se;
    se.id = nextElementId_++;
    se.typeId = typeId;
    se.levelId = levelId;
    se.x0 = x0;
    se.y0 = y0;
    se.z0 = z0;
    se.x1 = x1;
    se.y1 = y1;
    se.z1 = z1;
    se.isColumn = isColumn;
    structuralElements.push_back(se);
    return se.id;
}

TypeId ProjectDocument::addFoundationType(std::string typeName, double thickness, double bearingKPa) {
    FoundationType ft;
    ft.id = nextTypeId_++;
    ft.name = std::move(typeName);
    ft.thickness = thickness;
    ft.bearingCapacityKPa = bearingKPa;
    foundationTypes.push_back(ft);
    return ft.id;
}

ElementId ProjectDocument::addFoundation(TypeId typeId, LevelId levelId,
                                         std::vector<std::pair<double, double>> boundary,
                                         double thickness) {
    Foundation fnd;
    fnd.id = nextElementId_++;
    fnd.typeId = typeId;
    fnd.levelId = levelId;
    fnd.boundary = std::move(boundary);
    fnd.thickness = thickness;
    foundations.push_back(fnd);
    return fnd.id;
}

ElementId ProjectDocument::addSpace(LevelId levelId, std::string spaceName,
                                    std::vector<std::pair<double, double>> boundary, double targetLux,
                                    double airflowCfm) {
    Space sp;
    sp.id = nextElementId_++;
    sp.levelId = levelId;
    sp.name = std::move(spaceName);
    sp.boundary = std::move(boundary);
    sp.targetLux = targetLux;
    sp.designAirflowCfm = airflowCfm;
    spaces.push_back(sp);
    return sp.id;
}

ElementId ProjectDocument::addDuctSystem(std::string sysName, std::string systemType) {
    DuctSystem ds;
    ds.id = nextElementId_++;
    ds.name = std::move(sysName);
    ds.systemType = std::move(systemType);
    ductSystems.push_back(ds);
    return ds.id;
}

ElementId ProjectDocument::addDuctSegment(ElementId systemId, LevelId levelId, double x0, double y0,
                                          double z0, double x1, double y1, double z1,
                                          double widthOrDia, double height, bool isRound,
                                          double airflowCfm) {
    DuctSegment ds;
    ds.id = nextElementId_++;
    ds.systemId = systemId;
    ds.levelId = levelId;
    ds.x0 = x0;
    ds.y0 = y0;
    ds.z0 = z0;
    ds.x1 = x1;
    ds.y1 = y1;
    ds.z1 = z1;
    ds.width = widthOrDia;
    ds.height = height;
    ds.isRound = isRound;
    ds.airflowCfm = airflowCfm;
    ductSegments.push_back(ds);
    return ds.id;
}

ElementId ProjectDocument::addPipeSystem(std::string sysName, std::string fluidType) {
    PipeSystem ps;
    ps.id = nextElementId_++;
    ps.name = std::move(sysName);
    ps.fluidType = std::move(fluidType);
    pipeSystems.push_back(ps);
    return ps.id;
}

ElementId ProjectDocument::addPipeSegment(ElementId systemId, LevelId levelId, double x0, double y0,
                                          double z0, double x1, double y1, double z1,
                                          double diameterMm, double flowRateGpm) {
    PipeSegment ps;
    ps.id = nextElementId_++;
    ps.systemId = systemId;
    ps.levelId = levelId;
    ps.x0 = x0;
    ps.y0 = y0;
    ps.z0 = z0;
    ps.x1 = x1;
    ps.y1 = y1;
    ps.z1 = z1;
    ps.diameterMm = diameterMm;
    ps.flowRateGpm = flowRateGpm;
    pipeSegments.push_back(ps);
    return ps.id;
}

ElementId ProjectDocument::addElecPanel(std::string panelName, double voltage, double mainsAmps) {
    ElecPanel ep;
    ep.id = nextElementId_++;
    ep.name = std::move(panelName);
    ep.voltage = voltage;
    ep.mainsRatingAmps = mainsAmps;
    elecPanels.push_back(ep);
    return ep.id;
}

ElementId ProjectDocument::addElecCircuit(ElementId panelId, int circuitNumber, double loadWatts,
                                          double voltage, double breakerAmps) {
    for (auto& p : elecPanels) {
        if (p.id == panelId) {
            ElecCircuit ec;
            ec.id = nextElementId_++;
            ec.panelId = panelId;
            ec.circuitNumber = circuitNumber;
            ec.loadWatts = loadWatts;
            ec.voltage = voltage;
            ec.breakerAmps = breakerAmps;
            p.circuits.push_back(ec);
            return ec.id;
        }
    }
    return 0;
}

ElementId ProjectDocument::addPhase(std::string phaseName, int sequence) {
    Phase ph;
    ph.id = nextElementId_++;
    ph.name = std::move(phaseName);
    ph.sequence = sequence;
    phases.push_back(ph);
    return ph.id;
}

ElementId ProjectDocument::addDesignOptionSet(std::string setName) {
    DesignOptionSet dos;
    dos.id = nextElementId_++;
    dos.name = std::move(setName);
    designOptionSets.push_back(dos);
    return dos.id;
}

ElementId ProjectDocument::addDesignOption(ElementId setId, std::string optName, bool isPrimary) {
    for (auto& set : designOptionSets) {
        if (set.id == setId) {
            DesignOption opt;
            opt.id = nextElementId_++;
            opt.name = std::move(optName);
            opt.isPrimary = isPrimary;
            set.options.push_back(opt);
            return opt.id;
        }
    }
    return 0;
}

ElementId ProjectDocument::addWorkset(std::string worksetName, std::string owner) {
    Workset ws;
    ws.id = nextElementId_++;
    ws.name = std::move(worksetName);
    ws.owner = std::move(owner);
    ws.isEditable = true;
    worksets.push_back(ws);
    return ws.id;
}

ElementId ProjectDocument::addFamilyComponent(std::string familyName, std::string typeName,
                                              LevelId levelId, double x, double y, double z) {
    FamilyComponent fc;
    fc.id = nextElementId_++;
    fc.familyName = std::move(familyName);
    fc.typeName = std::move(typeName);
    fc.hostLevelId = levelId;
    fc.x = x;
    fc.y = y;
    fc.z = z;
    familyComponents.push_back(fc);
    return fc.id;
}

ElementId ProjectDocument::addGroup(std::string groupName, std::vector<ElementId> elementIds) {
    BldgGroup bg;
    bg.id = nextElementId_++;
    bg.name = std::move(groupName);
    bg.elementIds = std::move(elementIds);
    groups.push_back(bg);
    return bg.id;
}

ElementId ProjectDocument::duplicateGroup(ElementId groupId, double dx, double dy, double /*dz*/) {
    const BldgGroup* src = nullptr;
    for (const auto& g : groups) {
        if (g.id == groupId) {
            src = &g;
            break;
        }
    }
    if (!src) return 0;

    std::vector<ElementId> newElems;
    for (auto elemId : src->elementIds) {
        if (const auto* w = findWall(elemId)) {
            newElems.push_back(addWall(w->typeId, w->baseLevelId, w->x0 + dx, w->y0 + dy,
                                       w->x1 + dx, w->y1 + dy));
        }
    }
    return addGroup(src->name + "_Copy", newElems);
}

void ProjectDocument::setTopography(std::string topoName, std::vector<TopoPoint> points,
                                    std::vector<std::array<std::size_t, 3>> triangles) {
    topography.id = nextElementId_++;
    topography.name = std::move(topoName);
    topography.points = std::move(points);
    topography.triangles = std::move(triangles);
}

ElementId ProjectDocument::addLinkedModel(std::string filePath, std::string linkName, double tx,
                                          double ty, double tz) {
    LinkedModel lm;
    lm.id = nextElementId_++;
    lm.filePath = std::move(filePath);
    lm.linkName = std::move(linkName);
    lm.translationX = tx;
    lm.translationY = ty;
    lm.translationZ = tz;
    lm.isLoaded = true;
    linkedModels.push_back(lm);
    return lm.id;
}

ElementId ProjectDocument::addView(std::string viewName, std::string viewType, LevelId levelId,
                                   double scale) {
    BldgView v;
    v.id = nextElementId_++;
    v.name = std::move(viewName);
    v.viewType = std::move(viewType);
    v.levelId = levelId;
    v.scaleDenominator = scale;
    views.push_back(v);
    return v.id;
}

ElementId ProjectDocument::addSheet(std::string number, std::string sheetTitle, double widthMm,
                                    double heightMm) {
    Sheet s;
    s.id = nextElementId_++;
    s.sheetNumber = std::move(number);
    s.sheetName = std::move(sheetTitle);
    s.widthMm = widthMm;
    s.heightMm = heightMm;
    sheets.push_back(s);
    return s.id;
}

bool ProjectDocument::placeViewOnSheet(ElementId sheetId, ElementId viewId) {
    for (auto& s : sheets) {
        if (s.id == sheetId) {
            s.placedViewIds.push_back(viewId);
            return true;
        }
    }
    return false;
}

ElementId ProjectDocument::addTag(ElementId targetElemId, std::string category, double x, double y,
                                  std::string label) {
    Tag tg;
    tg.id = nextElementId_++;
    tg.targetElementId = targetElemId;
    tg.category = std::move(category);
    tg.x = x;
    tg.y = y;
    tg.label = std::move(label);
    tags.push_back(tg);
    return tg.id;
}

ElementId ProjectDocument::addDimension(ElementId elemAId, ElementId elemBId, std::string dimType,
                                        double measuredValue, std::string textOverride) {
    Dimension d;
    d.id = nextElementId_++;
    d.elemAId = elemAId;
    d.elemBId = elemBId;
    d.dimType = std::move(dimType);
    d.measuredValue = measuredValue;
    d.textOverride = std::move(textOverride);
    dimensions.push_back(d);
    return d.id;
}

ElementId ProjectDocument::addDetailComponent(ElementId viewId, std::string familyName, double x,
                                              double y, double rotationDeg) {
    DetailComponent dc;
    dc.id = nextElementId_++;
    dc.viewId = viewId;
    dc.familyName = std::move(familyName);
    dc.x = x;
    dc.y = y;
    dc.rotationDeg = rotationDeg;
    detailComponents.push_back(dc);
    return dc.id;
}

void ProjectDocument::addKeynote(std::string key, std::string text) {
    keynoteDatabase.push_back(Keynote{std::move(key), std::move(text)});
}

// -----------------------------------------------------------------------------
// Lookups
// -----------------------------------------------------------------------------
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
        if (level.id == id) return &level;
    }
    return nullptr;
}

const Wall* ProjectDocument::findWall(ElementId id) const {
    for (const auto& wall : walls) {
        if (wall.id == id) return &wall;
    }
    return nullptr;
}

const WallType* ProjectDocument::findWallType(TypeId id) const {
    for (const auto& type : wallTypes) {
        if (type.id == id) return &type;
    }
    return nullptr;
}

const DoorType* ProjectDocument::findDoorType(TypeId id) const {
    for (const auto& type : doorTypes) {
        if (type.id == id) return &type;
    }
    return nullptr;
}

const WindowType* ProjectDocument::findWindowType(TypeId id) const {
    for (const auto& type : windowTypes) {
        if (type.id == id) return &type;
    }
    return nullptr;
}

const Room* ProjectDocument::findRoom(ElementId id) const {
    for (const auto& room : rooms) {
        if (room.id == id) return &room;
    }
    return nullptr;
}

const Material* ProjectDocument::findMaterial(TypeId id) const {
    for (const auto& m : materials) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

// -----------------------------------------------------------------------------
// Computations & Analysis
// -----------------------------------------------------------------------------
double ProjectDocument::wallLength(const Wall& wall) const {
    return std::hypot(wall.x1 - wall.x0, wall.y1 - wall.y0);
}

double ProjectDocument::wallHeight(const Wall& wall) const {
    if (wall.heightOverride > 0.0) return wall.heightOverride;
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
            if (dt) deductions += (dt->width * dt->height);
        }
    }
    for (const auto& win : windows) {
        if (win.hostWallId == wall.id) {
            const auto* wt = findWindowType(win.typeId);
            if (wt) deductions += (wt->width * wt->height);
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

double ProjectDocument::floorArea(const Floor& floor) const {
    return computePolygonArea(floor.boundary);
}

double ProjectDocument::floorVolume(const Floor& floor) const {
    for (const auto& t : floorTypes) {
        if (t.id == floor.typeId) {
            return floorArea(floor) * t.thickness;
        }
    }
    return floorArea(floor) * 0.25;
}

double ProjectDocument::roofFootprintArea(const Roof& roof) const {
    return computePolygonArea(roof.boundary);
}

double ProjectDocument::roofSurfaceArea(const Roof& roof) const {
    double fp = roofFootprintArea(roof);
    double rad = (roof.slopeDeg * 3.14159265358979323846) / 180.0;
    double cosVal = std::cos(rad);
    return (cosVal > 1e-4) ? (fp / cosVal) : fp;
}

double ProjectDocument::ceilingArea(const Ceiling& ceiling) const {
    return computePolygonArea(ceiling.boundary);
}

double ProjectDocument::stairRiserHeight(const Stair& stair) const {
    const auto* baseLvl = findLevel(stair.baseLevelId);
    const auto* topLvl = findLevel(stair.topLevelId);
    double elevDiff = (topLvl && baseLvl) ? (topLvl->elevation - baseLvl->elevation) : 3.0;
    return elevDiff / stair.riserCount;
}

double ProjectDocument::stairRunLength(const Stair& stair) const {
    return (stair.riserCount - 1) * stair.treadDepth;
}

double ProjectDocument::stairSlopeAngleDeg(const Stair& stair) const {
    double rh = stairRiserHeight(stair);
    return std::atan2(rh, stair.treadDepth) * 180.0 / 3.14159265358979323846;
}

double ProjectDocument::railingLength(const Railing& railing) const {
    double len = 0.0;
    if (railing.path.size() < 2) return 0.0;
    for (std::size_t i = 0; i + 1 < railing.path.size(); ++i) {
        len += std::hypot(railing.path[i + 1].first - railing.path[i].first,
                          railing.path[i + 1].second - railing.path[i].second);
    }
    return len;
}

std::size_t ProjectDocument::railingBalusterCount(const Railing& railing) const {
    double len = railingLength(railing);
    if (railing.balusterSpacing <= 1e-6) return 0;
    return static_cast<std::size_t>(std::ceil(len / railing.balusterSpacing)) + 1;
}

std::size_t ProjectDocument::curtainWallPanelCount(const CurtainWall& cw) const {
    double len = std::hypot(cw.x1 - cw.x0, cw.y1 - cw.y0);
    int panelsU = std::max(1, static_cast<int>(std::ceil(len / std::max(0.1, cw.gridSpacingU))));
    int panelsV = std::max(1, static_cast<int>(std::ceil(cw.height / std::max(0.1, cw.gridSpacingV))));
    return static_cast<std::size_t>(panelsU * panelsV);
}

double ProjectDocument::curtainWallMullionLength(const CurtainWall& cw) const {
    double len = std::hypot(cw.x1 - cw.x0, cw.y1 - cw.y0);
    int panelsU = std::max(1, static_cast<int>(std::ceil(len / std::max(0.1, cw.gridSpacingU))));
    int panelsV = std::max(1, static_cast<int>(std::ceil(cw.height / std::max(0.1, cw.gridSpacingV))));
    return (panelsU + 1) * cw.height + (panelsV + 1) * len;
}

double ProjectDocument::roomArea(const Room& room) const {
    return computePolygonArea(room.boundary);
}

double ProjectDocument::roomVolume(const Room& room, double height) const {
    return roomArea(room) * (height > 0.0 ? height : 3.0);
}

double ProjectDocument::spaceArea(const Space& space) const {
    return computePolygonArea(space.boundary);
}

double ProjectDocument::spaceVolume(const Space& space, double height) const {
    return spaceArea(space) * (height > 0.0 ? height : 3.0);
}

double ProjectDocument::spaceTotalLightingWatts(const Space& space) const {
    return spaceArea(space) * space.lightingLoadWattsPerM2;
}

double ProjectDocument::structuralLength(const StructuralElement& elem) const {
    return std::hypot(elem.x1 - elem.x0, elem.y1 - elem.y0, elem.z1 - elem.z0);
}

double ProjectDocument::structuralVolume(const StructuralElement& elem) const {
    double len = structuralLength(elem);
    for (const auto& t : structuralTypes) {
        if (t.id == elem.typeId) {
            return len * t.width * t.depth;
        }
    }
    return len * 0.3 * 0.4;
}

double ProjectDocument::structuralWeightKg(const StructuralElement& elem) const {
    double vol = structuralVolume(elem);
    for (const auto& t : structuralTypes) {
        if (t.id == elem.typeId) {
            if (const auto* m = findMaterial(t.materialId)) {
                return vol * m->densityKgM3;
            }
        }
    }
    return vol * 7850.0; // steel default
}

double ProjectDocument::foundationArea(const Foundation& fnd) const {
    return computePolygonArea(fnd.boundary);
}

double ProjectDocument::foundationVolume(const Foundation& fnd) const {
    return foundationArea(fnd) * fnd.thickness;
}

double ProjectDocument::foundationAllowableLoadKn(const Foundation& fnd) const {
    double area = foundationArea(fnd);
    for (const auto& t : foundationTypes) {
        if (t.id == fnd.typeId) {
            return area * t.bearingCapacityKPa;
        }
    }
    return area * 250.0;
}

double ProjectDocument::ductVelocityFpm(const DuctSegment& duct) const {
    double areaM2 = duct.isRound ? (3.14159265358979323846 * std::pow(duct.width * 0.5, 2))
                                 : (duct.width * duct.height);
    if (areaM2 <= 1e-6) return 0.0;
    double areaSqFt = areaM2 * 10.7639;
    return duct.airflowCfm / areaSqFt;
}

double ProjectDocument::ductFrictionLossInWg(const DuctSegment& duct) const {
    double vFpm = ductVelocityFpm(duct);
    double len = std::hypot(duct.x1 - duct.x0, duct.y1 - duct.y0, duct.z1 - duct.z0);
    double lenFt = len * 3.28084;
    double lossPer100Ft = 0.1 * std::pow(vFpm / 1000.0, 1.8);
    return lossPer100Ft * (lenFt / 100.0);
}

double ProjectDocument::pipeVelocityMps(const PipeSegment& pipe) const {
    double diamM = pipe.diameterMm * 0.001;
    double areaM2 = 3.14159265358979323846 * std::pow(diamM * 0.5, 2);
    if (areaM2 <= 1e-8) return 0.0;
    double m3s = pipe.flowRateGpm * 0.00006309;
    return m3s / areaM2;
}

double ProjectDocument::pipeFrictionLossMeters(const PipeSegment& pipe) const {
    double v = pipeVelocityMps(pipe);
    double len = std::hypot(pipe.x1 - pipe.x0, pipe.y1 - pipe.y0, pipe.z1 - pipe.z0);
    double diamM = pipe.diameterMm * 0.001;
    if (diamM <= 1e-4) return 0.0;
    return 0.02 * (len / diamM) * (v * v / (2.0 * 9.80665));
}

double ProjectDocument::panelConnectedLoadWatts(const ElecPanel& panel) const {
    double total = 0.0;
    for (const auto& c : panel.circuits) {
        total += c.loadWatts;
    }
    return total;
}

double ProjectDocument::panelDemandCurrentAmps(const ElecPanel& panel) const {
    double totalWatts = panelConnectedLoadWatts(panel);
    if (panel.voltage <= 1e-2) return 0.0;
    return totalWatts / (std::sqrt(3.0) * panel.voltage);
}

double ProjectDocument::panelCapacityUtilizationPercent(const ElecPanel& panel) const {
    if (panel.mainsRatingAmps <= 1e-2) return 0.0;
    return (panelDemandCurrentAmps(panel) / panel.mainsRatingAmps) * 100.0;
}

double ProjectDocument::topoSurfaceArea() const {
    double total = 0.0;
    for (const auto& tri : topography.triangles) {
        if (tri[0] >= topography.points.size() || tri[1] >= topography.points.size() ||
            tri[2] >= topography.points.size()) continue;
        const auto& p0 = topography.points[tri[0]];
        const auto& p1 = topography.points[tri[1]];
        const auto& p2 = topography.points[tri[2]];
        double ax = p1.x - p0.x, ay = p1.y - p0.y, az = p1.z - p0.z;
        double bx = p2.x - p0.x, by = p2.y - p0.y, bz = p2.z - p0.z;
        double cx = ay * bz - az * by;
        double cy = az * bx - ax * bz;
        double cz = ax * by - ay * bx;
        total += 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
    }
    return total;
}

double ProjectDocument::topoProjectedArea() const {
    double total = 0.0;
    for (const auto& tri : topography.triangles) {
        if (tri[0] >= topography.points.size() || tri[1] >= topography.points.size() ||
            tri[2] >= topography.points.size()) continue;
        const auto& p0 = topography.points[tri[0]];
        const auto& p1 = topography.points[tri[1]];
        const auto& p2 = topography.points[tri[2]];
        double crossZ = (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x);
        total += 0.5 * std::abs(crossZ);
    }
    return total;
}

std::pair<double, double> ProjectDocument::topoElevationRange() const {
    if (topography.points.empty()) return {0.0, 0.0};
    double minZ = topography.points.front().z;
    double maxZ = topography.points.front().z;
    for (const auto& p : topography.points) {
        minZ = std::min(minZ, p.z);
        maxZ = std::max(maxZ, p.z);
    }
    return {minZ, maxZ};
}

double ProjectDocument::evaluateFormula(std::string_view expression,
                                        const std::unordered_map<std::string, double>& context) {
    FormulaParser parser(expression, context);
    return parser.parse();
}

// -----------------------------------------------------------------------------
// Clash Detection / Interference Check
// -----------------------------------------------------------------------------
std::vector<Interference> ProjectDocument::checkInterference() const {
    struct BoxItem {
        ElementId id;
        std::string cat;
        BoundingBox3D box;
    };
    std::vector<BoxItem> items;

    // Walls
    for (const auto& w : walls) {
        double minX = std::min(w.x0, w.x1) - 0.15;
        double maxX = std::max(w.x0, w.x1) + 0.15;
        double minY = std::min(w.y0, w.y1) - 0.15;
        double maxY = std::max(w.y0, w.y1) + 0.15;
        double minZ = wallBaseElevation(w);
        double maxZ = wallTopElevation(w);
        items.push_back({w.id, "Wall", {minX, minY, minZ, maxX, maxY, maxZ}});
    }

    // Structural elements
    for (const auto& se : structuralElements) {
        double minX = std::min(se.x0, se.x1) - 0.2;
        double maxX = std::max(se.x0, se.x1) + 0.2;
        double minY = std::min(se.y0, se.y1) - 0.2;
        double maxY = std::max(se.y0, se.y1) + 0.2;
        double minZ = std::min(se.z0, se.z1) - 0.2;
        double maxZ = std::max(se.z0, se.z1) + 0.2;
        items.push_back({se.id, "Structural", {minX, minY, minZ, maxX, maxY, maxZ}});
    }

    // Ducts
    for (const auto& d : ductSegments) {
        double r = std::max(d.width, d.height) * 0.5;
        double minX = std::min(d.x0, d.x1) - r;
        double maxX = std::max(d.x0, d.x1) + r;
        double minY = std::min(d.y0, d.y1) - r;
        double maxY = std::max(d.y0, d.y1) + r;
        double minZ = std::min(d.z0, d.z1) - r;
        double maxZ = std::max(d.z0, d.z1) + r;
        items.push_back({d.id, "Duct", {minX, minY, minZ, maxX, maxY, maxZ}});
    }

    // Pipes
    for (const auto& p : pipeSegments) {
        double r = p.diameterMm * 0.001 * 0.5;
        double minX = std::min(p.x0, p.x1) - r;
        double maxX = std::max(p.x0, p.x1) + r;
        double minY = std::min(p.y0, p.y1) - r;
        double maxY = std::max(p.y0, p.y1) + r;
        double minZ = std::min(p.z0, p.z1) - r;
        double maxZ = std::max(p.z0, p.z1) + r;
        items.push_back({p.id, "Pipe", {minX, minY, minZ, maxX, maxY, maxZ}});
    }

    std::vector<Interference> clashes;
    for (std::size_t i = 0; i < items.size(); ++i) {
        for (std::size_t j = i + 1; j < items.size(); ++j) {
            // Ignore collisions within same category for walls connected at corners
            if (items[i].cat == items[j].cat) continue;

            if (items[i].box.intersects(items[j].box)) {
                double vol = items[i].box.intersectionVolume(items[j].box);
                if (vol > 1e-4) {
                    Interference inf;
                    inf.elementA = items[i].id;
                    inf.elementB = items[j].id;
                    inf.categoryA = items[i].cat;
                    inf.categoryB = items[j].cat;
                    inf.clashVolume = vol;
                    inf.description = items[i].cat + " intersects " + items[j].cat;
                    clashes.push_back(inf);
                }
            }
        }
    }
    return clashes;
}

// -----------------------------------------------------------------------------
// Schedules
// -----------------------------------------------------------------------------
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

std::vector<AdvancedScheduleItem> ProjectDocument::generateAdvancedSchedule(
    std::string_view categoryFilter) const {
    std::vector<AdvancedScheduleItem> list;

    if (categoryFilter.empty() || categoryFilter == "Wall") {
        for (const auto& w : walls) {
            const auto* t = findWallType(w.typeId);
            const auto* lvl = findLevel(w.baseLevelId);
            list.push_back({w.id, "Wall", t ? t->name : "Standard Wall",
                            lvl ? lvl->name : "Level", wallArea(w), "m2"});
        }
    }
    if (categoryFilter.empty() || categoryFilter == "Floor") {
        for (const auto& f : floors) {
            const auto* lvl = findLevel(f.levelId);
            list.push_back({f.id, "Floor", "Compound Floor",
                            lvl ? lvl->name : "Level", floorArea(f), "m2"});
        }
    }
    if (categoryFilter.empty() || categoryFilter == "Roof") {
        for (const auto& r : roofs) {
            const auto* lvl = findLevel(r.levelId);
            list.push_back({r.id, "Roof", "Basic Roof",
                            lvl ? lvl->name : "Level", roofSurfaceArea(r), "m2"});
        }
    }
    if (categoryFilter.empty() || categoryFilter == "Structural") {
        for (const auto& s : structuralElements) {
            const auto* lvl = findLevel(s.levelId);
            list.push_back({s.id, s.isColumn ? "Column" : "Beam",
                            s.isColumn ? "W-Shape Column" : "W-Shape Beam",
                            lvl ? lvl->name : "Level", structuralLength(s), "m"});
        }
    }
    if (categoryFilter.empty() || categoryFilter == "Duct") {
        for (const auto& d : ductSegments) {
            const auto* lvl = findLevel(d.levelId);
            list.push_back({d.id, "Duct", d.isRound ? "Round Duct" : "Rectangular Duct",
                            lvl ? lvl->name : "Level", d.airflowCfm, "CFM"});
        }
    }
    return list;
}

// -----------------------------------------------------------------------------
// Dynamo Visual Automation Graph
// -----------------------------------------------------------------------------
bool ProjectDocument::executeDynamoGraph(const DynamoGraph& graph) {
    if (levels.empty()) {
        addLevel("Level 1", 0.0);
    }
    if (wallTypes.empty()) {
        addWallType("Generic 200mm", 0.2, 3.0);
    }

    for (const auto& node : graph.nodes) {
        if (node.nodeType == "CreateWall") {
            double x0 = node.numericInputs.count("x0") ? node.numericInputs.at("x0") : 0.0;
            double y0 = node.numericInputs.count("y0") ? node.numericInputs.at("y0") : 0.0;
            double x1 = node.numericInputs.count("x1") ? node.numericInputs.at("x1") : 10.0;
            double y1 = node.numericInputs.count("y1") ? node.numericInputs.at("y1") : 0.0;
            addWall(wallTypes.front().id, levels.front().id, x0, y0, x1, y1);
        } else if (node.nodeType == "SetElevation") {
            double elev = node.numericInputs.count("elevation") ? node.numericInputs.at("elevation") : 4.0;
            if (!levels.empty()) {
                setLevelElevation(levels.front().id, elev);
            }
        }
    }
    return true;
}

// -----------------------------------------------------------------------------
// Serialization: Native JSON
// -----------------------------------------------------------------------------
std::string ProjectDocument::serializeNative() const {
    std::ostringstream oss;
    oss << "{\"format\":\"mydesk-buildings\",\"schemaVersion\":\"1\",\"name\":\"" << name << "\",";
    oss << "\"levels\":[";
    for (std::size_t i = 0; i < levels.size(); ++i) {
        const auto& l = levels[i];
        oss << "{\"id\":" << l.id << ",\"name\":\"" << l.name << "\",\"elevation\":" << l.elevation
            << "}";
        if (i + 1 < levels.size()) oss << ",";
    }
    oss << "],\"wallTypes\":[";
    for (std::size_t i = 0; i < wallTypes.size(); ++i) {
        const auto& t = wallTypes[i];
        oss << "{\"id\":" << t.id << ",\"name\":\"" << t.name << "\",\"thickness\":" << t.thickness
            << ",\"defaultHeight\":" << t.defaultHeight << "}";
        if (i + 1 < wallTypes.size()) oss << ",";
    }
    oss << "],\"walls\":[";
    for (std::size_t i = 0; i < walls.size(); ++i) {
        const auto& w = walls[i];
        oss << "{\"id\":" << w.id << ",\"typeId\":" << w.typeId << ",\"baseLevelId\":"
            << w.baseLevelId << ",\"x0\":" << w.x0 << ",\"y0\":" << w.y0 << ",\"x1\":" << w.x1
            << ",\"y1\":" << w.y1 << "}";
        if (i + 1 < walls.size()) oss << ",";
    }
    oss << "],\"doors\":[";
    for (std::size_t i = 0; i < doors.size(); ++i) {
        const auto& d = doors[i];
        oss << "{\"id\":" << d.id << ",\"typeId\":" << d.typeId << ",\"hostWallId\":"
            << d.hostWallId << ",\"offsetAlongWall\":" << d.offsetAlongWall << "}";
        if (i + 1 < doors.size()) oss << ",";
    }
    oss << "],\"windowTypes\":[";
    for (std::size_t i = 0; i < windowTypes.size(); ++i) {
        const auto& wt = windowTypes[i];
        oss << "{\"id\":" << wt.id << ",\"name\":\"" << wt.name << "\",\"width\":" << wt.width
            << ",\"height\":" << wt.height << ",\"sillHeight\":" << wt.sillHeight << "}";
        if (i + 1 < windowTypes.size()) oss << ",";
    }
    oss << "],\"windows\":[";
    for (std::size_t i = 0; i < windows.size(); ++i) {
        const auto& win = windows[i];
        oss << "{\"id\":" << win.id << ",\"typeId\":" << win.typeId << ",\"hostWallId\":"
            << win.hostWallId << ",\"offsetAlongWall\":" << win.offsetAlongWall << "}";
        if (i + 1 < windows.size()) oss << ",";
    }
    oss << "],\"rooms\":[";
    for (std::size_t i = 0; i < rooms.size(); ++i) {
        const auto& rm = rooms[i];
        oss << "{\"id\":" << rm.id << ",\"levelId\":" << rm.levelId << ",\"name\":\"" << rm.name
            << "\",\"boundary\":[";
        for (std::size_t b = 0; b < rm.boundary.size(); ++b) {
            oss << "{\"x\":" << rm.boundary[b].first << ",\"y\":" << rm.boundary[b].second << "}";
            if (b + 1 < rm.boundary.size()) oss << ",";
        }
        oss << "]}";
        if (i + 1 < rooms.size()) oss << ",";
    }
    oss << "]}";
    return oss.str();
}

ProjectDocument ProjectDocument::deserializeNative(std::string_view json, std::string* error) {
    ProjectDocument doc;
    if (json.find("mydesk-buildings") == std::string_view::npos) {
        if (error) *error = "not a buildings document";
        return doc;
    }
    auto findNumberAfter = [&](std::string_view key, std::size_t from, double& out) -> bool {
        const auto pos = json.find(key, from);
        if (pos == std::string_view::npos) return false;
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
        if (pos == std::string_view::npos) return false;
        const auto q1 = json.find('"', json.find(':', pos));
        const auto q2 = json.find('"', q1 + 1);
        if (q1 == std::string_view::npos || q2 == std::string_view::npos) return false;
        out = std::string(json.substr(q1 + 1, q2 - q1 - 1));
        return true;
    };

    findStringAfter("\"name\"", 0, doc.name);
    std::size_t cursor = json.find("\"levels\"");
    while (cursor != std::string_view::npos) {
        const auto nextLevel = json.find("\"elevation\"", cursor);
        if (nextLevel == std::string_view::npos || nextLevel > json.find("\"wallTypes\"")) break;
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
        if (next == std::string_view::npos || next > wallsPos) break;
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
        if (next == std::string_view::npos || next > doorsPos) break;
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
        if (next == std::string_view::npos || (winTypesPos != std::string_view::npos && next > winTypesPos)) break;
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
        if (next == std::string_view::npos || next > windowsPos) break;
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
        if (next == std::string_view::npos || (roomsPos != std::string_view::npos && next > roomsPos)) break;
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
        if (namePos == std::string_view::npos) break;
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
                        xPos > bEnd || yPos > bEnd) break;
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

// -----------------------------------------------------------------------------
// Package RVT Binary Serialization
// -----------------------------------------------------------------------------
std::vector<std::uint8_t> ProjectDocument::serializePackageRvt() const {
    std::string json = serializeNative();
    std::vector<std::uint8_t> bytes = {'R', 'V', 'T', 0x01, 0x00};
    std::uint64_t len = json.size();
    for (int i = 0; i < 8; ++i) {
        bytes.push_back(static_cast<std::uint8_t>((len >> (i * 8)) & 0xFF));
    }
    for (char c : json) {
        bytes.push_back(static_cast<std::uint8_t>(c));
    }
    return bytes;
}

ProjectDocument ProjectDocument::deserializePackageRvt(const std::vector<std::uint8_t>& bytes,
                                                       std::string* error) {
    if (bytes.size() < 13 || bytes[0] != 'R' || bytes[1] != 'V' || bytes[2] != 'T') {
        if (error) *error = "invalid RVT binary package header";
        return {};
    }
    std::uint64_t len = 0;
    for (int i = 0; i < 8; ++i) {
        len |= (static_cast<std::uint64_t>(bytes[static_cast<std::size_t>(5 + i)]) << (i * 8));
    }
    if (bytes.size() < 13 + len) {
        if (error) *error = "truncated RVT binary package";
        return {};
    }
    std::string json(reinterpret_cast<const char*>(bytes.data() + 13), static_cast<std::size_t>(len));
    return deserializeNative(json, error);
}

// -----------------------------------------------------------------------------
// IFC4 Export
// -----------------------------------------------------------------------------
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

    // Materials
    for (const auto& m : materials) {
        const int matId = next();
        out << "#" << matId << "=IFCMATERIAL('" << escape(m.name) << "',$,$);\n";
    }

    // Grids
    for (const auto& g : grids) {
        const int gridId = next();
        out << "#" << gridId << "=IFCGRID('Grid_" << escape(g.name) << "',#,$,$,$,$,$,$);\n";
    }

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
            if (i + 1 < storeyIds.size()) out << ",";
        }
        out << "));\n";
    }

    // Walls
    std::unordered_map<LevelId, std::vector<int>> wallsByLevel;
    for (const auto& wall : walls) {
        const int wallId = next();
        wallsByLevel[wall.baseLevelId].push_back(wallId);
        out << "#" << wallId << "=IFCWALLSTANDARDCASE('Wall" << wall.id << "',#,$,$,$,$,$,$);\n";
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
        if (levelIfc.count(levelId) == 0U || wallIds.empty()) continue;
        const int rel = next();
        out << "#" << rel << "=IFCRELCONTAINEDINSPATIALSTRUCTURE('RelWalls',#,$,$,(";
        for (std::size_t i = 0; i < wallIds.size(); ++i) {
            out << "#" << wallIds[i];
            if (i + 1 < wallIds.size()) out << ",";
        }
        out << "),#" << levelIfc[levelId] << ");\n";
    }

    // Doors & Windows
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

    // Floors (IFCSLAB)
    for (const auto& fl : floors) {
        const int slabId = next();
        out << "#" << slabId << "=IFCSLAB('Floor" << fl.id << "',#,$,$,$,$,$,.FLOOR.);\n";
    }

    // Roofs (IFCROOF)
    for (const auto& rf : roofs) {
        const int roofId = next();
        out << "#" << roofId << "=IFCROOF('Roof" << rf.id << "',#,$,$,$,$,$,.NOTDEFINED.);\n";
    }

    // Ceilings (IFCCOVERING)
    for (const auto& cl : ceilings) {
        const int covId = next();
        out << "#" << covId << "=IFCCOVERING('Ceiling" << cl.id << "',#,$,$,$,$,$,.CEILING.);\n";
    }

    // Stairs (IFCSTAIR)
    for (const auto& st : stairs) {
        const int stairId = next();
        out << "#" << stairId << "=IFCSTAIR('Stair" << st.id << "',#,$,$,$,$,$,.STRAIGHT_RUN_STAIR.);\n";
    }

    // Railings (IFCRAILING)
    for (const auto& rl : railings) {
        const int railId = next();
        out << "#" << railId << "=IFCRAILING('Railing" << rl.id << "',#,$,$,$,$,$,.HANDRAIL.);\n";
    }

    // Curtain Walls (IFCCURTAINWALL)
    for (const auto& cw : curtainWalls) {
        const int cwId = next();
        out << "#" << cwId << "=IFCCURTAINWALL('CurtainWall" << cw.id << "',#,$,$,$,$,$,$);\n";
    }

    // Structural elements (IFCBEAM / IFCCOLUMN)
    for (const auto& se : structuralElements) {
        const int seId = next();
        if (se.isColumn) {
            out << "#" << seId << "=IFCCOLUMN('Column" << se.id << "',#,$,$,$,$,$,.COLUMN.);\n";
        } else {
            out << "#" << seId << "=IFCBEAM('Beam" << se.id << "',#,$,$,$,$,$,.BEAM.);\n";
        }
    }

    // Foundations (IFCFOOTING)
    for (const auto& fn : foundations) {
        const int fnId = next();
        out << "#" << fnId << "=IFCFOOTING('Footing" << fn.id << "',#,$,$,$,$,$,.STRIP_FOOTING.);\n";
    }

    // MEP Ducts & Pipes
    for (const auto& ds : ductSegments) {
        const int ductId = next();
        out << "#" << ductId << "=IFCDUCTSEGMENT('Duct" << ds.id << "',#,$,$,$,$,$,$);\n";
    }
    for (const auto& ps : pipeSegments) {
        const int pipeId = next();
        out << "#" << pipeId << "=IFCPIPESEGMENT('Pipe" << ps.id << "',#,$,$,$,$,$,$);\n";
    }
    for (const auto& ep : elecPanels) {
        const int pnlId = next();
        out << "#" << pnlId << "=IFCELECTRICDISTRIBUTIONBOARD('Panel" << ep.id << "',#,$,$,$,$,$,$);\n";
    }

    // Spaces & Rooms
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
        if (levelIfc.count(levelId) == 0U || spaceIds.empty()) continue;
        const int rel = next();
        out << "#" << rel << "=IFCRELCONTAINEDINSPATIALSTRUCTURE('RelSpaces',#,$,$,(";
        for (std::size_t i = 0; i < spaceIds.size(); ++i) {
            out << "#" << spaceIds[i];
            if (i + 1 < spaceIds.size()) out << ",";
        }
        out << "),#" << levelIfc[levelId] << ");\n";
    }

    out << "ENDSEC;\nEND-ISO-10303-21;\n";
    return out.str();
}

ProjectDocument ProjectDocument::importIfc4(std::string_view ifcData, std::string* error) {
    ProjectDocument doc;
    if (ifcData.find("ISO-10303-21") == std::string_view::npos) {
        if (error) *error = "Not a valid STEP/IFC physical file";
        return doc;
    }

    // Extract IFCBUILDINGSTOREY
    std::size_t pos = 0;
    while ((pos = ifcData.find("IFCBUILDINGSTOREY", pos)) != std::string_view::npos) {
        auto q1 = ifcData.find('\'', pos);
        if (q1 != std::string_view::npos) {
            auto q2 = ifcData.find('\'', q1 + 1);
            if (q2 != std::string_view::npos) {
                std::string lvlName(ifcData.substr(q1 + 1, q2 - q1 - 1));
                // Extract elevation if present after .ELEMENT.,
                double elev = 0.0;
                auto elPos = ifcData.find(".ELEMENT.,", q2);
                if (elPos != std::string_view::npos && elPos < ifcData.find(';', q2)) {
                    try {
                        elev = std::stod(std::string(ifcData.substr(elPos + 10, 10)));
                    } catch (...) {}
                }
                doc.addLevel(lvlName, elev);
            }
        }
        pos += 18;
    }

    // Default level if none found in IFC
    if (doc.levels.empty()) {
        doc.addLevel("Imported Level 1", 0.0);
    }
    TypeId defaultWallType = doc.addWallType("Imported Wall Type", 0.2, 3.0);

    // Extract IFCWALLSTANDARDCASE
    pos = 0;
    int wallIdx = 0;
    while ((pos = ifcData.find("IFCWALLSTANDARDCASE", pos)) != std::string_view::npos) {
        doc.addWall(defaultWallType, doc.levels.front().id, wallIdx * 5.0, 0.0,
                    (wallIdx + 1) * 5.0, 0.0);
        ++wallIdx;
        pos += 20;
    }

    // Extract IFCSPACE
    pos = 0;
    while ((pos = ifcData.find("IFCSPACE", pos)) != std::string_view::npos) {
        std::string spName = "Imported Space";
        auto q1 = ifcData.find('\'', pos);
        if (q1 != std::string_view::npos) {
            auto q2 = ifcData.find('\'', q1 + 1);
            if (q2 != std::string_view::npos) {
                spName = std::string(ifcData.substr(q1 + 1, q2 - q1 - 1));
            }
        }
        std::vector<std::pair<double, double>> boundary = {
            {0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {0.0, 4.0}
        };
        doc.addRoom(doc.levels.front().id, spName, boundary);
        pos += 9;
    }

    return doc;
}

// -----------------------------------------------------------------------------
// Vector / Printable SVG Export
// -----------------------------------------------------------------------------
std::string ProjectDocument::exportSvg(ElementId /*viewOrSheetId*/) const {
    std::ostringstream svg;
    svg << R"(<?xml version="1.0" encoding="UTF-8"?>)" << "\n";
    svg << R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1000 800" width="1000" height="800">)" << "\n";
    svg << R"(<rect width="1000" height="800" fill="#ffffff" stroke="#000000" stroke-width="2"/>)" << "\n";

    // Titleblock
    svg << R"(<text x="30" y="50" font-family="Arial" font-size="20" font-weight="bold">)"
        << escape(name) << R"( - BIM Sheet</text>)" << "\n";

    // Transform (center 500, 400, scale 30 px/meter)
    const double cx = 500.0;
    const double cy = 400.0;
    const double scale = 30.0;

    // Grids
    for (const auto& g : grids) {
        double sx0 = cx + g.x0 * scale;
        double sy0 = cy - g.y0 * scale;
        double sx1 = cx + g.x1 * scale;
        double sy1 = cy - g.y1 * scale;
        svg << R"(<line x1=")" << sx0 << R"(" y1=")" << sy0 << R"(" x2=")" << sx1 << R"(" y2=")"
            << sy1 << R"(" stroke="#888888" stroke-dasharray="6,4" stroke-width="1"/>)" << "\n";
        svg << R"(<circle cx=")" << sx1 << R"(" cy=")" << sy1 << R"(" r="12" fill="#ffffff" stroke="#888888"/>)" << "\n";
        svg << R"(<text x=")" << sx1 - 4 << R"(" y=")" << sy1 + 4 << R"(" font-family="Arial" font-size="10">)"
            << escape(g.name) << R"(</text>)" << "\n";
    }

    // Walls
    for (const auto& w : walls) {
        double sx0 = cx + w.x0 * scale;
        double sy0 = cy - w.y0 * scale;
        double sx1 = cx + w.x1 * scale;
        double sy1 = cy - w.y1 * scale;
        svg << R"(<line x1=")" << sx0 << R"(" y1=")" << sy0 << R"(" x2=")" << sx1 << R"(" y2=")"
            << sy1 << R"(" stroke="#222222" stroke-width="6" stroke-linecap="round"/>)" << "\n";
    }

    // Doors & Windows symbols
    for (const auto& d : doors) {
        if (const auto* w = findWall(d.hostWallId)) {
            double hx = w->x0 + (w->x1 - w->x0) * d.offsetAlongWall;
            double hy = w->y0 + (w->y1 - w->y0) * d.offsetAlongWall;
            double sx = cx + hx * scale;
            double sy = cy - hy * scale;
            svg << R"(<circle cx=")" << sx << R"(" cy=")" << sy << R"(" r="6" fill="#00aa00"/>)" << "\n";
        }
    }
    for (const auto& win : windows) {
        if (const auto* w = findWall(win.hostWallId)) {
            double hx = w->x0 + (w->x1 - w->x0) * win.offsetAlongWall;
            double hy = w->y0 + (w->y1 - w->y0) * win.offsetAlongWall;
            double sx = cx + hx * scale;
            double sy = cy - hy * scale;
            svg << R"(<circle cx=")" << sx << R"(" cy=")" << sy << R"(" r="5" fill="#0088ff"/>)" << "\n";
        }
    }

    // Rooms
    for (const auto& rm : rooms) {
        if (!rm.boundary.empty()) {
            double sumX = 0, sumY = 0;
            for (const auto& p : rm.boundary) {
                sumX += p.first;
                sumY += p.second;
            }
            double midX = cx + (sumX / static_cast<double>(rm.boundary.size())) * scale;
            double midY = cy - (sumY / static_cast<double>(rm.boundary.size())) * scale;
            svg << R"(<text x=")" << midX - 25 << R"(" y=")" << midY << R"(" font-family="Arial" font-size="12" fill="#444444">)"
                << escape(rm.name) << R"(</text>)" << "\n";
        }
    }

    svg << R"(</svg>)" << "\n";
    return svg.str();
}

} // namespace mydesk::buildings
