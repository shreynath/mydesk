// SPDX-License-Identifier: MPL-2.0
// MyDesk Draft LT — Professional 2D Drafting Standalone Application
#include "mydesk/commands/session.hpp"
#include "mydesk/document/document.hpp"
#include "mydesk/exchange/native_format.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::cout << "MyDesk Draft LT v0.1.0 — AutoCAD LT Class Professional 2D Drafting\n";
    mydesk::commands::Session session;
    session.document().setName("Drawing_LT");

    session.executeLine("LAYER NEW CONSTRUCTION");
    session.executeLine("LAYER LTYPE CONSTRUCTION DASHED");
    session.executeLine("LAYER LWEIGHT CONSTRUCTION 0.35");
    session.executeLine("LAYER MAKE CONSTRUCTION");
    session.executeLine("L 0,0 100,0");

    std::cout << "Layers: " << session.document().layers().size() << "\n";
    std::cout << "Entities: " << session.document().entities().size() << "\n";

    if (argc >= 2) {
        std::string path = argv[1];
        auto report = mydesk::exchange::saveNative(session.document(), path);
        if (!report.ok) {
            std::cerr << "Failed to save: " << (report.errors.empty() ? "error" : report.errors.front()) << "\n";
            return 1;
        }
        std::cout << "Saved: " << path << "\n";
    }

    return 0;
}
