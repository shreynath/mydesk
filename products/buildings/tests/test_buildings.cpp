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
    const auto wallCount = std::count(ifc.begin(), ifc.end(), '\n');
    expect(wallCount > 20, "ifc substantive");
    expect(std::count(ifc.begin(), ifc.end(), '#') >= 10, "many ifc entities");
}

void test_windows_rooms_and_net_area() {
    mydesk::buildings::ProjectDocument project;
    const auto l1 = project.addLevel("Ground", 0.0);
    const auto wt = project.addWallType("ExteriorWall", 0.25, 3.0);
    const auto dt = project.addDoorType("EntryDoor", 1.0, 2.0); // 2.0 m2
    const auto winT = project.addWindowType("StdWindow", 1.5, 1.2, 0.9); // 1.8 m2

    const auto w1 = project.addWall(wt, l1, 0.0, 0.0, 10.0, 0.0); // 10m length * 3m = 30 m2 gross
    project.addDoor(dt, w1, 0.2);
    project.addWindow(winT, w1, 0.7);

    const auto* wallPtr = project.findWall(w1);
    expect(wallPtr != nullptr, "wall found");
    const double gross = project.wallArea(*wallPtr);
    expect(std::abs(gross - 30.0) < 1e-9, "gross area 30");

    const double net = project.wallNetArea(*wallPtr);
    // deductions: door 2.0 m2 + window 1.8 m2 = 3.8 m2; net = 26.2 m2
    expect(std::abs(net - 26.2) < 1e-9, "net area 26.2 after door and window opening deduction");

    // Add room with 4 corners (5m x 4m rectangle = 20 m2)
    std::vector<std::pair<double, double>> boundary = {
        {0.0, 0.0}, {5.0, 0.0}, {5.0, 4.0}, {0.0, 4.0}
    };
    const auto r1 = project.addRoom(l1, "Living Room", boundary);
    const auto* roomPtr = project.findRoom(r1);
    expect(roomPtr != nullptr, "room found");
    const double rArea = project.roomArea(*roomPtr);
    expect(std::abs(rArea - 20.0) < 1e-9, "shoelace room area 20");
    const double rVol = project.roomVolume(*roomPtr, 3.0);
    expect(std::abs(rVol - 60.0) < 1e-9, "room volume 60");

    const auto roomSched = project.roomScheduleByLevel();
    expect(roomSched.size() == 1, "one room in schedule");
    expect(roomSched.front().roomName == "Living Room", "room name in schedule");
    expect(std::abs(roomSched.front().area - 20.0) < 1e-9, "scheduled room area");

    // Serialization roundtrip
    const auto json = project.serializeNative();
    std::string err;
    auto reloaded = mydesk::buildings::ProjectDocument::deserializeNative(json, &err);
    expect(reloaded.windowTypes.size() == 1, "windowTypes reloaded");
    expect(reloaded.windows.size() == 1, "windows reloaded");
    expect(reloaded.windows.front().hostWallId == w1, "window host preserved");
    expect(reloaded.rooms.size() == 1, "rooms reloaded");
    expect(reloaded.rooms.front().boundary.size() == 4, "room boundary reloaded");

    // IFC4 export check
    const auto ifc = reloaded.exportIfc4();
    expect(ifc.find("IFCWINDOW") != std::string::npos, "IFCWINDOW present in IFC4");
    expect(ifc.find("IFCSPACE") != std::string::npos, "IFCSPACE present in IFC4");
    expect(ifc.find("GrossFloorArea") != std::string::npos, "room gross floor area in IFC4");
    expect(ifc.find("NetArea") != std::string::npos, "wall net area property in IFC4");
}

void test_grids_and_datums() {
    mydesk::buildings::ProjectDocument project;
    auto g1 = project.addGrid("1", 0.0, 0.0, 0.0, 20.0);
    auto g2 = project.addGrid("A", 0.0, 0.0, 30.0, 0.0);
    (void)g1; (void)g2;
    expect(project.grids.size() == 2, "two grids added");
    expect(project.grids[0].name == "1" && project.grids[1].name == "A", "grid names match");
    const auto ifc = project.exportIfc4();
    expect(ifc.find("IFCGRID") != std::string::npos, "IFCGRID exported in IFC4");
}

void test_floors_area_volume() {
    mydesk::buildings::ProjectDocument project;
    auto l1 = project.addLevel("Ground", 0.0);
    auto ft = project.addFloorType("Concrete Slab 200mm", 0.2);
    std::vector<std::pair<double, double>> boundary = {
        {0.0, 0.0}, {10.0, 0.0}, {10.0, 8.0}, {0.0, 8.0}
    };
    auto fl = project.addFloor(ft, l1, boundary);
    (void)fl;
    expect(project.floors.size() == 1, "floor added");
    double area = project.floorArea(project.floors.front());
    double volume = project.floorVolume(project.floors.front());
    expect(std::abs(area - 80.0) < 1e-6, "floor area 80 m2");
    expect(std::abs(volume - 16.0) < 1e-6, "floor volume 16 m3");
    const auto ifc = project.exportIfc4();
    expect(ifc.find("IFCSLAB") != std::string::npos, "IFCSLAB exported in IFC4");
}

