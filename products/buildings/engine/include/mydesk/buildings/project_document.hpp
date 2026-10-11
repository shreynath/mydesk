#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mydesk::buildings {

using ElementId = std::uint64_t;
using TypeId = std::uint64_t;
using LevelId = std::uint64_t;

// -----------------------------------------------------------------------------
// Parameters & Formulas
// -----------------------------------------------------------------------------
struct Parameter {
    std::string name;
    std::string value;
    std::string formula; // formula e.g. "Height*Width"
};

// -----------------------------------------------------------------------------
// Datums
// -----------------------------------------------------------------------------
struct Level {
    LevelId id{0};
    std::string name;
    double elevation{0.0};
};

struct Grid {
    ElementId id{0};
    std::string name;
    double x0{0.0};
    double y0{0.0};
    double x1{0.0};
    double y1{0.0};
};

// -----------------------------------------------------------------------------
// Materials
// -----------------------------------------------------------------------------
struct Material {
    TypeId id{0};
    std::string name;
    double densityKgM3{2400.0};          // e.g. concrete 2400 kg/m3
    double thermalConductivityW_mK{1.4}; // W/(m*K)
    double youngsModulusGPa{30.0};       // GPa
    double yieldStrengthMPa{25.0};       // MPa
    std::string colorHex{"#808080"};
};

