// SPDX-License-Identifier: MPL-2.0
// MyDesk Infra — Conceptual Infrastructure Standalone Application
#include "mydesk/infra/infra_document.hpp"

#include <iostream>

int main(int argc, char** argv) {
    std::cout << "MyDesk Infra v0.1.0 — Autodesk InfraWorks Class Conceptual Infrastructure\n";
    mydesk::infra::InfraDocument doc;
    doc.setName("HighlandHighwayProject");
    doc.setCrs("EPSG:32618"); // UTM Zone 18N

    // Setup 4x4 DEM terrain grid (10m cell size)
    std::vector<double> demData = {
        100.0, 102.0, 105.0, 110.0,
        101.0, 104.0, 108.0, 112.0,
        103.0, 106.0, 111.0, 115.0,
        105.0, 108.0, 113.0, 118.0
    };
    doc.setDemGrid(500000.0, 4500000.0, 10.0, 4, 4, std::move(demData));

    // Add component road
    const auto roadId = doc.addRoad("Route_9", 4, 3.75, 100.0, {
        {500005.0, 4500005.0, 101.5},
        {500015.0, 4500015.0, 106.0},
        {500025.0, 4500025.0, 111.0}
    });

    const auto* road = doc.findRoad(roadId);
    if (road) {
        std::cout << "Created Road '" << road->name << "' with length " << road->totalLengthMeters() << " m\n";
    }

    doc.addBridge("RiverBridge_01", roadId, 10.0, 25.0, 3);
    doc.addBuilding("AdminComplex", "Civic", 22.0, {
        {500002.0, 4500002.0},
        {500008.0, 4500002.0},
        {500008.0, 4500008.0},
        {500002.0, 4500008.0}
    });

    std::cout << "Bridges: " << doc.bridges().size() << " Buildings: " << doc.buildings().size() << "\n";

    (void)argc;
    (void)argv;
    return 0;
}