void test_roofs_slope_area() {
    mydesk::buildings::ProjectDocument project;
    auto l2 = project.addLevel("Level 2", 3.5);
    auto rt = project.addRoofType("Pitched Roof", 0.25, 30.0);
    std::vector<std::pair<double, double>> boundary = {
        {0.0, 0.0}, {12.0, 0.0}, {12.0, 8.0}, {0.0, 8.0}
    };
    auto rf = project.addRoof(rt, l2, boundary, 30.0, 0.6);
    (void)rf;
    expect(project.roofs.size() == 1, "roof added");
    double fp = project.roofFootprintArea(project.roofs.front());
    double surf = project.roofSurfaceArea(project.roofs.front());
    expect(std::abs(fp - 96.0) < 1e-6, "roof footprint 96 m2");
    expect(surf > fp, "sloped roof surface exceeds footprint");
    const auto ifc = project.exportIfc4();
    expect(ifc.find("IFCROOF") != std::string::npos, "IFCROOF exported in IFC4");
}

void test_ceilings_modeling() {
    mydesk::buildings::ProjectDocument project;
    auto l1 = project.addLevel("Ground", 0.0);
    auto ct = project.addCeilingType("Acoustic Tile", 0.02);
    std::vector<std::pair<double, double>> boundary = {
        {0.0, 0.0}, {6.0, 0.0}, {6.0, 5.0}, {0.0, 5.0}
    };
    project.addCeiling(ct, l1, boundary, 2.7);
    expect(project.ceilings.size() == 1, "ceiling added");
    double area = project.ceilingArea(project.ceilings.front());
    expect(std::abs(area - 30.0) < 1e-6, "ceiling area 30 m2");
    const auto ifc = project.exportIfc4();
    expect(ifc.find("IFCCOVERING") != std::string::npos, "IFCCOVERING exported in IFC4");
}

void test_stairs_risers_run() {
    mydesk::buildings::ProjectDocument project;
    auto l1 = project.addLevel("Ground", 0.0);
    auto l2 = project.addLevel("First", 3.2);
    auto stId = project.addStair(l1, l2, 16, 0.28, 1.2);
    (void)stId;
    expect(project.stairs.size() == 1, "stair added");
    const auto& st = project.stairs.front();
    double rh = project.stairRiserHeight(st);
    double run = project.stairRunLength(st);
    double slope = project.stairSlopeAngleDeg(st);
    expect(std::abs(rh - 0.20) < 1e-6, "riser height 0.20m");
    expect(std::abs(run - 4.20) < 1e-6, "run length 4.20m");
    expect(slope > 30.0 && slope < 40.0, "comfortable stair slope angle");
    const auto ifc = project.exportIfc4();
    expect(ifc.find("IFCSTAIR") != std::string::npos, "IFCSTAIR exported in IFC4");
}

void test_railings_path_balusters() {
    mydesk::buildings::ProjectDocument project;
    auto l1 = project.addLevel("Ground", 0.0);
    std::vector<std::pair<double, double>> path = {
        {0.0, 0.0}, {4.0, 0.0}, {4.0, 3.0}
    };
    project.addRailing(l1, path, 1.05, 0.15);
    expect(project.railings.size() == 1, "railing added");
    const auto& rl = project.railings.front();
    double len = project.railingLength(rl);
    std::size_t balusters = project.railingBalusterCount(rl);
    expect(std::abs(len - 7.0) < 1e-6, "railing length 7m");
    expect(balusters >= 47, "baluster count correct");
    const auto ifc = project.exportIfc4();
    expect(ifc.find("IFCRAILING") != std::string::npos, "IFCRAILING exported in IFC4");
}

void test_curtain_walls_panels_mullions() {
    mydesk::buildings::ProjectDocument project;
    auto l1 = project.addLevel("Ground", 0.0);
    project.addCurtainWall(l1, 0.0, 0.0, 9.0, 0.0, 4.0, 1.5, 2.0);
    expect(project.curtainWalls.size() == 1, "curtain wall added");
    const auto& cw = project.curtainWalls.front();
    std::size_t panelCount = project.curtainWallPanelCount(cw);
    double mullionLen = project.curtainWallMullionLength(cw);
    expect(panelCount == 12, "12 glass panels (6x2 grid)");
    expect(mullionLen > 0.0, "mullions computed");
    const auto ifc = project.exportIfc4();
    expect(ifc.find("IFCCURTAINWALL") != std::string::npos, "IFCCURTAINWALL exported in IFC4");
}

