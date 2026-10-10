#include "mydesk/mechanical/part_document.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    mydesk::mechanical::PartDocument part;
    part.setName("Bracket");
    part.addRectangleExtrude("BaseSketch", "BaseExtrude", 40.0, 20.0, 8.0);
    const auto report = part.regenerate();
    if (!report.ok) {
        std::cerr << "Regeneration failed: " << report.message << '\n';
        return 1;
    }

    std::cout << "MyDesk Mechanical\n";
    std::cout << "Part: " << part.name() << '\n';
    std::cout << "Solid volume: " << report.solidVolume << " mm^3\n";
    std::cout << "Features: " << part.features().size() << '\n';

    if (argc >= 2) {
        std::string error;
        if (!part.exportStep(argv[1], &error)) {
            std::cerr << "STEP export failed: " << error << '\n';
            return 2;
        }
        std::cout << "Wrote STEP: " << argv[1] << '\n';
    }
    return 0;
}
