// SPDX-License-Identifier: MPL-2.0
// MyDesk Surface — Alias Class Class-A Industrial Surfacing Standalone Application
#include "mydesk/surface/surface_document.hpp"

#include <iostream>

int main(int argc, char** argv) {
    std::cout << "MyDesk Surface v0.1.0 — Autodesk Alias Class Industrial Surfacing\n";
    mydesk::surface::SurfaceDocument doc;
    doc.setName("AutomotiveHoodFeature");

    const auto curveId = doc.addCubicCurve("FeatureLine", {
        {0.0, 0.0, 0.0},
        {50.0, 10.0, 5.0},
        {100.0, 15.0, 8.0},
        {150.0, 0.0, 0.0}
    });

    const auto* curve = doc.findCurve(curveId);
    if (curve) {
        std::cout << "Created NURBS Curve '" << curve->name << "' with degree " << curve->degree << "\n";
        std::cout << "Midpoint evaluated: (" << curve->evaluate(0.5).x << ", "
                  << curve->evaluate(0.5).y << ", " << curve->evaluate(0.5).z << ")\n";
        std::cout << "Midpoint curvature: " << curve->curvatureAt(0.5) << " mm^-1\n";
    }

    const auto patchId = doc.addBilinearPatch("TestPatch",
        {0, 0, 0}, {100, 0, 0},
        {0, 50, 0}, {100, 50, 10}
    );
    const auto zebra = doc.evaluateZebraStripes(patchId, 10, 10);
    std::cout << "Evaluated zebra reflection field: " << zebra.size() << " samples.\n";

    (void)argc;
    (void)argv;
    return 0;
}