// -----------------------------------------------------------------------------
// Architectural Elements
// -----------------------------------------------------------------------------
struct WallType {
    TypeId id{0};
    std::string name;
    double thickness{0.2};
    double defaultHeight{3.0};
    TypeId materialId{0};
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
    ElementId phaseCreatedId{0};
    ElementId phaseDemolishedId{0};
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

struct FloorType {
    TypeId id{0};
    std::string name;
    double thickness{0.25};
    TypeId materialId{0};
};

struct Floor {
    ElementId id{0};
    TypeId typeId{0};
    LevelId levelId{0};
    std::vector<std::pair<double, double>> boundary;
    double elevationOffset{0.0};
};

struct RoofType {
    TypeId id{0};
    std::string name;
    double thickness{0.3};
    double defaultSlopeDeg{20.0};
    TypeId materialId{0};
};

struct Roof {
    ElementId id{0};
    TypeId typeId{0};
    LevelId levelId{0};
    std::vector<std::pair<double, double>> boundary;
    double slopeDeg{20.0};
    double overhang{0.5};
};

struct CeilingType {
    TypeId id{0};
    std::string name;
    double thickness{0.02};
};

struct Ceiling {
    ElementId id{0};
    TypeId typeId{0};
    LevelId levelId{0};
    std::vector<std::pair<double, double>> boundary;
    double heightAboveLevel{2.7};
};

struct Stair {
    ElementId id{0};
    LevelId baseLevelId{0};
    LevelId topLevelId{0};
    int riserCount{16};
    double treadDepth{0.28};
    double width{1.1};
};

struct Railing {
    ElementId id{0};
    LevelId levelId{0};
    std::vector<std::pair<double, double>> path;
    double height{1.0};
    double balusterSpacing{0.15};
};

struct CurtainWall {
    ElementId id{0};
    LevelId baseLevelId{0};
    double x0{0.0};
    double y0{0.0};
    double x1{0.0};
    double y1{0.0};
    double height{4.0};
    double gridSpacingU{1.5};
    double gridSpacingV{2.0};
};

struct Room {
    ElementId id{0};
    LevelId levelId{0};
    std::string name;
    std::vector<std::pair<double, double>> boundary;
};

// -----------------------------------------------------------------------------
// Structural Elements
// -----------------------------------------------------------------------------
struct StructuralType {
    TypeId id{0};
    std::string name;
    std::string category; // "Beam" or "Column"
    double width{0.3};
    double depth{0.4};
    TypeId materialId{0};
};

struct StructuralElement {
    ElementId id{0};
    TypeId typeId{0};
    LevelId levelId{0};
    double x0{0.0};
    double y0{0.0};
    double z0{0.0};
    double x1{0.0};
    double y1{0.0};
    double z1{0.0};
    bool isColumn{false};
};

struct FoundationType {
    TypeId id{0};
    std::string name;
    double thickness{0.5};
    double bearingCapacityKPa{250.0};
};

struct Foundation {
    ElementId id{0};
    TypeId typeId{0};
    LevelId levelId{0};
    std::vector<std::pair<double, double>> boundary;
    double thickness{0.5};
};

// -----------------------------------------------------------------------------
// MEP Elements
// -----------------------------------------------------------------------------
struct Space {
    ElementId id{0};
    LevelId levelId{0};
    std::string name;
    std::vector<std::pair<double, double>> boundary;
    double targetLux{500.0};
    double designAirflowCfm{800.0};
    double lightingLoadWattsPerM2{12.0};
    double occupantDensityM2PerPerson{10.0};
};

struct DuctSystem {
    ElementId id{0};
    std::string name;
    std::string systemType; // "Supply", "Return", "Exhaust"
};

struct DuctSegment {
    ElementId id{0};
    ElementId systemId{0};
    LevelId levelId{0};
    double x0{0.0};
    double y0{0.0};
    double z0{0.0};
    double x1{0.0};
    double y1{0.0};
    double z1{0.0};
    double width{0.4};  // or diameter
    double height{0.3}; // 0 if round
    bool isRound{false};
    double airflowCfm{600.0};
};

struct PipeSystem {
    ElementId id{0};
    std::string name;
    std::string fluidType; // "DomesticColdWater", "DomesticHotWater", "Hydronic", "Sanitary"
};

struct PipeSegment {
    ElementId id{0};
    ElementId systemId{0};
    LevelId levelId{0};
    double x0{0.0};
    double y0{0.0};
    double z0{0.0};
    double x1{0.0};
    double y1{0.0};
    double z1{0.0};
    double diameterMm{50.0};
    double flowRateGpm{25.0};
};

struct ElecCircuit {
    ElementId id{0};
    ElementId panelId{0};
    int circuitNumber{1};
    double loadWatts{1500.0};
    double voltage{120.0};
    double breakerAmps{20.0};
};

struct ElecPanel {
    ElementId id{0};
    std::string name;
    double voltage{208.0};
    double mainsRatingAmps{100.0};
    std::vector<ElecCircuit> circuits;
};

// -----------------------------------------------------------------------------
// Project Collaboration, Options, Phasing
// -----------------------------------------------------------------------------
struct Phase {
    ElementId id{0};
    std::string name;
    int sequence{0};
};

struct DesignOption {
    ElementId id{0};
    std::string name;
    bool isPrimary{false};
    std::vector<ElementId> elementIds;
};

struct DesignOptionSet {
    ElementId id{0};
    std::string name;
    std::vector<DesignOption> options;
};

struct Workset {
    ElementId id{0};
    std::string name;
    std::string owner;
    bool isEditable{true};
};

// -----------------------------------------------------------------------------
// Families / Loadable Components
// -----------------------------------------------------------------------------
struct FamilyComponent {
    ElementId id{0};
    std::string familyName;
    std::string typeName;
    LevelId hostLevelId{0};
    double x{0.0};
    double y{0.0};
    double z{0.0};
    double rotationDeg{0.0};
    std::unordered_map<std::string, double> numericParameters;
    std::unordered_map<std::string, std::string> textParameters;
};

// -----------------------------------------------------------------------------
// Coordination & Clash Detection
// -----------------------------------------------------------------------------
struct BoundingBox3D {
    double minX{0.0}, minY{0.0}, minZ{0.0};
    double maxX{0.0}, maxY{0.0}, maxZ{0.0};

    [[nodiscard]] bool intersects(const BoundingBox3D& other) const {
        return (minX <= other.maxX && maxX >= other.minX) &&
               (minY <= other.maxY && maxY >= other.minY) &&
               (minZ <= other.maxZ && maxZ >= other.minZ);
    }