void test_mep_spaces_loads() {
    mydesk::buildings::ProjectDocument project;
    auto l1 = project.addLevel("Ground", 0.0);
    std::vector<std::pair<double, double>> boundary = {
        {0.0, 0.0}, {8.0, 0.0}, {8.0, 6.0}, {0.0, 6.0}
    };
    project.addSpace(l1, "Open Office", boundary, 500.0, 1200.0);
    expect(project.spaces.size() == 1, "space added");
    const auto& sp = project.spaces.front();
    double sArea = project.spaceArea(sp);
    double sVol = project.spaceVolume(sp, 3.0);
    double watts = project.spaceTotalLightingWatts(sp);
    expect(std::abs(sArea - 48.0) < 1e-6, "space area 48 m2");
    expect(std::abs(sVol - 144.0) < 1e-6, "space volume 144 m3");
    expect(watts > 500.0, "lighting load evaluated");
}

void test_family_components_parameters() {
    mydesk::buildings::ProjectDocument project;
    auto l1 = project.addLevel("Ground", 0.0);
    auto compId = project.addFamilyComponent("M_Desk", "1500x750", l1, 3.0, 2.5, 0.0);
    (void)compId;
    expect(project.familyComponents.size() == 1, "family component added");
    expect(project.familyComponents.front().familyName == "M_Desk", "family name matches");
}

void test_formula_evaluation() {
    std::unordered_map<std::string, double> ctx = {
        {"Width", 3.0}, {"Length", 5.0}, {"Factor", 2.0}
    };
    double val1 = mydesk::buildings::ProjectDocument::evaluateFormula("Width * Length", ctx);
    double val2 = mydesk::buildings::ProjectDocument::evaluateFormula("(Width + Length) * Factor", ctx);
    expect(std::abs(val1 - 15.0) < 1e-9, "formula Width*Length = 15");
    expect(std::abs(val2 - 16.0) < 1e-9, "formula (Width+Length)*Factor = 16");
}

void test_design_options_sets() {
    mydesk::buildings::ProjectDocument project;
    auto setId = project.addDesignOptionSet("Entry Layout");
    auto opt1 = project.addDesignOption(setId, "Option 1 - Revolving Door", true);
    auto opt2 = project.addDesignOption(setId, "Option 2 - Double Sliding Door", false);
    (void)opt1; (void)opt2;
    expect(project.designOptionSets.size() == 1, "design option set created");
    expect(project.designOptionSets.front().options.size() == 2, "2 options in set");
}

void test_project_phasing() {
    mydesk::buildings::ProjectDocument project;
    auto ph1 = project.addPhase("Existing", 1);
    auto ph2 = project.addPhase("Demolition", 2);
    auto ph3 = project.addPhase("New Construction", 3);
    (void)ph1; (void)ph2; (void)ph3;
    expect(project.phases.size() == 3, "3 phases created");
    expect(project.phases[2].name == "New Construction", "phase sequence correct");
}

void test_worksets_collaboration() {
    mydesk::buildings::ProjectDocument project;
    auto ws1 = project.addWorkset("Architecture Core", "Architect1");
    auto ws2 = project.addWorkset("MEP Systems", "Engineer1");
    (void)ws1; (void)ws2;
    expect(project.worksets.size() == 2, "worksets created");
    expect(project.worksets[0].owner == "Architect1", "workset owner preserved");
}

void test_views_and_crop() {
    mydesk::buildings::ProjectDocument project;
    auto l1 = project.addLevel("Ground", 0.0);
    auto vId = project.addView("Level 1 Floor Plan", "FloorPlan", l1, 100.0);
    (void)vId;
    expect(project.views.size() == 1, "view created");
    expect(project.views.front().scaleDenominator == 100.0, "scale 1:100");
}

void test_sheets_placement() {
    mydesk::buildings::ProjectDocument project;
    auto l1 = project.addLevel("Ground", 0.0);
    auto vId = project.addView("Level 1 Floor Plan", "FloorPlan", l1, 100.0);
    auto shId = project.addSheet("A101", "Plans and Sections", 841.0, 594.0);
    bool placed = project.placeViewOnSheet(shId, vId);
    expect(placed, "view placed on sheet");
    expect(project.sheets.front().placedViewIds.size() == 1, "sheet records placed view");
}

void test_tags_annotation() {
    mydesk::buildings::ProjectDocument project;
    auto l1 = project.addLevel("Ground", 0.0);
    auto wt = project.addWallType("Generic 200", 0.2, 3.0);
    auto w1 = project.addWall(wt, l1, 0, 0, 6, 0);
    auto tagId = project.addTag(w1, "Wall", 3.0, 0.2, "W1-200");
    (void)tagId;
    expect(project.tags.size() == 1, "tag created");
    expect(project.tags.front().label == "W1-200", "tag label matches");
}

void test_dimensions_measurement() {
    mydesk::buildings::ProjectDocument project;
    auto dId = project.addDimension(1, 2, "Aligned", 5.25, "");
    (void)dId;
    expect(project.dimensions.size() == 1, "dimension created");
    expect(std::abs(project.dimensions.front().measuredValue - 5.25) < 1e-9, "measured value preserved");
}

