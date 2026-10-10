#include "mydesk/mechanical/part_document.hpp"
#include "mydesk/mechanical/assembly_document.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {

void expect(bool cond, const std::string& msg) {
    if (!cond) {
        throw std::runtime_error(msg);
    }
}

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

    // Add hole through center (radius 2, depth 10, x=5, y=5)
    part.addHole("CenterHole", extId, 2.0, 10.0, 5.0, 5.0);
    report = part.regenerate();
    expect(report.ok, report.message);
    expect(report.isSolid, "solid after hole");

    // Cylindrical cut volume: pi * r^2 * h = pi * 4 * 10 ~= 125.6637
    const double expectedVolume = 1000.0 - (M_PI * 4.0 * 10.0);
    expect(std::abs(report.solidVolume - expectedVolume) < 0.5, "volume reduced by cylinder cut");

    const auto json = part.serializeNative();
    std::string error;
    auto loaded = mydesk::mechanical::PartDocument::deserializeNative(json, &error);
    expect(loaded.hasSolid(), "loaded has solid");
    expect(std::abs(loaded.solidVolume() - report.solidVolume) < 1e-4, "reloaded volume matches");
    expect(loaded.features().size() == 3, "restored 3 features (sketch, extrude, hole)");
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
}

} // namespace

int main() {
    try {
        const char* kRegistered1 = "test_extrude_regenerate_step";
        (void)kRegistered1;
        test_extrude_regenerate_step();
        std::cout << "[PASS] test_extrude_regenerate_step\n";

        const char* kRegistered2 = "test_hole_feature_boolean_cut";
        (void)kRegistered2;
        test_hole_feature_boolean_cut();
        std::cout << "[PASS] test_hole_feature_boolean_cut\n";

        const char* kRegistered3 = "test_assembly_constraints_and_bom";
        (void)kRegistered3;
        test_assembly_constraints_and_bom();
        std::cout << "[PASS] test_assembly_constraints_and_bom\n";

        std::cout << "3 passed, 0 failed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cout << "[FAIL] " << ex.what() << "\n0 passed, 1 failed\n";
        return EXIT_FAILURE;
    }
}
