// SPDX-License-Identifier: MPL-2.0
// MyDesk Infra — Regression & Verification Tests
#include "mydesk/infra/infra_document.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

using namespace mydesk::infra;

static int g_failed = 0;
#define CHECK(cond) \
    do { if (!(cond)) { \
        std::fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, #cond); \
        ++g_failed; \
    } } while(0)

#define CHECK_NEAR(a, b, eps) \
    do { double _a = (a), _b = (b), _e = (eps); \
         if (std::abs(_a - _b) > _e) { \
             std::fprintf(stderr, "FAIL [line %d]: |%.6g - %.6g| > %.6g\n", \
                          __LINE__, _a, _b, _e); \
             ++g_failed; \
    } } while(0)

static void test_infra_dem_interpolation() {
    InfraDocument doc;
    // 2x2 grid, corners at:
    // (0,0)->10.0, (10,0)->20.0
    // (0,10)->10.0, (10,10)->20.0
    std::vector<double> elevs = {10.0, 20.0, 10.0, 20.0};
    doc.setDemGrid(0.0, 0.0, 10.0, 2, 2, std::move(elevs));

    auto zMid = doc.terrainElevationAt(5.0, 5.0);
    CHECK(zMid.has_value());
    CHECK_NEAR(*zMid, 15.0, 1e-6);

    auto zCorner = doc.terrainElevationAt(10.0, 0.0);
    CHECK(zCorner.has_value());
    CHECK_NEAR(*zCorner, 20.0, 1e-6);

    // Outside bounds returns nullopt
    auto zOut = doc.terrainElevationAt(15.0, 5.0);
    CHECK(!zOut.has_value());
}

static void test_infra_road_alignment_length() {
    InfraDocument doc;
    const auto rId = doc.addRoad("Highway_101", 4, 3.5, 100.0, {
        {0.0, 0.0, 0.0},
        {100.0, 0.0, 0.0},
        {100.0, 100.0, 0.0}
    });

    const auto* road = doc.findRoad(rId);
    CHECK(road != nullptr);
    CHECK_NEAR(road->totalLengthMeters(), 200.0, 1e-6);
}

static void test_infra_bridge_structure() {
    InfraDocument doc;
    const auto rId = doc.addRoad("BridgeApproach", 2, 3.5, 60.0, {
        {0.0, 0.0, 10.0},
        {200.0, 0.0, 10.0}
    });
    const auto bId = doc.addBridge("Viaduct_A", rId, 50.0, 150.0, 4);
    CHECK(bId > 0);
    CHECK(doc.bridges().size() == 1);
    CHECK(doc.bridges().front().pierCount == 4);
}

static void test_infra_building_footprints() {
    InfraDocument doc;
    // Set elevation grid so building base gets populated
    doc.setDemGrid(0.0, 0.0, 100.0, 2, 2, {50.0, 50.0, 50.0, 50.0});

    const auto bId = doc.addBuilding("Tower_1", "Commercial", 45.0, {
        {10.0, 10.0},
        {30.0, 10.0},
        {30.0, 30.0},
        {10.0, 30.0}
    });
    CHECK(bId > 0);
    CHECK(doc.buildings().size() == 1);
    CHECK_NEAR(doc.buildings().front().baseElevation, 50.0, 1e-6);
    CHECK_NEAR(doc.buildings().front().roofHeightMeters, 45.0, 1e-6);
}

static void test_infra_geojson_export() {
    InfraDocument doc;
    doc.addRoad("Avenue_A", 2, 3.0, 50.0, {{0,0,0}, {50,50,0}});
    doc.addBuilding("Hall", "Civic", 12.0, {{0,0}, {10,0}, {10,10}, {0,10}});

    const auto geojson = doc.exportGeoJson();
    CHECK(geojson.find("\"type\": \"FeatureCollection\"") != std::string::npos);
    CHECK(geojson.find("\"name\": \"Avenue_A\"") != std::string::npos);
    CHECK(geojson.find("\"name\": \"Hall\"") != std::string::npos);
    CHECK(geojson.find("\"type\": \"LineString\"") != std::string::npos);
    CHECK(geojson.find("\"type\": \"Polygon\"") != std::string::npos);
}

static void test_infra_native_roundtrip() {
    InfraDocument doc;
    doc.setName("MetropolitanStudy");
    doc.setCrs("EPSG:3857");
    doc.setDemGrid(0, 0, 10, 2, 2, {1, 2, 3, 4});
    doc.addRoad("LoopRoad", 4, 3.5, 90.0, {{0,0,0}, {10,0,0}});

    const auto json = doc.serializeNative();
    CHECK(!json.empty());

    std::string err;
    InfraDocument loaded = InfraDocument::deserializeNative(json, &err);
    CHECK(err.empty());
    CHECK(loaded.name() == "MetropolitanStudy");
    CHECK(loaded.crs() == "EPSG:3857");
    CHECK(loaded.roads().size() == 1);
    CHECK(loaded.dem().cols == 2);
    CHECK(loaded.dem().rows == 2);
}

int main() {
    // Registered capability test identifiers:
    // "test_infra_dem_interpolation"
    test_infra_dem_interpolation();
    // "test_infra_road_alignment_length"
    test_infra_road_alignment_length();
    // "test_infra_bridge_structure"
    test_infra_bridge_structure();
    // "test_infra_building_footprints"
    test_infra_building_footprints();
    // "test_infra_geojson_export"
    test_infra_geojson_export();
    // "test_infra_native_roundtrip"
    test_infra_native_roundtrip();

    if (g_failed == 0) {
        std::printf("All Infra tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d Infra test(s) FAILED.\n", g_failed);
    return 1;
}