void test_parametric_wall_host_dependency_chain() {
    using namespace mydesk::buildings;
    ProjectDocument project;
    project.name = "Parametric Core";
    const auto baseLevel = project.addLevel("Level 1", 0.0);
    const auto topLevel = project.addLevel("Level 2", 3.0);
    const auto wallType = project.addWallType("Interior 150", 0.15, 2.8);
    const auto doorType = project.addDoorType("Single 900", 0.9, 2.1);
    const auto windowType = project.addWindowType("Window 1200", 1.2, 1.5, 0.9);
    const auto wallId = project.addWall(wallType, baseLevel, 0.0, 0.0, 10.0, 0.0);
    const auto doorId = project.addDoor(doorType, wallId, 0.25);
    const auto windowId = project.addWindow(windowType, wallId, 0.75);
    expect(doorId != 0 && windowId != 0, "hosted openings created on valid wall");
    expect(project.addDoor(doorType, wallId, 1.2) == 0,
           "opening outside normalized host range is rejected");

    expect(project.setWallTopConstraint(wallId, topLevel, 0.2),
           "wall constrained to upper level with offset");
    const auto* wall = project.findWall(wallId);
    expect(wall != nullptr, "constrained wall found");
    expect(std::abs(project.wallHeight(*wall) - 3.2) < 1e-9,
           "top constraint drives wall height");
    expect(project.undo(), "top constraint undo succeeds");
    expect(std::abs(project.wallHeight(*wall) - 2.8) < 1e-9,
           "undo restores unconnected type height");
    expect(project.redo(), "top constraint redo succeeds");
    expect(std::abs(project.wallHeight(*wall) - 3.2) < 1e-9,
           "redo restores level constraint");

    const auto dimensionId = project.addAssociativeDimension(doorId, windowId, "Aligned");
    const auto* dimension = project.findDimension(dimensionId);
    expect(dimension != nullptr && dimension->associative, "associative dimension created");
    expect(std::abs(project.dimensionValue(*dimension) - 5.0) < 1e-9,
           "dimension derives from hosted opening references");

    expect(project.moveWall(wallId, 2.0, 1.0, 2.0, 13.0), "wall geometry edit succeeds");
    auto doorPosition = project.elementPosition(doorId);
    auto windowPosition = project.elementPosition(windowId);
    expect(doorPosition && windowPosition, "hosted opening positions resolve");
    expect(std::abs(doorPosition->x - 2.0) < 1e-9 &&
               std::abs(doorPosition->y - 4.0) < 1e-9,
           "door follows moved and rotated host at normalized offset");
    expect(std::abs(windowPosition->y - 10.0) < 1e-9,
           "window follows moved host at normalized offset");
    expect(std::abs(project.dimensionValue(*dimension) - 6.0) < 1e-9,
           "associative dimension recomputes after host edit");

    expect(project.undo(), "wall move undo succeeds");
    expect(std::abs(project.dimensionValue(*dimension) - 5.0) < 1e-9,
           "undo propagates to hosted references and dimension");
    expect(project.redo(), "wall move redo succeeds");
    expect(std::abs(project.dimensionValue(*dimension) - 6.0) < 1e-9,
           "redo propagates dependency chain");

    expect(project.setLevelElevation(topLevel, 4.0), "upper level edit succeeds");
    expect(std::abs(project.wallHeight(*wall) - 4.2) < 1e-9,
           "constrained wall follows upper level");
    expect(project.setLevelElevation(baseLevel, 0.5), "base level edit succeeds");
    expect(std::abs(project.wallHeight(*wall) - 3.7) < 1e-9,
           "base-level move preserves constrained world top");
    windowPosition = project.elementPosition(windowId);
    expect(windowPosition && std::abs(windowPosition->z - 1.4) < 1e-9,
           "window sill elevation follows base level");
    expect(!project.setLevelElevation(baseLevel, 4.3),
           "edit that would invert constrained wall is atomically rejected");
    expect(std::abs(project.findLevel(baseLevel)->elevation - 0.5) < 1e-9,
           "rejected edit leaves level unchanged");
    expect(project.undo(), "base-level edit undo succeeds");
    expect(std::abs(project.wallHeight(*wall) - 4.2) < 1e-9,
           "undo restores dependent wall height");
    expect(project.redo(), "base-level edit redo succeeds");
    expect(std::abs(project.wallHeight(*wall) - 3.7) < 1e-9,
           "redo reapplies dependent wall height");

    const auto native = project.serializeNative();
    std::string error;
    auto restored = ProjectDocument::deserializeNative(native, &error);
    expect(error.empty(), "parametric document roundtrip has no error");
    expect(restored.doorTypes.size() == 1, "hosted door type persists");
    const auto* restoredWall = restored.findWall(wallId);
    const auto* restoredDimension = restored.findDimension(dimensionId);
    expect(restoredWall && restoredWall->topConstraintLevelId == topLevel,
           "wall top constraint persists");
    expect(std::abs(restored.wallHeight(*restoredWall) - 3.7) < 1e-9,
           "restored constraint evaluates against restored levels");
    expect(restoredDimension && restoredDimension->associative,
           "associative dimension metadata persists");
    expect(std::abs(restored.dimensionValue(*restoredDimension) - 6.0) < 1e-9,
           "restored dimension remains live against host geometry");
    const auto ifc = restored.exportIfc4();
    expect(ifc.find("'TopElevation'") != std::string::npos,
           "IFC export includes propagated constrained top elevation");
}

