// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <cstddef>
#include <cstdint>
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
    void setName(std::string name) { name_ = std::move(name); }

    [[nodiscard]] const std::string& crs() const { return crs_; }
    void setCrs(std::string crs) { crs_ = std::move(crs); }

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

    // Bridges
    ElementId addBridge(std::string name, ElementId roadId, double startSta, double endSta, int piers);
    [[nodiscard]] const std::vector<BridgeStructure>& bridges() const { return bridges_; }

    // City Buildings
    ElementId addBuilding(std::string name, std::string usage, double roofHeight,
                          std::vector<std::pair<double, double>> footprint);
    [[nodiscard]] const std::vector<CityBuilding>& buildings() const { return buildings_; }

    // Interoperability & GeoJSON
    [[nodiscard]] std::string exportGeoJson() const;

    // Native persistence
    [[nodiscard]] std::string serializeNative() const;
    static InfraDocument deserializeNative(std::string_view json, std::string* error = nullptr);

private:
    std::string name_{"Untitled_Infra"};
    std::string crs_{"EPSG:3857"}; // Web Mercator default
    DemGrid dem_;
    std::vector<ComponentRoad> roads_;
    std::vector<BridgeStructure> bridges_;
    std::vector<CityBuilding> buildings_;
    ElementId nextId_{1};
};

} // namespace mydesk::infra