    [[nodiscard]] double intersectionVolume(const BoundingBox3D& other) const {
        if (!intersects(other)) return 0.0;
        double dx = std::min(maxX, other.maxX) - std::max(minX, other.minX);
        double dy = std::min(maxY, other.maxY) - std::max(minY, other.minY);
        double dz = std::min(maxZ, other.maxZ) - std::max(minZ, other.minZ);
        return (dx > 0 && dy > 0 && dz > 0) ? (dx * dy * dz) : 0.0;
    }
};

struct Interference {
    ElementId elementA{0};
    ElementId elementB{0};
    std::string categoryA;
    std::string categoryB;
    double clashVolume{0.0};
    std::string description;
};

// -----------------------------------------------------------------------------
// Documentation, Views, Sheets, Annotation
// -----------------------------------------------------------------------------
struct BldgView {
    ElementId id{0};
    std::string name;
    std::string viewType; // "FloorPlan", "Section", "Elevation", "3D"
    LevelId levelId{0};
    double scaleDenominator{100.0}; // 1:100
    double cropMinX{-100.0};
    double cropMinY{-100.0};
    double cropMaxX{100.0};
    double cropMaxY{100.0};
};

struct Sheet {
    ElementId id{0};
    std::string sheetNumber;
    std::string sheetName;
    double widthMm{841.0};  // A1 width
    double heightMm{594.0}; // A1 height
    std::vector<ElementId> placedViewIds;
};

struct Tag {
    ElementId id{0};
    ElementId targetElementId{0};
    std::string category; // "Wall", "Door", "Window", "Room"
    double x{0.0};
    double y{0.0};
    std::string label;
};

struct Dimension {
    ElementId id{0};
    ElementId elemAId{0};
    ElementId elemBId{0};
    std::string dimType{"Aligned"};
    double measuredValue{0.0};
    std::string textOverride;
};

struct DetailComponent {
    ElementId id{0};
    ElementId viewId{0};
    std::string familyName;
    double x{0.0};
    double y{0.0};
    double rotationDeg{0.0};
};

struct Keynote {
    std::string key;
    std::string text;
};

// -----------------------------------------------------------------------------
// Site / Topography
// -----------------------------------------------------------------------------
struct TopoPoint {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

struct TopoSurface {
    ElementId id{0};
    std::string name;
    std::vector<TopoPoint> points;
    std::vector<std::array<std::size_t, 3>> triangles;
};

// -----------------------------------------------------------------------------
// Groups, Links, Coordinates, Rendering
// -----------------------------------------------------------------------------
struct BldgGroup {
    ElementId id{0};
    std::string name;
    std::vector<ElementId> elementIds;
};

struct LinkedModel {
    ElementId id{0};
    std::string filePath;
    std::string linkName;
    double translationX{0.0};
    double translationY{0.0};
    double translationZ{0.0};
    double rotationDeg{0.0};
    bool isLoaded{true};
};

struct SharedCoordinates {
    double originX{0.0};
    double originY{0.0};
    double originZ{0.0};
    double angleToTrueNorthDeg{0.0};
    std::string coordinateSystemName{"Project-Local"};
};

struct RenderSettings {
    double sunAzimuthDeg{135.0};
    double sunAltitudeDeg{45.0};
    double exposureValue{14.0};
    bool enableShadows{true};
    bool enableArtificialLights{true};
};

// -----------------------------------------------------------------------------
// Schedules
// -----------------------------------------------------------------------------
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

struct AdvancedScheduleItem {
    ElementId elementId{0};
    std::string category;
    std::string typeName;
    std::string levelName;
    double primaryQuantity{0.0}; // length, area, volume or count
    std::string quantityUnit;
};

// -----------------------------------------------------------------------------
// Dynamo Visual Automation Graph
// -----------------------------------------------------------------------------
struct DynamoNode {
    std::string id;
    std::string nodeType; // "Number", "CreateWall", "SetElevation", "AddRoom"
    std::unordered_map<std::string, double> numericInputs;
    std::unordered_map<std::string, std::string> stringInputs;
};

struct DynamoGraph {
    std::vector<DynamoNode> nodes;
};

// -----------------------------------------------------------------------------
// ProjectDocument
// -----------------------------------------------------------------------------
struct ProjectDocument {
    std::string name{"Project1"};

    // Datum
    std::vector<Level> levels;
    std::vector<Grid> grids;

    // Materials
    std::vector<Material> materials;