void test_advanced_schedules_filtering() {
    mydesk::buildings::ProjectDocument project;
    auto l1 = project.addLevel("Ground", 0.0);
    auto wt = project.addWallType("Exterior 300", 0.3, 3.0);
    project.addWall(wt, l1, 0, 0, 10, 0);
    project.addWall(wt, l1, 10, 0, 10, 5);

    auto items = project.generateAdvancedSchedule("Wall");
    expect(items.size() == 2, "2 items in filtered schedule");
    expect(items.front().category == "Wall", "category matches");
}

void test_materials_thermal_structural() {
    mydesk::buildings::ProjectDocument project;
    auto mId = project.addMaterial("C30/37 Concrete", 2400.0, 1.6, 33.0, "#A0A0A0");
    expect(project.materials.size() == 1, "material added");
    const auto* mat = project.findMaterial(mId);
    expect(mat != nullptr, "material found");
    expect(mat->densityKgM3 == 2400.0, "density matches");
    const auto ifc = project.exportIfc4();
    expect(ifc.find("IFCMATERIAL") != std::string::npos, "IFCMATERIAL exported in IFC4");
}

void test_render_settings() {
    mydesk::buildings::ProjectDocument project;
    project.renderSettings.exposureValue = 13.5;
    project.renderSettings.sunAltitudeDeg = 50.0;
    expect(project.renderSettings.enableShadows, "shadows enabled");
    expect(project.renderSettings.sunAltitudeDeg == 50.0, "sun altitude set");
}

void test_structural_framing_columns_beams() {
    mydesk::buildings::ProjectDocument project;
    auto l1 = project.addLevel("Ground", 0.0);
    auto stCol = project.addStructuralType("UC 305x305x97", "Column", 0.305, 0.305);
    auto stBeam = project.addStructuralType("UB 457x191x67", "Beam", 0.191, 0.457);

    auto colId = project.addStructuralElement(stCol, l1, 0, 0, 0, 0, 0, 3.5, true);
    auto bmId = project.addStructuralElement(stBeam, l1, 0, 0, 3.5, 6, 0, 3.5, false);
    (void)colId; (void)bmId;

    expect(project.structuralElements.size() == 2, "structural elements added");
    double colLen = project.structuralLength(project.structuralElements[0]);
    double bmLen = project.structuralLength(project.structuralElements[1]);
    expect(std::abs(colLen - 3.5) < 1e-6, "column height 3.5m");
    expect(std::abs(bmLen - 6.0) < 1e-6, "beam span 6.0m");
    expect(project.structuralWeightKg(project.structuralElements[0]) > 0.0, "weight evaluated");
    const auto ifc = project.exportIfc4();
    expect(ifc.find("IFCCOLUMN") != std::string::npos, "IFCCOLUMN in IFC4");
    expect(ifc.find("IFCBEAM") != std::string::npos, "IFCBEAM in IFC4");
}

void test_foundations_bearing_capacity() {
    mydesk::buildings::ProjectDocument project;
    auto l0 = project.addLevel("Foundation", -1.5);
    auto ft = project.addFoundationType("Pad Footing 600mm", 0.6, 300.0);
    std::vector<std::pair<double, double>> boundary = {
        {0, 0}, {2.0, 0}, {2.0, 2.0}, {0, 2.0}
    };
    project.addFoundation(ft, l0, boundary, 0.6);
    expect(project.foundations.size() == 1, "foundation added");
    const auto& f = project.foundations.front();
    double area = project.foundationArea(f);
    double load = project.foundationAllowableLoadKn(f);
    expect(std::abs(area - 4.0) < 1e-6, "pad footing area 4 m2");
    expect(std::abs(load - 1200.0) < 1e-6, "bearing capacity 1200 kN");
    const auto ifc = project.exportIfc4();
    expect(ifc.find("IFCFOOTING") != std::string::npos, "IFCFOOTING in IFC4");
}

void test_mep_duct_airflow_velocity() {
    mydesk::buildings::ProjectDocument project;
    auto l1 = project.addLevel("Ground", 0.0);
    auto sys = project.addDuctSystem("Supply Air", "Supply");
    project.addDuctSegment(sys, l1, 0, 0, 2.8, 10, 0, 2.8, 0.5, 0.4, false, 1200.0);
    expect(project.ductSegments.size() == 1, "duct segment added");
    const auto& d = project.ductSegments.front();
    double vel = project.ductVelocityFpm(d);
    double loss = project.ductFrictionLossInWg(d);
    expect(vel > 0.0, "duct velocity computed");
    expect(loss > 0.0, "duct friction loss computed");
    const auto ifc = project.exportIfc4();
    expect(ifc.find("IFCDUCTSEGMENT") != std::string::npos, "IFCDUCTSEGMENT in IFC4");
}

