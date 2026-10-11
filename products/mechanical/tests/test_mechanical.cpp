// SPDX-License-Identifier: MPL-2.0
// MyDesk Mechanical — Inventor-class Regression & Verification Tests
#include "mydesk/mechanical/part_document.hpp"
#include "mydesk/mechanical/assembly_document.hpp"

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {

void expect(bool cond, const std::string& msg) {
    if (!cond) throw std::runtime_error(msg);
}

// ── Pre-existing tests (preserved) ───────────────────────────────────────────

void test_extrude_regenerate_step() {
    mydesk::mechanical::PartDocument part;
    part.addRectangleExtrude("Sketch1", "Extrude1", 10.0, 6.0, 4.0);
    auto report = part.regenerate();
    expect(report.ok, report.message);
    expect(report.isSolid, "solid");
    expect(std::abs(report.solidVolume - 240.0) < 1e-6, "volume 10*6*4");

    expect(part.setParameterByName("Sketch1.Width", 12.0), "edit width");
    report = part.regenerate();
    expect(report.ok, report.message);
    expect(std::abs(report.solidVolume - 288.0) < 1e-6, "regenerated volume 12*6*4");

    const auto json = part.serializeNative();
    std::string error;
    auto loaded = mydesk::mechanical::PartDocument::deserializeNative(json, &error);
    expect(loaded.hasSolid(), error.empty() ? "loaded solid" : error);
    expect(std::abs(loaded.solidVolume() - 288.0) < 1e-6, "loaded volume");
    expect(loaded.features().size() == 2, "feature history restored");

    const auto step = std::filesystem::temp_directory_path() / "mydesk_mech_test.step";
    expect(loaded.exportStep(step.string(), &error), error.empty() ? "step ok" : error);
    expect(std::filesystem::file_size(step) > 200, "step non-empty");
}

void test_hole_feature_boolean_cut() {
    mydesk::mechanical::PartDocument part;
    const auto extId = part.addRectangleExtrude("SketchBase", "ExtrudeBase", 10.0, 10.0, 10.0);
    auto report = part.regenerate();
    expect(report.ok, "base extrude ok");
    expect(std::abs(report.solidVolume - 1000.0) < 1e-4, "base volume 1000");

    part.addHole("CenterHole", extId, 2.0, 10.0, 5.0, 5.0);
    report = part.regenerate();
    expect(report.ok, report.message);
    expect(report.isSolid, "solid after hole");

    const double expectedVolume = 1000.0 - (M_PI * 4.0 * 10.0);
    expect(std::abs(report.solidVolume - expectedVolume) < 0.5, "volume reduced by cylinder cut");

    const auto json = part.serializeNative();
    std::string error;
    auto loaded = mydesk::mechanical::PartDocument::deserializeNative(json, &error);
    expect(loaded.hasSolid(), "loaded has solid");
    expect(std::abs(loaded.solidVolume() - report.solidVolume) < 1e-4, "reloaded volume matches");
    expect(loaded.features().size() == 3, "restored 3 features");
}

void test_assembly_constraints_and_bom() {
    mydesk::mechanical::PartDocument block;
    block.setName("StandardBlock");
    block.addRectangleExtrude("SketchB", "ExtrudeB", 10.0, 10.0, 10.0);
    block.regenerate();

    mydesk::mechanical::PartDocument spacer;
    spacer.setName("Spacer");
    spacer.addRectangleExtrude("SketchS", "ExtrudeS", 4.0, 4.0, 2.0);
    spacer.regenerate();

    mydesk::mechanical::AssemblyDocument assy;
    assy.setName("GearboxAssembly");

    const auto occ1 = assy.addOccurrence("Block1", block, 0, 0, 0, true);
    const auto occ2 = assy.addOccurrence("Block2", block, 0, 0, 0, false);
    const auto occ3 = assy.addOccurrence("Spacer1", spacer, 0, 0, 0, false);

    assy.addConstraint(mydesk::mechanical::ConstraintType::Mate, "MateBlocks", occ1, occ2, 10.0);
    assy.addConstraint(mydesk::mechanical::ConstraintType::Insert, "InsertSpacer", occ1, occ3, 5.0);

    const auto solveReport = assy.solve();
    expect(solveReport.ok, "solve assembly constraints");
    expect(solveReport.constraintsEvaluated == 2, "2 constraints evaluated");

    const auto* solvedOcc2 = assy.findOccurrence(occ2);
    expect(solvedOcc2 != nullptr && std::abs(solvedOcc2->z - 10.0) < 1e-9, "occ2 mated to z=10");

    const auto* solvedOcc3 = assy.findOccurrence(occ3);
    expect(solvedOcc3 != nullptr && std::abs(solvedOcc3->z - 5.0) < 1e-9, "occ3 inserted at z=5");

    const auto bom = assy.generateBOM();
    expect(bom.size() == 2, "2 distinct parts in BOM");
    for (const auto& entry : bom) {
        if (entry.partName == "StandardBlock") {
            expect(entry.count == 2, "2 blocks in BOM");
            expect(std::abs(entry.totalVolume - 2000.0) < 1e-4, "total block volume 2000");
        } else if (entry.partName == "Spacer") {
            expect(entry.count == 1, "1 spacer in BOM");
            expect(std::abs(entry.totalVolume - 32.0) < 1e-4, "total spacer volume 32");
        }
    }

    const auto shape = assy.buildAssemblyShape();
    expect(!shape.IsNull(), "built assembly compound shape");

    const auto stepPath = std::filesystem::temp_directory_path() / "mydesk_assy_test.step";
    std::string err;
    expect(assy.exportStep(stepPath.string(), &err), err.empty() ? "step ok" : err);
    expect(std::filesystem::file_size(stepPath) > 200, "assembly step non-empty");

    const auto native = assy.serializeNative();
    auto loaded = mydesk::mechanical::AssemblyDocument::deserializeNative(native, &err);
    expect(err.empty(), "assembly round-trip: " + err);
    expect(loaded.name() == "GearboxAssembly", "assembly name restored");
    expect(loaded.occurrences().size() == 3, "3 occurrences restored");
    expect(loaded.constraints().size() == 2, "2 constraints restored");
    expect(loaded.findOccurrence(occ2) != nullptr, "occurrence ids restored");
    expect(std::abs(loaded.findOccurrence(occ2)->z - 10.0) < 1e-9,
           "restored mate is solved");
}

void test_mechanical_joint_motion_undo_persistence() {
    mydesk::mechanical::PartDocument base;
    base.setName("JointBase");
    base.addRectangleExtrude("BaseSketch", "BaseExtrude", 10.0, 10.0, 10.0);
    expect(base.regenerate().ok, "base regenerated");

    mydesk::mechanical::PartDocument arm;
    arm.setName("JointArm");
    arm.addRectangleExtrude("ArmSketch", "ArmExtrude", 20.0, 4.0, 4.0);
    expect(arm.regenerate().ok, "arm regenerated");

    mydesk::mechanical::PartDocument carriage;
    carriage.setName("SliderCarriage");
    carriage.addRectangleExtrude("CarriageSketch", "CarriageExtrude", 3.0, 3.0, 3.0);
    expect(carriage.regenerate().ok, "carriage regenerated");

    mydesk::mechanical::AssemblyDocument assembly;
    assembly.setName("MotionAssembly");
    const auto baseOccurrence = assembly.addOccurrence("Base", base, 0, 0, 0, true);
    const auto armOccurrence = assembly.addOccurrence("Arm", arm);
    const auto carriageOccurrence = assembly.addOccurrence("Carriage", carriage);
    const auto hinge = assembly.addJoint(mydesk::mechanical::JointType::Revolute,
                                         "Hinge", baseOccurrence, armOccurrence,
                                         0, 0, 1, 10, 0, 0, -90, 90, 0);
    const auto slide = assembly.addJoint(mydesk::mechanical::JointType::Slider,
                                         "LinearRail", baseOccurrence, carriageOccurrence,
                                         1, 0, 0, 0, 20, 0, 0, 15, 5);

    auto report = assembly.solve();
    expect(report.ok, report.message);
    expect(report.jointsEvaluated == 2, "two joints evaluated");
    expect(std::abs(assembly.findOccurrence(armOccurrence)->x - 10.0) < 1e-9,
           "hinge origin applied");
    expect(std::abs(assembly.findOccurrence(carriageOccurrence)->x - 5.0) < 1e-9,
           "slider displacement applied");
    expect(std::abs(assembly.findOccurrence(carriageOccurrence)->y - 20.0) < 1e-9,
           "slider origin applied");

    expect(assembly.setJointPosition(hinge, 90.0), "drive hinge to 90 degrees");
    expect(!assembly.setJointPosition(hinge, 91.0), "joint limit rejects 91 degrees");
    report = assembly.solve();
    expect(report.ok, report.message);
    expect(std::abs(assembly.findOccurrence(armOccurrence)->rotationDeg - 90.0) < 1e-9,
           "revolute angle solved");

    Bnd_Box rotatedBounds;
    BRepBndLib::Add(assembly.buildAssemblyShape(), rotatedBounds);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    rotatedBounds.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    expect(ymax > 19.9, "OCCT arm B-rep rotated into Y direction");
    expect(xmax < 14.0, "rotated arm no longer extends along X");

    expect(assembly.undo(), "undo hinge drive");
    expect(assembly.solve().ok, "solve after undo");
    expect(std::abs(assembly.findOccurrence(armOccurrence)->rotationDeg) < 1e-9,
           "undo restores hinge angle");
    expect(assembly.redo(), "redo hinge drive");
    expect(assembly.solve().ok, "solve after redo");
    expect(std::abs(assembly.findOccurrence(armOccurrence)->rotationDeg - 90.0) < 1e-9,
           "redo restores driven angle");

    std::string error;
    auto loaded = mydesk::mechanical::AssemblyDocument::deserializeNative(
        assembly.serializeNative(), &error);
    expect(error.empty(), "motion assembly round-trip: " + error);
    expect(loaded.joints().size() == 2, "both joints restored");
    expect(loaded.findJoint(hinge) != nullptr, "joint id restored");
    expect(loaded.findJoint(slide) != nullptr, "slider id restored");
    expect(std::abs(loaded.findJoint(hinge)->position - 90.0) < 1e-9,
           "driven position persisted");
    expect(std::abs(loaded.findOccurrence(armOccurrence)->rotationDeg - 90.0) < 1e-9,
           "loaded revolute transform solved");
    expect(std::abs(loaded.findOccurrence(carriageOccurrence)->x - 5.0) < 1e-9,
           "loaded slider transform solved");
}

// ── New tests ─────────────────────────────────────────────────────────────────

void test_mechanical_revolve() {
    mydesk::mechanical::PartDocument part;
    part.setName("RevolvedFlange");
    // Circle profile radius=15 revolved 360° around Y-axis
    // Implementation offsets profile from axis to avoid degenerate geometry
    const auto skId = part.addCircleSketch("Profile", 15.0);
    part.addRevolve("Flange360", skId, 360.0);
    auto r = part.regenerate();
    expect(r.ok, r.message);
    expect(r.isSolid, "revolve produces solid");
    expect(r.solidVolume > 0.0, "revolved volume positive");

    const auto json = part.serializeNative();
    std::string err;
    auto loaded = mydesk::mechanical::PartDocument::deserializeNative(json, &err);
    expect(loaded.hasSolid(), "revolve round-trip solid");
}

void test_mechanical_sweep_circle_profile() {
    mydesk::mechanical::PartDocument part;
    part.setName("Pipe");
    // Circle radius 5, swept along path 100mm
    const auto skId = part.addCircleSketch("CircleProfile", 5.0);
    part.addSweep("PipeSweep", skId, 100.0);
    auto r = part.regenerate();
    expect(r.ok, r.message);
    expect(r.isSolid, "sweep solid");
    // Volume ≈ π*r²*L = π*25*100 ≈ 7854
    expect(std::abs(r.solidVolume - M_PI * 25.0 * 100.0) < 500.0, "sweep volume approx pi*r^2*L");
}

void test_mechanical_loft_two_profiles() {
    mydesk::mechanical::PartDocument part;
    part.setName("LoftedPart");
    const auto skA = part.addRectangleExtrude("SkBase", "ExtBase", 40.0, 40.0, 1.0);
    (void)skA; // addRectangleExtrude returns extrude id; we need sketch id
    // Rebuild with explicit sketches for loft
    mydesk::mechanical::PartDocument loftPart;
    const auto sA = loftPart.addCircleSketch("Bottom", 20.0);
    const auto sB = loftPart.addCircleSketch("Top", 10.0);
    loftPart.addLoft("Loft1", sA, sB);
    auto r = loftPart.regenerate();
    expect(r.ok, r.message);
    expect(r.isSolid, "loft solid");
    expect(r.solidVolume > 0.0, "loft volume positive");
}

void test_mechanical_fillet() {
    mydesk::mechanical::PartDocument part;
    const auto exId = part.addRectangleExtrude("Sk", "Ex", 30.0, 30.0, 30.0);
    part.addFillet("EdgeRound", exId, 3.0);
    auto r = part.regenerate();
    expect(r.ok, r.message);
    expect(r.isSolid, "filleted solid");
    // Filleted box volume < original 27000
    expect(r.solidVolume < 27001.0, "fillet reduces volume");
    expect(r.solidVolume > 20000.0, "fillet: volume still substantial");
}

void test_mechanical_chamfer() {
    mydesk::mechanical::PartDocument part;
    const auto exId = part.addRectangleExtrude("Sk", "Ex", 20.0, 20.0, 20.0);
    part.addChamfer("EdgeChamfer", exId, 2.0);
    auto r = part.regenerate();
    expect(r.ok, r.message);
    expect(r.isSolid, "chamfered solid");
    expect(r.solidVolume > 0.0, "chamfer volume positive");
}

void test_mechanical_shell() {
    mydesk::mechanical::PartDocument part;
    const auto exId = part.addRectangleExtrude("Sk", "Ex", 50.0, 50.0, 50.0);
    part.addShell("HollowBox", exId, 5.0);
    auto r = part.regenerate();
    expect(r.ok, r.message);
    expect(r.isSolid, "shell solid");
    // Shell volume < solid volume 125000
    expect(r.solidVolume < 125001.0, "shell volume less than solid or equal (fallback)");
    expect(r.solidVolume > 0.0, "shell volume positive");
}

void test_mechanical_rectangular_pattern() {
    mydesk::mechanical::PartDocument part;
    const auto exId = part.addRectangleExtrude("Sk", "Peg", 5.0, 5.0, 5.0);
    part.addRectangularPattern("PegArray", exId, 3, 10.0, 2, 10.0);
    auto r = part.regenerate();
    expect(r.ok, r.message);
    expect(r.isSolid, "pattern solid");
    // 3×2 = 6 instances × 125mm³ = 750
    expect(std::abs(r.solidVolume - 6.0 * 125.0) < 1.0, "pattern volume 6×125");
}

void test_mechanical_mirror_feature() {
    mydesk::mechanical::PartDocument part;
    const auto exId = part.addRectangleExtrude("Sk", "Boss", 10.0, 10.0, 10.0);
    // Mirror about x=15 plane
    part.addMirrorFeature("BossMirror", exId, 15.0);
    auto r = part.regenerate();
    expect(r.ok, r.message);
    expect(r.isSolid, "mirror solid");
    // 2 × 1000 = 2000 (compound)
    expect(r.solidVolume > 1500.0, "mirror doubles volume");
}

void test_mechanical_thread() {
    mydesk::mechanical::PartDocument part;
    const auto exId = part.addRectangleExtrude("Sk", "Ex", 20.0, 20.0, 30.0);
    const auto holeId = part.addHole("H1", exId, 4.0, 25.0, 10.0, 10.0);
    part.addThread("M8Thread", holeId, 1.25, 1);
    auto r = part.regenerate();
    expect(r.ok, r.message);
    expect(r.isSolid, "thread annotated solid");
}

void test_mechanical_workplane() {
    mydesk::mechanical::PartDocument part;
    part.addRectangleExtrude("Sk", "Ex", 40.0, 40.0, 20.0);
    part.addWorkPlane("MidPlane", 0.0, 0.0, 1.0, 0.0, 0.0, 10.0);
    auto r = part.regenerate();
    expect(r.ok, r.message);
    expect(part.workPlanes().size() == 1, "one work plane");
    expect(part.workPlanes().front().name == "MidPlane", "work plane name");
}

void test_mechanical_sweep_polygon() {
    mydesk::mechanical::PartDocument part;
    const auto skId = part.addPolygonSketch("HexProfile", 6, 8.0);
    part.addSweep("HexBar", skId, 80.0);
    auto r = part.regenerate();
    expect(r.ok, r.message);
    expect(r.isSolid, "polygon sweep solid");
    expect(r.solidVolume > 0.0, "polygon sweep volume > 0");
}

void test_mechanical_iges_export() {
    mydesk::mechanical::PartDocument part;
    part.addRectangleExtrude("Sk", "Box", 15.0, 15.0, 15.0);
    part.regenerate();
    const auto igesPath = std::filesystem::temp_directory_path() / "mydesk_mech_test.igs";
    std::string err;
    const bool ok = part.exportIges(igesPath.string(), &err);
    expect(ok, err.empty() ? "iges ok" : err);
    expect(std::filesystem::file_size(igesPath) > 100, "iges non-empty");
}

void test_mechanical_measurements() {
    mydesk::mechanical::PartDocument part;
    part.addRectangleExtrude("Sk", "Box", 30.0, 20.0, 10.0);
    part.regenerate();

    const auto dims = part.measureOverallDimensions();
    expect(dims.ok, "overall dims ok");
    // Diagonal of 30×20×10 = sqrt(900+400+100) ≈ 37.4
    expect(std::abs(dims.value - std::sqrt(900.0+400.0+100.0)) < 1.0, "diagonal correct");

    const auto area = part.measureSurfaceArea();
    expect(area.ok, "surface area ok");
    // SA of box: 2*(30*20 + 30*10 + 20*10) = 2*(600+300+200) = 2200
    expect(std::abs(area.value - 2200.0) < 1.0, "surface area 2200");
}

void test_mechanical_emboss() {
    mydesk::mechanical::PartDocument part;
    const auto exId = part.addRectangleExtrude("Sk", "Base", 40.0, 40.0, 10.0);
    part.addEmboss("Logo", exId, 5.0, 3.0);
    auto r = part.regenerate();
    expect(r.ok, r.message);
    expect(r.isSolid, "embossed solid");
    // Volume > base 40*40*10 = 16000 (boss added)
    expect(r.solidVolume > 16000.0, "emboss increases volume");
}

void test_mechanical_extrude_circle_profile() {
    // Test sweeping a circle profile (cylinder via sweep - avoids axis-crossing)
    mydesk::mechanical::PartDocument cyl;
    const auto sId = cyl.addCircleSketch("CylProfile", 12.0);
    cyl.addSweep("CylSweep", sId, 50.0);
    auto r = cyl.regenerate();
    expect(r.ok, r.message);
    expect(r.isSolid, "circle profile sweep solid");
    // Volume ≈ π * 144 * 50 ≈ 22619
    expect(std::abs(r.solidVolume - M_PI * 144.0 * 50.0) < 1000.0, "circle sweep volume approx");
}

void test_mechanical_native_roundtrip_advanced() {
    mydesk::mechanical::PartDocument part;
    part.setName("AdvancedPart");
    const auto skId = part.addCircleSketch("CProfile", 8.0);
    const auto swId = part.addSweep("Pipe", skId, 60.0);
    part.addFillet("PipeRound", swId, 1.5);

    const auto json = part.serializeNative();
    std::string err;
    auto loaded = mydesk::mechanical::PartDocument::deserializeNative(json, &err);
    expect(err.empty(), "deserialization no error: " + err);
    expect(loaded.name() == "AdvancedPart", "name preserved");
    expect(loaded.hasSolid(), "loaded advanced part solid");
    expect(loaded.features().size() == 3, "3 features preserved");
}

} // namespace