    // Architectural
    std::vector<WallType> wallTypes;
    std::vector<Wall> walls;
    std::vector<DoorType> doorTypes;
    std::vector<Door> doors;
    std::vector<WindowType> windowTypes;
    std::vector<Window> windows;
    std::vector<FloorType> floorTypes;
    std::vector<Floor> floors;
    std::vector<RoofType> roofTypes;
    std::vector<Roof> roofs;
    std::vector<CeilingType> ceilingTypes;
    std::vector<Ceiling> ceilings;
    std::vector<Stair> stairs;
    std::vector<Railing> railings;
    std::vector<CurtainWall> curtainWalls;
    std::vector<Room> rooms;

    // Structural
    std::vector<StructuralType> structuralTypes;
    std::vector<StructuralElement> structuralElements;
    std::vector<FoundationType> foundationTypes;
    std::vector<Foundation> foundations;

    // MEP
    std::vector<Space> spaces;
    std::vector<DuctSystem> ductSystems;
    std::vector<DuctSegment> ductSegments;
    std::vector<PipeSystem> pipeSystems;
    std::vector<PipeSegment> pipeSegments;
    std::vector<ElecPanel> elecPanels;

    // Project & Coordination
    std::vector<Phase> phases;
    std::vector<DesignOptionSet> designOptionSets;
    std::vector<Workset> worksets;
    std::vector<FamilyComponent> familyComponents;
    std::vector<BldgGroup> groups;
    std::vector<LinkedModel> linkedModels;
    SharedCoordinates coordinates;
    RenderSettings renderSettings;
    TopoSurface topography;

    // Documentation
    std::vector<BldgView> views;
    std::vector<Sheet> sheets;
    std::vector<Tag> tags;
    std::vector<Dimension> dimensions;
    std::vector<DetailComponent> detailComponents;
    std::vector<Keynote> keynoteDatabase;

    // -------------------------------------------------------------------------
    // Element Factory Methods
    // -------------------------------------------------------------------------
    LevelId addLevel(std::string levelName, double elevation);
    ElementId addGrid(std::string gridName, double x0, double y0, double x1, double y1);
    TypeId addMaterial(std::string matName, double density, double thermalK, double youngsGPa, std::string hex);

    TypeId addWallType(std::string typeName, double thickness, double defaultHeight, TypeId materialId = 0);
    TypeId addDoorType(std::string typeName, double width, double height);
    TypeId addWindowType(std::string typeName, double width, double height, double sillHeight = 0.9);
    TypeId addFloorType(std::string typeName, double thickness, TypeId materialId = 0);
    TypeId addRoofType(std::string typeName, double thickness, double defaultSlope = 20.0, TypeId materialId = 0);
    TypeId addCeilingType(std::string typeName, double thickness);

    ElementId addWall(TypeId typeId, LevelId levelId, double x0, double y0, double x1, double y1);
    ElementId addDoor(TypeId typeId, ElementId hostWallId, double offsetAlongWall);
    ElementId addWindow(TypeId typeId, ElementId hostWallId, double offsetAlongWall);
    ElementId addFloor(TypeId typeId, LevelId levelId, std::vector<std::pair<double, double>> boundary, double offset = 0.0);
    ElementId addRoof(TypeId typeId, LevelId levelId, std::vector<std::pair<double, double>> boundary, double slope = 20.0, double overhang = 0.5);
    ElementId addCeiling(TypeId typeId, LevelId levelId, std::vector<std::pair<double, double>> boundary, double heightAboveLevel = 2.7);
    ElementId addStair(LevelId baseLevelId, LevelId topLevelId, int riserCount, double treadDepth, double width);
    ElementId addRailing(LevelId levelId, std::vector<std::pair<double, double>> path, double height = 1.0, double balusterSpacing = 0.15);
    ElementId addCurtainWall(LevelId levelId, double x0, double y0, double x1, double y1, double height, double spacingU, double spacingV);
    ElementId addRoom(LevelId levelId, std::string roomName, std::vector<std::pair<double, double>> boundary);

