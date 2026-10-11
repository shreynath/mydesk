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

void test_oriented_box_narrow_phase_and_broad_phase() {
    using namespace mydesk::coordinate;
    CoordinateDocument doc;
    const auto modelA = doc.addModel("Fabrication A", "a.nwc", "MEP");
    const auto modelB = doc.addModel("Fabrication B", "b.nwc", "MEP");
    constexpr double pi = 3.14159265358979323846;

    // These parallel trays have heavily overlapping AABBs, but their actual rotated
    // solids are separated by 0.30 m perpendicular to their long axes.
    const auto trayA = OrientedBox3D::fromEuler({0.0, 0.0, 0.0}, {2.0, 0.1, 0.1}, pi / 4.0);
    const auto trayB = OrientedBox3D::fromEuler({-0.3535533905932738, 0.3535533905932738, 0.0},
                                                {2.0, 0.1, 0.1}, pi / 4.0);
    expect(trayA.axisAlignedBounds().intersects(trayB.axisAlignedBounds()),
           "rotated tray AABBs overlap for broad phase");
    doc.addOrientedElementToModel(modelA, "Tray A", "CableTray", trayA);
    doc.addOrientedElementToModel(modelB, "Tray B", "CableTray", trayB);

    // Populate distant elements so the broad-phase reduction is observable.
    for (int i = 0; i < 20; ++i) {
        const double x = 100.0 + i * 10.0;
        doc.addElementToModel(modelA, "Remote " + std::to_string(i), "Equipment",
                              {x, x, x, x + 1.0, x + 1.0, x + 1.0});
    }

    const auto hard = doc.createClashTest("Rotated hard", ClashType::Hard, 0.001);
    doc.setClashTestSets(hard, {modelA}, {modelB});
    expect(doc.runClashTest(hard) == 0,
           "OBB narrow phase rejects overlapping-AABB false positive");
    const auto* hardTest = doc.findClashTest(hard);
    expect(hardTest != nullptr, "hard OBB test exists");
    expect(hardTest->lastRun.rawPairCount == 21, "raw cartesian pair count recorded");
    expect(hardTest->lastRun.broadPhaseCandidates == 1,
           "sweep-and-prune reduces 21 pairs to one narrow-phase candidate");
    expect(hardTest->lastRun.narrowPhaseChecks == 1, "one SAT narrow-phase check");

    const auto clearance = doc.createClashTest("Rotated clearance", ClashType::Clearance,
                                               0.001, 0.31);
    doc.setClashTestSets(clearance, {modelA}, {modelB});
    expect(doc.runClashTest(clearance) == 1, "exact rotated-box clearance violation found");
    const auto* clearanceTest = doc.findClashTest(clearance);
    expect(clearanceTest != nullptr && clearanceTest->results.size() == 1,
           "one rotated clearance result stored");
    expect(std::abs(clearanceTest->results.front().penetrationOrDistance - 0.30) < 1e-8,
           "feature-level OBB distance is 0.30 m");

    CoordinateDocument intersecting;
    const auto clashA = intersecting.addModel("Clash A", "clash-a.nwc", "MEP");
    const auto clashB = intersecting.addModel("Clash B", "clash-b.nwc", "MEP");
    intersecting.addOrientedElementToModel(
        clashA, "Duct A", "Duct",
        OrientedBox3D::fromEuler({0.0, 0.0, 0.0}, {2.0, 0.1, 0.1}, pi / 4.0));
    intersecting.addOrientedElementToModel(
        clashB, "Duct B", "Duct",
        OrientedBox3D::fromEuler({-0.1060660171779821, 0.1060660171779821, 0.0},
                                 {2.0, 0.1, 0.1}, pi / 4.0));
    const auto orientedHard = intersecting.createClashTest("OBB penetration", ClashType::Hard,
                                                            0.001);
    intersecting.setClashTestSets(orientedHard, {clashA}, {clashB});
    expect(intersecting.runClashTest(orientedHard) == 1,
           "SAT accepts genuinely intersecting rotated solids");
    expect(std::abs(intersecting.findClashTest(orientedHard)->results.front()
                        .penetrationOrDistance -
                    0.05) < 1e-8,
           "SAT reports 0.05 m minimum translation penetration");
}

void test_duplicate_suppression_review_undo_and_rerun() {
    using namespace mydesk::coordinate;
    CoordinateDocument doc;
    const auto first = doc.addModel("First", "first.nwc", "Architecture");
    const auto second = doc.addModel("Second", "second.nwc", "Structure");
    doc.addElementToModel(first, "Wall", "Wall", {0, 0, 0, 2, 1, 2});
    doc.addElementToModel(second, "Beam", "Beam", {1, 0, 1, 3, 1, 2});
    const auto testId = doc.createClashTest("Symmetric selections", ClashType::Hard, 0.01);
    doc.setClashTestSets(testId, {first, second}, {second, first});
    expect(doc.runClashTest(testId) == 1, "overlapping selection sets emit one unordered pair");

    const auto resultId = doc.findClashTest(testId)->results.front().id;
    expect(doc.setClashResultReview(testId, resultId, ClashStatus::Approved,
                                    "Sleeve approved by coordination lead"),
           "review update succeeds");
    expect(doc.findClashTest(testId)->results.front().status == ClashStatus::Approved,
           "approved state applied");
    expect(doc.undo(), "review edit undo succeeds");
    expect(doc.findClashTest(testId)->results.front().status == ClashStatus::New,
           "undo restores New state");
    expect(doc.redo(), "review edit redo succeeds");
    expect(doc.findClashTest(testId)->results.front().status == ClashStatus::Approved,
           "redo restores approval");

    expect(doc.runClashTest(testId) == 1, "rerun still has one result");
    const auto& rerun = doc.findClashTest(testId)->results.front();
    expect(rerun.id == resultId, "rerun preserves stable result identity");
    expect(rerun.status == ClashStatus::Approved,
           "rerun preserves professional review disposition");
    expect(rerun.comments == "Sleeve approved by coordination lead",
           "rerun preserves reviewer comments");
}

