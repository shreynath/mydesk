// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mydesk::infra {

using ElementId = std::uint64_t;

struct DemGrid {
    double originEasting{0.0};
    double originNorthing{0.0};
    double cellSizeMeters{10.0};
    std::size_t cols{0};
    std::size_t rows{0};
    std::vector<double> elevations; // row-major

    [[nodiscard]] std::optional<double> elevationAt(double easting, double northing) const;
};

struct RoadVertex {
    double easting{0.0};
    double northing{0.0};
    double elevation{0.0};
};

struct ComponentRoad {
    ElementId id{0};
    std::string name;
    int numLanes{2};
    double laneWidthMeters{3.5};
    double designSpeedKmh{80.0};
    std::vector<RoadVertex> centerline;

    [[nodiscard]] double totalLengthMeters() const;
};

struct CorridorSample {
    double stationMeters{0.0};
    RoadVertex centerline;
    double terrainElevation{0.0};
    double gradePercent{0.0};
    double cutAreaSquareMeters{0.0};
    double fillAreaSquareMeters{0.0};
};

struct CorridorEarthworkReport {
    ElementId roadId{0};
    double formationWidthMeters{0.0};
    double cutVolumeCubicMeters{0.0};
    double fillVolumeCubicMeters{0.0};
    double maximumGradePercent{0.0};
    std::vector<CorridorSample> samples;
};

struct BridgeStructure {
    ElementId id{0};
    std::string name;
    ElementId roadId{0};
    double startStation{0.0};
    double endStation{0.0};
    int pierCount{2};
    double deckThicknessMeters{1.2};
};

struct CityBuilding {
    ElementId id{0};
    std::string name;
    std::string usageCategory; // "Residential", "Commercial", "Civic"
    double baseElevation{0.0};
    double roofHeightMeters{15.0};
    std::vector<std::pair<double, double>> footprint; // 2D polygon (easting, northing)
};

class InfraDocument {
public:
    InfraDocument();

    [[nodiscard]] const std::string& name() const { return name_; }
    void setName(std::string name);

    [[nodiscard]] const std::string& crs() const { return crs_; }
    void setCrs(std::string crs);

    // Terrain DEM
    void setDemGrid(double originEasting, double originNorthing, double cellSize,
                    std::size_t cols, std::size_t rows, std::vector<double> elevations);
    [[nodiscard]] const DemGrid& dem() const { return dem_; }
    [[nodiscard]] std::optional<double> terrainElevationAt(double easting, double northing) const;

    // Component Roads
    ElementId addRoad(std::string name, int numLanes, double laneWidth, double designSpeed,
                      std::vector<RoadVertex> centerline);
    [[nodiscard]] const std::vector<ComponentRoad>& roads() const { return roads_; }
    [[nodiscard]] const ComponentRoad* findRoad(ElementId id) const;
    bool updateRoad(ElementId id, std::string name, int numLanes, double laneWidth,
                    double designSpeed);
    bool updateRoadCenterline(ElementId id, std::vector<RoadVertex> centerline);
    bool removeRoad(ElementId id);

    // Samples the design profile and terrain across the full formation width.
    // Cross-section cut/fill areas are integrated by trapezoids; volumes use
    // the average-end-area method between successive stations.
    [[nodiscard]] std::optional<CorridorEarthworkReport> analyzeRoadEarthworks(
        ElementId roadId, double stationIntervalMeters, double shoulderWidthMeters = 1.5,
        double crossSectionIntervalMeters = 1.0) const;

    // Bridges
    ElementId addBridge(std::string name, ElementId roadId, double startSta, double endSta, int piers);
    [[nodiscard]] const std::vector<BridgeStructure>& bridges() const { return bridges_; }
    [[nodiscard]] const BridgeStructure* findBridge(ElementId id) const;
    bool updateBridge(ElementId id, std::string name, double startSta, double endSta,
                      int piers, double deckThickness);

    // City Buildings
    ElementId addBuilding(std::string name, std::string usage, double roofHeight,
                          std::vector<std::pair<double, double>> footprint);
    [[nodiscard]] const std::vector<CityBuilding>& buildings() const { return buildings_; }
    [[nodiscard]] const CityBuilding* findBuilding(ElementId id) const;
    bool updateBuilding(ElementId id, std::string name, std::string usage, double roofHeight);

    // Interoperability & GeoJSON
    [[nodiscard]] std::string exportGeoJson() const;

    // Native persistence
    [[nodiscard]] std::string serializeNative() const;
    static InfraDocument deserializeNative(std::string_view json, std::string* error = nullptr);
    [[nodiscard]] bool saveNativeFile(const std::filesystem::path& path, std::string* error = nullptr) const;
    static std::optional<InfraDocument> loadNativeFile(const std::filesystem::path& path,
                                                        std::string* error = nullptr);

    [[nodiscard]] bool canUndo() const { return !undoStack_.empty(); }
    [[nodiscard]] bool canRedo() const { return !redoStack_.empty(); }
    bool undo();
    bool redo();
    void clearHistory();

private:
    struct State {
        std::string name;
        std::string crs;
        DemGrid dem;
        std::vector<ComponentRoad> roads;
        std::vector<BridgeStructure> bridges;
        std::vector<CityBuilding> buildings;
        ElementId nextId{1};
    };

    [[nodiscard]] State captureState() const;
    void restoreState(State state);
    void checkpoint();

    std::string name_{"Untitled_Infra"};
    std::string crs_{"EPSG:3857"}; // Web Mercator default
    DemGrid dem_;
    std::vector<ComponentRoad> roads_;
    std::vector<BridgeStructure> bridges_;
    std::vector<CityBuilding> buildings_;
    ElementId nextId_{1};
    std::vector<State> undoStack_;
    std::vector<State> redoStack_;
};

} // namespace mydesk::infra
