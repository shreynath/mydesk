// SPDX-License-Identifier: MPL-2.0
// MyDesk Infra — Regression & Verification Tests
#include "mydesk/infra/infra_document.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
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
    const auto roadId = doc.addRoad("Loop\"Road", 4, 3.5, 90.0, {{0,0,0}, {10,0,1}});
    doc.addBridge("Crossing", roadId, 1.0, 9.0, 2);
    doc.addBuilding("Depot", "Industrial", 8.0, {{1,1}, {2,1}, {2,2}, {1,2}});

    const auto json = doc.serializeNative();
    CHECK(!json.empty());

    std::string err;
    InfraDocument loaded = InfraDocument::deserializeNative(json, &err);
    CHECK(err.empty());
    CHECK(loaded.name() == "MetropolitanStudy");
    CHECK(loaded.crs() == "EPSG:3857");
    CHECK(loaded.roads().size() == 1);
    CHECK(loaded.roads().front().name == "Loop\"Road");
    CHECK(loaded.roads().front().centerline.size() == 2);
    CHECK_NEAR(loaded.roads().front().centerline.back().elevation, 1.0, 1e-12);
    CHECK(loaded.bridges().size() == 1);
    CHECK(loaded.bridges().front().roadId == roadId);
    CHECK(loaded.buildings().size() == 1);
    CHECK(loaded.buildings().front().footprint.size() == 4);
    CHECK(loaded.dem().cols == 2);
    CHECK(loaded.dem().rows == 2);
    CHECK(!loaded.canUndo());
}

static void test_infra_corridor_earthworks() {
    InfraDocument doc;
    doc.setDemGrid(-10.0, -10.0, 10.0, 13, 3, std::vector<double>(39, 100.0));
    const auto roadId = doc.addRoad("BalancedGrade", 2, 3.5, 80.0, {
        {0.0, 0.0, 98.0},
        {100.0, 0.0, 102.0}
    });

    const auto report = doc.analyzeRoadEarthworks(roadId, 25.0, 1.5, 1.0);
    CHECK(report.has_value());
    CHECK(report->samples.size() == 5);
    CHECK_NEAR(report->formationWidthMeters, 10.0, 1e-9);
    CHECK_NEAR(report->maximumGradePercent, 4.0, 1e-9);
    CHECK_NEAR(report->cutVolumeCubicMeters, 500.0, 1e-6);
    CHECK_NEAR(report->fillVolumeCubicMeters, 500.0, 1e-6);
    CHECK_NEAR(report->samples.front().cutAreaSquareMeters, 20.0, 1e-6);
    CHECK_NEAR(report->samples.back().fillAreaSquareMeters, 20.0, 1e-6);
    CHECK(!doc.analyzeRoadEarthworks(roadId, 0.0).has_value());
}

static void test_infra_road_edit_undo_redo() {
    InfraDocument doc;
    doc.setDemGrid(0.0, 0.0, 10.0, 2, 2, {0, 0, 0, 0});
    doc.clearHistory();
    const auto roadId = doc.addRoad("Editable", 2, 3.5, 60.0, {{0,0,0}, {10,0,0}});
    CHECK(doc.canUndo());
    CHECK(doc.updateRoadCenterline(roadId, {{0,0,0}, {20,0,2}}));
    CHECK_NEAR(doc.findRoad(roadId)->totalLengthMeters(), std::sqrt(404.0), 1e-9);
    CHECK(doc.undo());
    CHECK_NEAR(doc.findRoad(roadId)->totalLengthMeters(), 10.0, 1e-9);
    CHECK(doc.redo());
    CHECK_NEAR(doc.findRoad(roadId)->totalLengthMeters(), std::sqrt(404.0), 1e-9);

    doc.addBridge("Attached", roadId, 1.0, 10.0, 1);
    CHECK(doc.removeRoad(roadId));
    CHECK(doc.findRoad(roadId) == nullptr);
    CHECK(doc.bridges().empty());
    CHECK(doc.undo());
    CHECK(doc.findRoad(roadId) != nullptr);
    CHECK(doc.bridges().size() == 1);
}

static void test_infra_native_validation_and_file_io() {
    std::string error;
    const auto malformed = InfraDocument::deserializeNative(
        R"({"format":"mydesk-infra","schemaVersion":2,"name":"x","crs":"EPSG:3857",)", &error);
    (void)malformed;
    CHECK(!error.empty());

    error.clear();
    InfraDocument doc;
    doc.setDemGrid(0.0, 0.0, 10.0, 2, 2, {1, 2, 3, 4});
    doc.addRoad("Saved", 2, 3.5, 50.0, {{0,0,1}, {10,0,2}});
    std::string versionOne = doc.serializeNative();
    const auto versionField = versionOne.find("\"schemaVersion\"");
    CHECK(versionField != std::string::npos);
    if (versionField != std::string::npos) {
        const auto lineStart = versionOne.rfind('\n', versionField);
        const auto lineEnd = versionOne.find('\n', versionField);
        const auto eraseStart = lineStart != std::string::npos && lineStart > 0 && versionOne[lineStart - 1] == ','
            ? lineStart - 1 : (lineStart == std::string::npos ? 0 : lineStart + 1);
        versionOne.erase(eraseStart, lineEnd - eraseStart);
    }
    const auto migrated = InfraDocument::deserializeNative(versionOne, &error);
    CHECK(error.empty());
    CHECK(migrated.roads().size() == 1);

    InfraDocument empty;
    const auto emptyReloaded = InfraDocument::deserializeNative(empty.serializeNative(), &error);
    CHECK(error.empty());
    CHECK(emptyReloaded.dem().elevations.empty());

    const auto path = std::filesystem::temp_directory_path() / "mydesk_infra_atomic_save_test.myinfra";
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    CHECK(doc.saveNativeFile(path, &error));
    CHECK(error.empty());
    const auto loaded = InfraDocument::loadNativeFile(path, &error);
    CHECK(loaded.has_value());
    CHECK(error.empty());
    CHECK(loaded->roads().size() == 1);
    CHECK(loaded->roads().front().centerline.size() == 2);
    std::filesystem::remove(path, ignored);
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
    // "test_infra_corridor_earthworks"
    test_infra_corridor_earthworks();
    // "test_infra_road_edit_undo_redo"
    test_infra_road_edit_undo_redo();
    // "test_infra_native_validation_and_file_io"
    test_infra_native_validation_and_file_io();

    if (g_failed == 0) {
        std::printf("All Infra tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d Infra test(s) FAILED.\n", g_failed);
    return 1;
}