void test_bcf_export_and_session_persistence() {
    mydesk::coordinate::CoordinateDocument doc;
    doc.setName("SiteCoordination");
    const auto m1 = doc.addModel("BuildingA", "bld.ifc", "General");
    const auto m2 = doc.addModel("Utilities", "util.ifc", "Civil");

    doc.addElementToModel(m1, "Footing_F1", "Footing", {0.0, 0.0, 0.0, 2.0, 2.0, 1.0});
    constexpr double pi = 3.14159265358979323846;
    doc.addOrientedElementToModel(
        m2, "WaterMain", "Pipe",
        mydesk::coordinate::OrientedBox3D::fromEuler({2.0, 1.1, 0.6}, {1.0, 0.1, 0.1},
                                                      pi / 12.0));

    const auto tId = doc.createClashTest("Footing_vs_Water", mydesk::coordinate::ClashType::Hard, 0.01);
    doc.setClashTestSets(tId, {m1}, {m2});
    expect(doc.runClashTest(tId) == 1, "persistence fixture produces one clash");
    const auto resultId = doc.findClashTest(tId)->results.front().id;
    expect(doc.setClashResultReview(tId, resultId, mydesk::coordinate::ClashStatus::Reviewed,
                                    "Needs R&D <review>"),
           "review state prepared for persistence");

    // BCF XML output
    const auto bcf = doc.exportBcfXml();
    expect(bcf.find("<VisualizationInfo") != std::string::npos, "BCF VisualizationInfo root");
    expect(bcf.find("<Component IfcGuid=\"EL_") != std::string::npos, "Component IFC GUID present");
    expect(bcf.find("<Origin") != std::string::npos, "Origin coordinates present");
    expect(bcf.find("Needs R&amp;D &lt;review&gt;") != std::string::npos,
           "BCF comment is XML escaped");

    // Native session persistence
    const auto json = doc.serializeNative();
    std::string err;
    auto loaded = mydesk::coordinate::CoordinateDocument::deserializeNative(json, &err);
    expect(err.empty(), "roundtrip reports no parse error");
    expect(loaded.models().size() == 2, "restored 2 models");
    expect(loaded.models().front().name == "BuildingA", "model name restored");
    expect(loaded.models()[1].sourcePath == "util.ifc", "model source path restored");
    expect(loaded.models()[1].elements.size() == 1, "element restored");
    expect(loaded.models()[1].elements.front().orientedBounds.has_value(),
           "oriented narrow-phase geometry restored");
    const auto* loadedTest = loaded.findClashTest(tId);
    expect(loadedTest != nullptr, "clash test definition restored");
    expect(loadedTest->selectionSetA == std::vector<mydesk::coordinate::ModelId>{m1},
           "selection set A restored");
    expect(loadedTest->selectionSetB == std::vector<mydesk::coordinate::ModelId>{m2},
           "selection set B restored");
    expect(loadedTest->results.size() == 1, "clash result restored");
    expect(loadedTest->results.front().status == mydesk::coordinate::ClashStatus::Reviewed,
           "review disposition restored");
    expect(loadedTest->results.front().comments == "Needs R&D <review>",
           "review comment restored");
    expect(loaded.runClashTest(tId) == 1, "restored oriented test remains executable");
    expect(loaded.findClashTest(tId)->results.front().status ==
               mydesk::coordinate::ClashStatus::Reviewed,
           "restored review disposition survives rerun");
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

        const char* kRegistered3 = "test_oriented_box_narrow_phase_and_broad_phase";
        (void)kRegistered3;
        test_oriented_box_narrow_phase_and_broad_phase();
        std::cout << "[PASS] test_oriented_box_narrow_phase_and_broad_phase\n";

        const char* kRegistered4 = "test_duplicate_suppression_review_undo_and_rerun";
        (void)kRegistered4;
        test_duplicate_suppression_review_undo_and_rerun();
        std::cout << "[PASS] test_duplicate_suppression_review_undo_and_rerun\n";

        const char* kRegistered5 = "test_bcf_export_and_session_persistence";
        (void)kRegistered5;
        test_bcf_export_and_session_persistence();
        std::cout << "[PASS] test_bcf_export_and_session_persistence\n";

        std::cout << "5 passed, 0 failed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& ex) {
        std::cout << "[FAIL] " << ex.what() << "\n0 passed, 1 failed\n";
        return EXIT_FAILURE;
    }
}
