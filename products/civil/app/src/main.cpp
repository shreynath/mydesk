#include "mydesk/civil/civil_document.hpp"

#include <iostream>

int main(int argc, char** argv) {
    std::cout << "MyDesk Civil v0.1.0 — Civil 3D Class Professional Engineering\n";
    mydesk::civil::CivilDocument doc;
    doc.setName("ProjectCivil");
    const auto p1 = doc.addPoint("100", 1000.0, 2000.0, 105.5, "BENCHMARK");
    const auto p2 = doc.addPoint("101", 1100.0, 2000.0, 107.2, "GROUND");
    const auto p3 = doc.addPoint("102", 1050.0, 2080.0, 109.0, "GROUND");
    const auto s1 = doc.buildTinSurface("EG", {p1, p2, p3});

    const auto* surf = doc.findSurface(s1);
    if (surf) {
        std::cout << "Built TIN surface '" << surf->name << "' with " << surf->triangles.size() << " triangles.\n";
    }

    const auto al1 = doc.createAlignment("Centerline", 0.0);
    doc.addAlignmentTangent(al1, 1000.0, 2000.0, 1500.0, 2000.0);
    const auto* al = doc.findAlignment(al1);
    if (al) {
        std::cout << "Alignment '" << al->name << "' total length: " << al->totalLength()
                  << " m (" << al->formatStation(al->totalLength()) << ")\n";
    }

    (void)argc;
    (void)argv;
    return 0;
}
