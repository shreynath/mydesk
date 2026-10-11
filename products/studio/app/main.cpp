// SPDX-License-Identifier: MPL-2.0
// MyDesk Studio — 3ds Max Class 3D Modeling & Rendering Standalone Application
#include "mydesk/studio/studio_document.hpp"

#include <iostream>

int main(int argc, char** argv) {
    std::cout << "MyDesk Studio v0.1.0 — Autodesk 3ds Max Class Modeling & Rendering\n";
    mydesk::studio::StudioDocument doc;
    doc.setName("ArchitecturalVisualization");

    const auto boxId = doc.addBox("Pillar", 1.0, 1.0, 5.0);
    doc.addTwistModifier(boxId, 45.0);
    doc.addTaperModifier(boxId, -0.2, 0.0);

    const auto* obj = doc.findObject(boxId);
    if (obj) {
        std::cout << "Created Object: " << obj->name << "\n";
        std::cout << "Modifiers in stack: " << obj->modifierStack.size() << "\n";
        std::cout << "Evaluated Mesh: " << obj->evaluatedMesh.vertices.size() << " vertices, "
                  << obj->evaluatedMesh.triangles.size() << " triangles.\n";
        std::cout << "Surface Area: " << obj->evaluatedMesh.computeSurfaceArea() << " m^2\n";
    }

    (void)argc;
    (void)argv;
    return 0;
}
