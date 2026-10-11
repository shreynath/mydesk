#include "mydesk/coordinate/coordinate_document.hpp"

#include <iostream>

int main(int argc, char** argv) {
    std::cout << "MyDesk Coordinate v0.2.0 — Navisworks Class Professional Model Coordination\n";
    mydesk::coordinate::CoordinateDocument doc;
    doc.setName("HospitalCoordination");

    const auto m1 = doc.addModel("ArchModel", "arch.ifc", "Architectural");
    const auto m2 = doc.addModel("MEPModel", "mep.ifc", "MEP");

    doc.addElementToModel(m1, "Wall_101", "Wall", {0.0, 0.0, 0.0, 5.0, 0.3, 3.0});
    doc.addElementToModel(m2, "SupplyDuct_01", "HVAC_Duct", {2.0, -0.2, 2.0, 2.5, 0.5, 2.5});

    const auto testId = doc.createClashTest("Arch_vs_MEP_HardClash",
                                            mydesk::coordinate::ClashType::Hard, 0.005);
    doc.setClashTestSets(testId, {m1}, {m2});
    const auto clashCount = doc.runClashTest(testId);

    std::cout << "Clash test executed: found " << clashCount << " clash(es).\n";
    if (const auto* test = doc.findClashTest(testId)) {
        std::cout << "Broad phase: " << test->lastRun.broadPhaseCandidates << " / "
                  << test->lastRun.rawPairCount << " candidate pair(s); narrow phase: "
                  << test->lastRun.narrowPhaseChecks << " check(s).\n";
    }

    (void)argc;
    (void)argv;
    return 0;
}