void test_mep_pipe_flow_friction() {
    mydesk::buildings::ProjectDocument project;
    auto l1 = project.addLevel("Ground", 0.0);
    auto sys = project.addPipeSystem("Domestic Cold Water", "DomesticColdWater");
    project.addPipeSegment(sys, l1, 0, 0, 0.5, 15, 0, 0.5, 50.0, 30.0);
    expect(project.pipeSegments.size() == 1, "pipe segment added");
    const auto& p = project.pipeSegments.front();
    double v = project.pipeVelocityMps(p);
    double hf = project.pipeFrictionLossMeters(p);
    expect(v > 0.0, "pipe velocity evaluated");
    expect(hf > 0.0, "pipe head loss evaluated");
    const auto ifc = project.exportIfc4();
    expect(ifc.find("IFCPIPESEGMENT") != std::string::npos, "IFCPIPESEGMENT in IFC4");
}

void test_mep_electrical_panel_load() {
    mydesk::buildings::ProjectDocument project;
    auto pnl = project.addElecPanel("Panel-LP1", 208.0, 100.0);
    project.addElecCircuit(pnl, 1, 1800.0, 120.0, 20.0);
    project.addElecCircuit(pnl, 2, 2400.0, 120.0, 20.0);
    expect(project.elecPanels.size() == 1, "panel added");
    const auto& ep = project.elecPanels.front();
    double totalWatts = project.panelConnectedLoadWatts(ep);
    double amps = project.panelDemandCurrentAmps(ep);
    double util = project.panelCapacityUtilizationPercent(ep);
    expect(std::abs(totalWatts - 4200.0) < 1e-6, "connected load 4200W");
    expect(amps > 0.0, "demand current computed");
    expect(util < 100.0, "panel within mains rating");
    const auto ifc = project.exportIfc4();
    expect(ifc.find("IFCELECTRICDISTRIBUTIONBOARD") != std::string::npos, "electric board in IFC4");
}

void test_interference_check_clash_detection() {
    mydesk::buildings::ProjectDocument project;
    auto l1 = project.addLevel("Ground", 0.0);
    auto wt = project.addWallType("Wall 200", 0.2, 3.0);
    auto w1 = project.addWall(wt, l1, 0, 0, 10, 0);
    (void)w1;

    auto sys = project.addDuctSystem("Supply", "Supply");
    // Duct crossing directly through the wall at (5, 0, 2.5)
    project.addDuctSegment(sys, l1, 5.0, -1.0, 2.5, 5.0, 1.0, 2.5, 0.4, 0.4, false, 800.0);

    auto clashes = project.checkInterference();
    expect(!clashes.empty(), "clash detected between duct and wall");
    expect(clashes.front().clashVolume > 0.0, "clash volume positive");
}

void test_rvt_package_roundtrip() {
    mydesk::buildings::ProjectDocument project;
    project.name = "Hospital_Wing_A";
    auto l1 = project.addLevel("L1", 0.0);
    auto wt = project.addWallType("Exterior 300", 0.3, 3.5);
    project.addWall(wt, l1, 0, 0, 20, 0);

    auto bytes = project.serializePackageRvt();
    expect(!bytes.empty(), "package RVT serialized");
    std::string err;
    auto loaded = mydesk::buildings::ProjectDocument::deserializePackageRvt(bytes, &err);
    expect(loaded.name == "Hospital_Wing_A", "RVT package roundtrip name matches");
    expect(loaded.levels.size() == 1, "RVT package levels preserved");
    expect(loaded.walls.size() == 1, "RVT package walls preserved");
}

void test_ifc4_import() {
    std::string ifcSample =
        "ISO-10303-21;\n"
        "HEADER;\n"
        "FILE_DESCRIPTION(('IFC4'),'2;1');\n"
        "FILE_NAME('test.ifc','',(),(),'','','');\n"
        "FILE_SCHEMA(('IFC4'));\n"
        "ENDSEC;\n"
        "DATA;\n"
        "#1=IFCBUILDINGSTOREY('Ground Floor',$,$,$,$,$,$,$,.ELEMENT.,0.0);\n"
        "#2=IFCBUILDINGSTOREY('First Floor',$,$,$,$,$,$,$,.ELEMENT.,3.2);\n"
        "#3=IFCWALLSTANDARDCASE('Wall1',$,$,$,$,$,$,$);\n"
        "#4=IFCWALLSTANDARDCASE('Wall2',$,$,$,$,$,$,$);\n"
        "#5=IFCSPACE('Office Suite',$,$,$,$,$,$,$,.ELEMENT.,$);\n"
        "ENDSEC;\n"
        "END-ISO-10303-21;\n";

    std::string err;
    auto doc = mydesk::buildings::ProjectDocument::importIfc4(ifcSample, &err);
    expect(doc.levels.size() >= 2, "imported storeys as levels");
    expect(doc.walls.size() >= 2, "imported walls");
    expect(doc.rooms.size() >= 1, "imported spaces as rooms");
}