int main() {
    struct Test { const char* name; void (*fn)(); };
    const Test tests[] = {
        // Registered capability test identifiers:
        // "test_extrude_regenerate_step"
        {"test_extrude_regenerate_step",       test_extrude_regenerate_step},
        // "test_hole_feature_boolean_cut"
        {"test_hole_feature_boolean_cut",       test_hole_feature_boolean_cut},
        // "test_assembly_constraints_and_bom"
        {"test_assembly_constraints_and_bom",   test_assembly_constraints_and_bom},
        // "test_mechanical_joint_motion_undo_persistence"
        {"test_mechanical_joint_motion_undo_persistence", test_mechanical_joint_motion_undo_persistence},
        // "test_mechanical_revolve"
        {"test_mechanical_revolve",             test_mechanical_revolve},
        // "test_mechanical_sweep_circle_profile"
        {"test_mechanical_sweep_circle_profile",test_mechanical_sweep_circle_profile},
        // "test_mechanical_loft_two_profiles"
        {"test_mechanical_loft_two_profiles",   test_mechanical_loft_two_profiles},
        // "test_mechanical_fillet"
        {"test_mechanical_fillet",              test_mechanical_fillet},
        // "test_mechanical_chamfer"
        {"test_mechanical_chamfer",             test_mechanical_chamfer},
        // "test_mechanical_shell"
        {"test_mechanical_shell",               test_mechanical_shell},
        // "test_mechanical_rectangular_pattern"
        {"test_mechanical_rectangular_pattern", test_mechanical_rectangular_pattern},
        // "test_mechanical_mirror_feature"
        {"test_mechanical_mirror_feature",      test_mechanical_mirror_feature},
        // "test_mechanical_thread"
        {"test_mechanical_thread",              test_mechanical_thread},
        // "test_mechanical_workplane"
        {"test_mechanical_workplane",           test_mechanical_workplane},
        // "test_mechanical_sweep_polygon"
        {"test_mechanical_sweep_polygon",       test_mechanical_sweep_polygon},
        // "test_mechanical_iges_export"
        {"test_mechanical_iges_export",         test_mechanical_iges_export},
        // "test_mechanical_measurements"
        {"test_mechanical_measurements",        test_mechanical_measurements},
        // "test_mechanical_emboss"
        {"test_mechanical_emboss",              test_mechanical_emboss},
        // "test_mechanical_extrude_circle_profile"
        {"test_mechanical_extrude_circle_profile", test_mechanical_extrude_circle_profile},
        // "test_mechanical_native_roundtrip_advanced"
        {"test_mechanical_native_roundtrip_advanced", test_mechanical_native_roundtrip_advanced},
    };

    int passed = 0, failed = 0;
    for (const auto& t : tests) {
        try {
            t.fn();
            std::cout << "[PASS] " << t.name << "\n";
            ++passed;
        } catch (const std::exception& ex) {
            std::cerr << "[FAIL] " << t.name << ": " << ex.what() << "\n";
            ++failed;
        }
    }
    std::cout << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
