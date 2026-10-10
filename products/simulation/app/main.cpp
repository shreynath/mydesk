// SPDX-License-Identifier: MPL-2.0
// MyDesk Simulation — headless CLI entry point
#include "mydesk/simulation/simulation_document.hpp"
#include <cstdio>

int main() {
    mydesk::simulation::SimulationDocument doc;
    std::printf("MyDesk Simulation engine ready. Materials: %zu\n",
                doc.material_count());
    return 0;
}