void test_dynamo_automation_graph() {
    mydesk::buildings::ProjectDocument project;
    mydesk::buildings::DynamoGraph graph;
    mydesk::buildings::DynamoNode node1;
    node1.id = "node1";
    node1.nodeType = "CreateWall";
    node1.numericInputs["x0"] = 0.0;
    node1.numericInputs["y0"] = 0.0;
    node1.numericInputs["x1"] = 15.0;
    node1.numericInputs["y1"] = 0.0;
    graph.nodes.push_back(node1);

    bool ok = project.executeDynamoGraph(graph);
    expect(ok, "dynamo graph executed");
    expect(project.walls.size() >= 1, "dynamo generated wall");
}

void test_groups_and_duplication() {
    mydesk::buildings::ProjectDocument project;
    auto l1 = project.addLevel("L1", 0.0);
    auto wt = project.addWallType("Interior 100", 0.1, 2.8);
    auto w1 = project.addWall(wt, l1, 0, 0, 4, 0);
    auto w2 = project.addWall(wt, l1, 4, 0, 4, 3);
    auto grpId = project.addGroup("Restroom Pod", {w1, w2});
    expect(project.groups.size() == 1, "group created");

    auto copyId = project.duplicateGroup(grpId, 10.0, 0.0, 0.0);
    expect(copyId != 0, "group duplicated");
    expect(project.walls.size() == 4, "walls duplicated with group");
}

void test_linked_models() {
    mydesk::buildings::ProjectDocument project;
    auto linkId = project.addLinkedModel("/projects/structural_model.rvt", "Structural_Model", 0, 0, 0);
    (void)linkId;
    expect(project.linkedModels.size() == 1, "linked model added");
    expect(project.linkedModels.front().linkName == "Structural_Model", "link name matches");
}

void test_shared_coordinates() {
    mydesk::buildings::ProjectDocument project;
    project.coordinates.originX = 500000.0;
    project.coordinates.originY = 200000.0;
    project.coordinates.originZ = 45.0;
    project.coordinates.angleToTrueNorthDeg = 15.5;
    expect(project.coordinates.originX == 500000.0, "survey easting set");
    expect(project.coordinates.angleToTrueNorthDeg == 15.5, "true north rotation set");
}

void test_topography_surface_area() {
    mydesk::buildings::ProjectDocument project;
    std::vector<mydesk::buildings::TopoPoint> pts = {
        {0, 0, 0}, {10, 0, 2}, {10, 10, 5}, {0, 10, 1}
    };
    std::vector<std::array<std::size_t, 3>> tris = {
        {0, 1, 2}, {0, 2, 3}
    };
    project.setTopography("Site Terrain", pts, tris);
    double surfArea = project.topoSurfaceArea();
    double projArea = project.topoProjectedArea();
    auto [minZ, maxZ] = project.topoElevationRange();
    expect(surfArea > 100.0, "3D sloped terrain surface area > 100");
    expect(std::abs(projArea - 100.0) < 1e-6, "projected footprint area 100");
    expect(minZ == 0.0 && maxZ == 5.0, "elevation range 0 to 5m");
}

void test_detail_components() {
    mydesk::buildings::ProjectDocument project;
    auto l1 = project.addLevel("L1", 0.0);
    auto vId = project.addView("Detail View", "Section", l1);
    auto dcId = project.addDetailComponent(vId, "Batt_Insulation", 2.0, 1.5, 0.0);
    (void)dcId;
    expect(project.detailComponents.size() == 1, "detail component added");
}

void test_keynote_database() {
    mydesk::buildings::ProjectDocument project;
    project.addKeynote("03 30 00", "Cast-in-Place Concrete");
    project.addKeynote("04 20 00", "Unit Masonry");
    expect(project.keynoteDatabase.size() == 2, "keynotes added");
    expect(project.keynoteDatabase[0].key == "03 30 00", "keynote key preserved");
}

void test_print_svg_export() {
    mydesk::buildings::ProjectDocument project;
    auto l1 = project.addLevel("L1", 0.0);
    auto wt = project.addWallType("Exterior Wall", 0.3, 3.0);
    project.addWall(wt, l1, -5.0, -3.0, 5.0, -3.0);
    project.addGrid("1", -5.0, -5.0, -5.0, 5.0);

    auto svg = project.exportSvg();
    expect(svg.find("<svg") != std::string::npos, "valid SVG opened");
    expect(svg.find("</svg>") != std::string::npos, "valid SVG closed");
    expect(svg.find("BIM Sheet") != std::string::npos, "title included");
}

} // namespace

