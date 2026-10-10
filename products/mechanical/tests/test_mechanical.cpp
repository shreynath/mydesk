#include "mydesk/mechanical/part_document.hpp"

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

} // namespace

int main() {
    try {
        const char* kRegistered = "test_extrude_regenerate_step";
        (void)kRegistered;
        test_extrude_regenerate_step();
        std::cout << "[PASS] test_extrude_regenerate_step\n1 passed, 0 failed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cout << "[FAIL] " << ex.what() << "\n0 passed, 1 failed\n";
        return EXIT_FAILURE;
    }
}
