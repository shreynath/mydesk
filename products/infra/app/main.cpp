// SPDX-License-Identifier: MPL-2.0
// MyDesk Infra — standalone project entry point
#include "mydesk/infra/infra_document.hpp"

#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

void printUsage(const char* executable) {
    std::cout << "MyDesk Infra 0.2.0\n\n"
              << "Usage:\n"
              << "  " << executable << " <project.myinfra>\n"
              << "  " << executable << " <project.myinfra> --earthworks <road-id> [station-interval-m]\n\n"
              << "Opening a project without an analysis option prints its editable model summary.\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 0;
    }

    std::string error;
    auto document = mydesk::infra::InfraDocument::loadNativeFile(argv[1], &error);
    if (!document) {
        std::cerr << "Could not open '" << argv[1] << "': " << error << '\n';
        return 1;
    }

    if (argc == 2) {
        std::cout << "Project: " << document->name() << '\n'
                  << "CRS: " << document->crs() << '\n'
                  << "Terrain: " << document->dem().cols << " x " << document->dem().rows << " samples\n"
                  << "Roads: " << document->roads().size() << '\n'
                  << "Bridges: " << document->bridges().size() << '\n'
                  << "Buildings: " << document->buildings().size() << '\n';
        return 0;
    }

    if (std::string(argv[2]) != "--earthworks" || argc < 4 || argc > 5) {
        printUsage(argv[0]);
        return 2;
    }

    try {
        std::size_t consumed = 0;
        const std::string roadIdText(argv[3]);
        const auto roadId = static_cast<mydesk::infra::ElementId>(std::stoull(roadIdText, &consumed));
        if (consumed != roadIdText.size()) throw std::invalid_argument("road id");
        const std::string intervalText = argc == 5 ? argv[4] : "10";
        const double interval = std::stod(intervalText, &consumed);
        if (consumed != intervalText.size()) throw std::invalid_argument("interval");
        const auto report = document->analyzeRoadEarthworks(roadId, interval);
        if (!report) {
            std::cerr << "Earthwork analysis failed. Check the road id, intervals, and terrain coverage.\n";
            return 3;
        }
        std::cout << std::fixed << std::setprecision(2)
                  << "Road " << report->roadId << " corridor analysis\n"
                  << "Formation width: " << report->formationWidthMeters << " m\n"
                  << "Maximum grade: " << report->maximumGradePercent << " %\n"
                  << "Cut: " << report->cutVolumeCubicMeters << " m^3\n"
                  << "Fill: " << report->fillVolumeCubicMeters << " m^3\n"
                  << "Sample stations: " << report->samples.size() << '\n';
    } catch (const std::exception&) {
        std::cerr << "Road id and station interval must be valid positive numbers.\n";
        return 2;
    }
    return 0;
}