int main() {
    try {
        const char* kTests[] = {
            "test_multistory_schedule_ifc_roundtrip",
            "test_windows_rooms_and_net_area",
            "test_grids_and_datums",
            "test_floors_area_volume",
            "test_roofs_slope_area",
            "test_ceilings_modeling",
            "test_stairs_risers_run",
            "test_railings_path_balusters",
            "test_curtain_walls_panels_mullions",
            "test_mep_spaces_loads",
            "test_family_components_parameters",
            "test_formula_evaluation",
            "test_design_options_sets",
            "test_project_phasing",
            "test_worksets_collaboration",
            "test_views_and_crop",
            "test_sheets_placement",
            "test_tags_annotation",
            "test_dimensions_measurement",
            "test_parametric_wall_host_dependency_chain",
            "test_advanced_schedules_filtering",
            "test_materials_thermal_structural",
            "test_render_settings",
            "test_structural_framing_columns_beams",
            "test_foundations_bearing_capacity",
            "test_mep_duct_airflow_velocity",
            "test_mep_pipe_flow_friction",
            "test_mep_electrical_panel_load",
            "test_interference_check_clash_detection",
            "test_rvt_package_roundtrip",
            "test_ifc4_import",
            "test_dynamo_automation_graph",
            "test_groups_and_duplication",
            "test_linked_models",
            "test_shared_coordinates",
            "test_topography_surface_area",
            "test_detail_components",
            "test_keynote_database",
            "test_print_svg_export"
        };
        (void)kTests;

        test_multistory_schedule_ifc_roundtrip();
        std::cout << "[PASS] test_multistory_schedule_ifc_roundtrip\n";

        test_windows_rooms_and_net_area();
        std::cout << "[PASS] test_windows_rooms_and_net_area\n";

        test_grids_and_datums();
        std::cout << "[PASS] test_grids_and_datums\n";

        test_floors_area_volume();
        std::cout << "[PASS] test_floors_area_volume\n";

        test_roofs_slope_area();
        std::cout << "[PASS] test_roofs_slope_area\n";

        test_ceilings_modeling();
        std::cout << "[PASS] test_ceilings_modeling\n";

        test_stairs_risers_run();
        std::cout << "[PASS] test_stairs_risers_run\n";

        test_railings_path_balusters();
        std::cout << "[PASS] test_railings_path_balusters\n";

        test_curtain_walls_panels_mullions();
        std::cout << "[PASS] test_curtain_walls_panels_mullions\n";

        test_mep_spaces_loads();
        std::cout << "[PASS] test_mep_spaces_loads\n";

        test_family_components_parameters();
        std::cout << "[PASS] test_family_components_parameters\n";

        test_formula_evaluation();
        std::cout << "[PASS] test_formula_evaluation\n";

        test_design_options_sets();
        std::cout << "[PASS] test_design_options_sets\n";

        test_project_phasing();
        std::cout << "[PASS] test_project_phasing\n";

        test_worksets_collaboration();
        std::cout << "[PASS] test_worksets_collaboration\n";

        test_views_and_crop();
        std::cout << "[PASS] test_views_and_crop\n";

        test_sheets_placement();
        std::cout << "[PASS] test_sheets_placement\n";

        test_tags_annotation();
        std::cout << "[PASS] test_tags_annotation\n";

        test_dimensions_measurement();
        std::cout << "[PASS] test_dimensions_measurement\n";

        test_parametric_wall_host_dependency_chain();
        std::cout << "[PASS] test_parametric_wall_host_dependency_chain\n";

        test_advanced_schedules_filtering();
        std::cout << "[PASS] test_advanced_schedules_filtering\n";

        test_materials_thermal_structural();
        std::cout << "[PASS] test_materials_thermal_structural\n";

        test_render_settings();
        std::cout << "[PASS] test_render_settings\n";

        test_structural_framing_columns_beams();
        std::cout << "[PASS] test_structural_framing_columns_beams\n";

        test_foundations_bearing_capacity();
        std::cout << "[PASS] test_foundations_bearing_capacity\n";

        test_mep_duct_airflow_velocity();
        std::cout << "[PASS] test_mep_duct_airflow_velocity\n";

        test_mep_pipe_flow_friction();
        std::cout << "[PASS] test_mep_pipe_flow_friction\n";

        test_mep_electrical_panel_load();
        std::cout << "[PASS] test_mep_electrical_panel_load\n";

        test_interference_check_clash_detection();
        std::cout << "[PASS] test_interference_check_clash_detection\n";

        test_rvt_package_roundtrip();
        std::cout << "[PASS] test_rvt_package_roundtrip\n";

        test_ifc4_import();
        std::cout << "[PASS] test_ifc4_import\n";

        test_dynamo_automation_graph();
        std::cout << "[PASS] test_dynamo_automation_graph\n";

        test_groups_and_duplication();
        std::cout << "[PASS] test_groups_and_duplication\n";

        test_linked_models();
        std::cout << "[PASS] test_linked_models\n";

        test_shared_coordinates();
        std::cout << "[PASS] test_shared_coordinates\n";

        test_topography_surface_area();
        std::cout << "[PASS] test_topography_surface_area\n";

        test_detail_components();
        std::cout << "[PASS] test_detail_components\n";

        test_keynote_database();
        std::cout << "[PASS] test_keynote_database\n";

        test_print_svg_export();
        std::cout << "[PASS] test_print_svg_export\n";

        std::cout << "39 passed, 0 failed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cout << "[FAIL] " << ex.what() << "\n0 passed, 1 failed\n";
        return EXIT_FAILURE;
    }
}
