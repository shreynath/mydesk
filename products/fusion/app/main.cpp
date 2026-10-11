// SPDX-License-Identifier: MPL-2.0
// MyDesk Fusion — Integrated CAD/CAM/CAE Standalone Application
#include "mydesk/fusion/fusion_document.hpp"

#include <iostream>

int main(int argc, char** argv) {
    std::cout << "MyDesk Fusion v0.1.0 — Autodesk Fusion Class Integrated CAD/CAM/CAE\n";
    mydesk::fusion::FusionDocument doc;
    doc.setName("DroneArmBracket");

    const auto sk1 = doc.addSketchRectangle("BaseSketch", 80.0, 40.0);
    const auto ex1 = doc.addExtrude("MainExtrude", sk1, 15.0);
    const auto pushed = doc.addDirectPushPull("FaceOffset", ex1, 5.0);
    doc.addCAMPocket("RoughPocket", pushed, 6.0, 10.0, 2.5, 3.0);
    doc.addECADBoard("MountingPCB", 70.0, 35.0, 1.6);

    std::cout << "Timeline features: " << doc.timeline().size() << "\n";
    std::cout << "Solid bodies: " << doc.bodies().size() << "\n";
    if (!doc.bodies().empty()) {
        std::cout << "Volume: " << doc.bodies().front().volumeMm3 << " mm^3\n";
    }
    std::cout << "CAM Setups: " << doc.camSetups().size() << "\n";

    (void)argc;
    (void)argv;
    return 0;
}