    // Structural
    TypeId addStructuralType(std::string typeName, std::string category, double width, double depth, TypeId materialId = 0);
    ElementId addStructuralElement(TypeId typeId, LevelId levelId, double x0, double y0, double z0, double x1, double y1, double z1, bool isColumn);
    TypeId addFoundationType(std::string typeName, double thickness, double bearingKPa);
    ElementId addFoundation(TypeId typeId, LevelId levelId, std::vector<std::pair<double, double>> boundary, double thickness);

    // MEP
    ElementId addSpace(LevelId levelId, std::string spaceName, std::vector<std::pair<double, double>> boundary, double targetLux = 500.0, double airflowCfm = 800.0);
    ElementId addDuctSystem(std::string sysName, std::string systemType);
    ElementId addDuctSegment(ElementId systemId, LevelId levelId, double x0, double y0, double z0, double x1, double y1, double z1, double widthOrDia, double height, bool isRound, double airflowCfm);
    ElementId addPipeSystem(std::string sysName, std::string fluidType);
    ElementId addPipeSegment(ElementId systemId, LevelId levelId, double x0, double y0, double z0, double x1, double y1, double z1, double diameterMm, double flowRateGpm);
    ElementId addElecPanel(std::string panelName, double voltage, double mainsAmps);
    ElementId addElecCircuit(ElementId panelId, int circuitNumber, double loadWatts, double voltage, double breakerAmps);

    // Phasing, Options, Worksets, Families, Groups, Topo, Links
    ElementId addPhase(std::string phaseName, int sequence);
    ElementId addDesignOptionSet(std::string setName);
    ElementId addDesignOption(ElementId setId, std::string optName, bool isPrimary);
    ElementId addWorkset(std::string worksetName, std::string owner);
    ElementId addFamilyComponent(std::string familyName, std::string typeName, LevelId levelId, double x, double y, double z);
    ElementId addGroup(std::string groupName, std::vector<ElementId> elementIds);
    ElementId duplicateGroup(ElementId groupId, double dx, double dy, double dz);
    void setTopography(std::string topoName, std::vector<TopoPoint> points, std::vector<std::array<std::size_t, 3>> triangles);
    ElementId addLinkedModel(std::string filePath, std::string linkName, double tx = 0.0, double ty = 0.0, double tz = 0.0);

    // Documentation
    ElementId addView(std::string viewName, std::string viewType, LevelId levelId, double scale = 100.0);
    ElementId addSheet(std::string number, std::string sheetTitle, double widthMm = 841.0, double heightMm = 594.0);
    bool placeViewOnSheet(ElementId sheetId, ElementId viewId);
    ElementId addTag(ElementId targetElemId, std::string category, double x, double y, std::string label);
    ElementId addDimension(ElementId elemAId, ElementId elemBId, std::string dimType, double measuredValue, std::string textOverride = "");
    ElementId addDetailComponent(ElementId viewId, std::string familyName, double x, double y, double rotationDeg = 0.0);
    void addKeynote(std::string key, std::string text);

    // Lookups
    bool setLevelElevation(LevelId id, double elevation);
    [[nodiscard]] const Level* findLevel(LevelId id) const;
    [[nodiscard]] const Wall* findWall(ElementId id) const;
    [[nodiscard]] const WallType* findWallType(TypeId id) const;
    [[nodiscard]] const DoorType* findDoorType(TypeId id) const;
    [[nodiscard]] const WindowType* findWindowType(TypeId id) const;
    [[nodiscard]] const Room* findRoom(ElementId id) const;
    [[nodiscard]] const Material* findMaterial(TypeId id) const;

    // Computational & Analysis Methods
    [[nodiscard]] double wallLength(const Wall& wall) const;
    [[nodiscard]] double wallHeight(const Wall& wall) const;
    [[nodiscard]] double wallArea(const Wall& wall) const;
    [[nodiscard]] double wallNetArea(const Wall& wall) const;
    [[nodiscard]] double wallBaseElevation(const Wall& wall) const;
    [[nodiscard]] double wallTopElevation(const Wall& wall) const;

