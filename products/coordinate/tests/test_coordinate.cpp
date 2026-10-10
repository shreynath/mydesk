#include "mydesk/coordinate/coordinate_document.hpp"

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

void test_hard_clash_detection_and_penetration() {
    mydesk::coordinate::CoordinateDocument doc;
    const auto mStruct = doc.addModel("Structural", "struct.ifc", "Structural");
    const auto mMep = doc.addModel("MEP", "mep.ifc", "MEP");

    // Structural column: 0..0.5 in X, 0..0.5 in Y, 0..4 in Z
    const auto colId = doc.addElementToModel(mStruct, "Col_C1", "Column",
                                            {0.0, 0.0, 0.0, 0.5, 0.5, 4.0});

    // MEP pipe penetrating column: 0.2..0.4 in X (overlap 0.2), -1.0..1.0 in Y (overlap 0.5), 2.0..2.2 in Z (overlap 0.2)
    const auto pipeId = doc.addElementToModel(mMep, "Pipe_P10", "Pipe",
                                             {0.2, -1.0, 2.0, 0.4, 1.0, 2.2});

    const auto testId = doc.createClashTest("Struct_vs_MEP_Hard",
                                            mydesk::coordinate::ClashType::Hard, 0.01);
    doc.setClashTestSets(testId, {mStruct}, {mMep});

    const auto numClashes = doc.runClashTest(testId);
    expect(numClashes == 1, "exactly 1 hard clash detected");

    const auto* test = doc.findClashTest(testId);
    expect(test != nullptr, "test found");
    expect(test->results.size() == 1, "1 result stored");
    const auto& res = test->results.front();
    expect(res.elementA == colId, "element A is column");
    expect(res.elementB == pipeId, "element B is pipe");
    expect(res.type == mydesk::coordinate::ClashType::Hard, "is hard clash");
    // Penetration depth in X is (0.4 - 0.2) = 0.2, in Z is (2.2 - 2.0) = 0.2 -> min overlap = 0.2
    expect(std::abs(res.penetrationOrDistance - 0.2) < 1e-4, "penetration depth 0.2 m");
    expect(res.status == mydesk::coordinate::ClashStatus::New, "status is New");
}

void test_clearance_clash_detection() {
    mydesk::coordinate::CoordinateDocument doc;
    const auto m1 = doc.addModel("Arch", "arch.ifc", "Architectural");
    const auto m2 = doc.addModel("Elec", "elec.ifc", "Electrical");

    // Wall: 0..1 in X, 0..1 in Y, 0..3 in Z
    doc.addElementToModel(m1, "Drywall_W1", "Wall", {0.0, 0.0, 0.0, 1.0, 1.0, 3.0});

    // Electrical tray 20mm away from wall (no physical overlap, but within 50mm clearance)
    doc.addElementToModel(m2, "CableTray_E1", "CableTray", {1.02, 0.0, 2.0, 1.5, 0.5, 2.2});

    // Run hard clash: should find 0 clashes
    const auto hardTest = doc.createClashTest("HardCheck", mydesk::coordinate::ClashType::Hard, 0.005);
    doc.setClashTestSets(hardTest, {m1}, {m2});
    expect(doc.runClashTest(hardTest) == 0, "no hard clash");

    // Run clearance test with 0.05m (50mm) buffer: should find 1 clearance clash
    const auto clearTest = doc.createClashTest("ClearanceCheck",
                                              mydesk::coordinate::ClashType::Clearance, 0.001, 0.05);
    doc.setClashTestSets(clearTest, {m1}, {m2});
    const auto clearCount = doc.runClashTest(clearTest);
    expect(clearCount == 1, "1 clearance violation detected");

    const auto* test = doc.findClashTest(clearTest);
    expect(test != nullptr && !test->results.empty(), "clearance results present");
    expect(std::abs(test->results.front().penetrationOrDistance - 0.02) < 1e-4, "clearance distance 0.02m");
}

void test_bcf_export_and_session_persistence() {
    mydesk::coordinate::CoordinateDocument doc;
    doc.setName("SiteCoordination");
    const auto m1 = doc.addModel("BuildingA", "bld.ifc", "General");
    const auto m2 = doc.addModel("Utilities", "util.ifc", "Civil");

    doc.addElementToModel(m1, "Footing_F1", "Footing", {0.0, 0.0, 0.0, 2.0, 2.0, 1.0});
    doc.addElementToModel(m2, "WaterMain", "Pipe", {1.0, 1.0, 0.5, 3.0, 1.2, 0.7});

    const auto tId = doc.createClashTest("Footing_vs_Water", mydesk::coordinate::ClashType::Hard, 0.01);
    doc.setClashTestSets(tId, {m1}, {m2});
    doc.runClashTest(tId);

    // BCF XML output
    const auto bcf = doc.exportBcfXml();
    expect(bcf.find("<VisualizationInfo") != std::string::npos, "BCF VisualizationInfo root");
    expect(bcf.find("<Component IfcGuid=\"EL_") != std::string::npos, "Component IFC GUID present");
    expect(bcf.find("<Origin") != std::string::npos, "Origin coordinates present");

    // Native session persistence
    const auto json = doc.serializeNative();
    std::string err;
    auto loaded = mydesk::coordinate::CoordinateDocument::deserializeNative(json, &err);
    expect(loaded.models().size() == 2, "restored 2 models");
    expect(loaded.models().front().name == "BuildingA", "model name restored");
}

} // namespace

int main() {
    try {
        const char* kRegistered1 = "test_hard_clash_detection_and_penetration";
        (void)kRegistered1;
        test_hard_clash_detection_and_penetration();
        std::cout << "[PASS] test_hard_clash_detection_and_penetration\n";

        const char* kRegistered2 = "test_clearance_clash_detection";
        (void)kRegistered2;
        test_clearance_clash_detection();
        std::cout << "[PASS] test_clearance_clash_detection\n";

        const char* kRegistered3 = "test_bcf_export_and_session_persistence";
        (void)kRegistered3;
        test_bcf_export_and_session_persistence();
        std::cout << "[PASS] test_bcf_export_and_session_persistence\n";

        std::cout << "3 passed, 0 failed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cout << "[FAIL] " << ex.what() << "\n0 passed, 1 failed\n";
        return EXIT_FAILURE;
    }
}