    [[nodiscard]] double floorArea(const Floor& floor) const;
    [[nodiscard]] double floorVolume(const Floor& floor) const;
    [[nodiscard]] double roofFootprintArea(const Roof& roof) const;
    [[nodiscard]] double roofSurfaceArea(const Roof& roof) const;
    [[nodiscard]] double ceilingArea(const Ceiling& ceiling) const;

    [[nodiscard]] double stairRiserHeight(const Stair& stair) const;
    [[nodiscard]] double stairRunLength(const Stair& stair) const;
    [[nodiscard]] double stairSlopeAngleDeg(const Stair& stair) const;

    [[nodiscard]] double railingLength(const Railing& railing) const;
    [[nodiscard]] std::size_t railingBalusterCount(const Railing& railing) const;

    [[nodiscard]] std::size_t curtainWallPanelCount(const CurtainWall& cw) const;
    [[nodiscard]] double curtainWallMullionLength(const CurtainWall& cw) const;

    [[nodiscard]] double roomArea(const Room& room) const;
    [[nodiscard]] double roomVolume(const Room& room, double height = 3.0) const;

    [[nodiscard]] double spaceArea(const Space& space) const;
    [[nodiscard]] double spaceVolume(const Space& space, double height = 3.0) const;
    [[nodiscard]] double spaceTotalLightingWatts(const Space& space) const;

    [[nodiscard]] double structuralLength(const StructuralElement& elem) const;
    [[nodiscard]] double structuralVolume(const StructuralElement& elem) const;
    [[nodiscard]] double structuralWeightKg(const StructuralElement& elem) const;

    [[nodiscard]] double foundationArea(const Foundation& fnd) const;
    [[nodiscard]] double foundationVolume(const Foundation& fnd) const;
    [[nodiscard]] double foundationAllowableLoadKn(const Foundation& fnd) const;

    [[nodiscard]] double ductVelocityFpm(const DuctSegment& duct) const;
    [[nodiscard]] double ductFrictionLossInWg(const DuctSegment& duct) const;

    [[nodiscard]] double pipeVelocityMps(const PipeSegment& pipe) const;
    [[nodiscard]] double pipeFrictionLossMeters(const PipeSegment& pipe) const;

    [[nodiscard]] double panelConnectedLoadWatts(const ElecPanel& panel) const;
    [[nodiscard]] double panelDemandCurrentAmps(const ElecPanel& panel) const;
    [[nodiscard]] double panelCapacityUtilizationPercent(const ElecPanel& panel) const;

    [[nodiscard]] double topoSurfaceArea() const;
    [[nodiscard]] double topoProjectedArea() const;
    [[nodiscard]] std::pair<double, double> topoElevationRange() const;

    // Formula & Parameter Evaluation
    static double evaluateFormula(std::string_view expression, const std::unordered_map<std::string, double>& context);

    // Clash Detection / Interference Check
    [[nodiscard]] std::vector<Interference> checkInterference() const;

    // Advanced Schedules
    [[nodiscard]] std::vector<ScheduleRow> wallScheduleByType() const;
    [[nodiscard]] std::vector<RoomScheduleRow> roomScheduleByLevel() const;
    [[nodiscard]] std::vector<AdvancedScheduleItem> generateAdvancedSchedule(std::string_view categoryFilter) const;

    // Dynamo Graph Execution
    bool executeDynamoGraph(const DynamoGraph& graph);

    // Persistence & Exchange
    [[nodiscard]] std::string serializeNative() const;
    static ProjectDocument deserializeNative(std::string_view json, std::string* error = nullptr);

    [[nodiscard]] std::vector<std::uint8_t> serializePackageRvt() const;
    static ProjectDocument deserializePackageRvt(const std::vector<std::uint8_t>& bytes, std::string* error = nullptr);

    [[nodiscard]] std::string exportIfc4() const;
    static ProjectDocument importIfc4(std::string_view ifcData, std::string* error = nullptr);

    [[nodiscard]] std::string exportSvg(ElementId viewOrSheetId = 0) const;

private:
    LevelId nextLevelId_{1};
    TypeId nextTypeId_{1};
    ElementId nextElementId_{1};

    static double computePolygonArea(const std::vector<std::pair<double, double>>& boundary);
};

} // namespace mydesk::buildings
