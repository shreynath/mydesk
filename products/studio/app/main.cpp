// SPDX-License-Identifier: MPL-2.0
// MyDesk Studio v0.1.0 — 3ds Max-class DCC application
// Interactive command-line interface for 3D modeling and animation.
#include "mydesk/studio/studio_document.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mydesk::studio;

static void printBanner() {
    std::cout << R"(
╔══════════════════════════════════════════════════════╗
║       MyDesk Studio  v0.1.0 — 3D Modeling            ║
║  Open-source Autodesk 3ds Max class alternative      ║
╚══════════════════════════════════════════════════════╝
)" << '\n';
}

static void printHelp() {
    std::cout <<
        "Primitives:\n"
        "  box <name> <length> <width> <height>    Create box primitive\n"
        "  cylinder <name> <radius> <height> [sides]  Create cylinder\n"
        "  sphere <name> <radius> [segments]       Create sphere\n"
        "\nModifier stack:\n"
        "  twist <obj_id> <angle_deg>              Add Twist modifier\n"
        "  bend <obj_id> <angle_deg> [dir_deg]     Add Bend modifier\n"
        "  taper <obj_id> <amount> [curve]         Add Taper modifier\n"
        "  turbosmooth <obj_id> [iterations]       Add TurboSmooth modifier\n"
        "  togglemod <obj_id> <mod_id>             Enable/disable modifier\n"
        "\nObjects:\n"
        "  ls                     List all objects\n"
        "  info <obj_id>          Show object details and modifier stack\n"
        "  eval                   Evaluate all objects (update meshes)\n"
        "\nMaterials:\n"
        "  mat <name> <r> <g> <b> [metallic] [roughness]  Add PBR material\n"
        "  mats                   List materials\n"
        "\nExport:\n"
        "  exportobj <obj_id> [output.obj]  Export mesh as Wavefront OBJ\n"
        "\n  help                 Show this help\n"
        "  exit                 Exit\n\n";
}

static std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream ss(line);
    std::string tok;
    while (ss >> tok) tokens.push_back(tok);
    return tokens;
}

static double toDouble(const std::string& s) { return std::stod(s); }
static ObjectId toObjId(const std::string& s) { return static_cast<ObjectId>(std::stoull(s)); }

static const char* primName(PrimitiveKind k) {
    switch (k) {
        case PrimitiveKind::Box:      return "Box";
        case PrimitiveKind::Cylinder: return "Cylinder";
        case PrimitiveKind::Sphere:   return "Sphere";
    }
    return "Unknown";
}

static const char* modName(ModifierKind k) {
    switch (k) {
        case ModifierKind::Bend:        return "Bend";
        case ModifierKind::Taper:       return "Taper";
        case ModifierKind::Twist:       return "Twist";
        case ModifierKind::TurboSmooth: return "TurboSmooth";
    }
    return "Unknown";
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    printBanner();
    printHelp();

    StudioDocument doc;
    doc.setName("Untitled");

    std::cout << "studio> " << std::flush;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) { std::cout << "studio> " << std::flush; continue; }
        auto tok = tokenize(line);
        if (tok.empty()) { std::cout << "studio> " << std::flush; continue; }

        const std::string& cmd = tok[0];

        if (cmd == "exit" || cmd == "quit") {
            std::cout << "Exiting MyDesk Studio.\n";
            break;

        } else if (cmd == "help") {
            printHelp();

        } else if (cmd == "box" && tok.size() >= 5) {
            auto id = doc.addBox(tok[1], toDouble(tok[2]), toDouble(tok[3]), toDouble(tok[4]));
            const auto* obj = doc.findObject(id);
            std::cout << "Box [" << id << "] '" << tok[1] << "'\n"
                      << "  Verts=" << obj->evaluatedMesh.vertices.size()
                      << "  Tris=" << obj->evaluatedMesh.triangles.size() << "\n";

        } else if (cmd == "cylinder" && tok.size() >= 4) {
            int sides = (tok.size() >= 5) ? static_cast<int>(std::stoi(tok[4])) : 16;
            auto id = doc.addCylinder(tok[1], toDouble(tok[2]), toDouble(tok[3]), sides);
            const auto* obj = doc.findObject(id);
            std::cout << "Cylinder [" << id << "] '" << tok[1] << "'\n"
                      << "  Verts=" << obj->evaluatedMesh.vertices.size()
                      << "  Tris=" << obj->evaluatedMesh.triangles.size() << "\n";

        } else if (cmd == "sphere" && tok.size() >= 3) {
            int segs = (tok.size() >= 4) ? static_cast<int>(std::stoi(tok[3])) : 12;
            auto id = doc.addSphere(tok[1], toDouble(tok[2]), segs);
            const auto* obj = doc.findObject(id);
            std::cout << "Sphere [" << id << "] '" << tok[1] << "'\n"
                      << "  Verts=" << obj->evaluatedMesh.vertices.size()
                      << "  Tris=" << obj->evaluatedMesh.triangles.size() << "\n";

        } else if (cmd == "twist" && tok.size() >= 3) {
            auto objId = toObjId(tok[1]);
            auto modId = doc.addTwistModifier(objId, toDouble(tok[2]));
            const auto* obj = doc.findObject(objId);
            std::cout << "Twist [mod=" << modId << "] on " << obj->name
                      << "  angle=" << tok[2] << "°"
                      << "  → " << obj->evaluatedMesh.vertices.size() << " verts\n";

        } else if (cmd == "bend" && tok.size() >= 3) {
            auto objId = toObjId(tok[1]);
            double dir = (tok.size() >= 4) ? toDouble(tok[3]) : 0.0;
            auto modId = doc.addBendModifier(objId, toDouble(tok[2]), dir);
            const auto* obj = doc.findObject(objId);
            std::cout << "Bend [mod=" << modId << "] on " << obj->name
                      << "  angle=" << tok[2] << "°\n";

        } else if (cmd == "taper" && tok.size() >= 3) {
            auto objId = toObjId(tok[1]);
            double curve = (tok.size() >= 4) ? toDouble(tok[3]) : 0.0;
            auto modId = doc.addTaperModifier(objId, toDouble(tok[2]), curve);
            const auto* obj = doc.findObject(objId);
            std::cout << "Taper [mod=" << modId << "] on " << obj->name
                      << "  amount=" << tok[2] << "\n";

        } else if (cmd == "turbosmooth" && tok.size() >= 2) {
            auto objId = toObjId(tok[1]);
            int iters = (tok.size() >= 3) ? std::stoi(tok[2]) : 1;
            auto modId = doc.addTurboSmoothModifier(objId, iters);
            const auto* obj = doc.findObject(objId);
            std::cout << "TurboSmooth [mod=" << modId << "] on " << obj->name
                      << "  iters=" << iters
                      << "  → " << obj->evaluatedMesh.vertices.size() << " verts, "
                      << obj->evaluatedMesh.triangles.size() << " tris\n";

        } else if (cmd == "togglemod" && tok.size() >= 3) {
            auto objId = toObjId(tok[1]);
            auto modId = static_cast<ModifierId>(std::stoull(tok[2]));
            auto* obj  = doc.findObject(objId);
            if (!obj) { std::cout << "Object not found.\n"; }
            else {
                // Find current state
                for (const auto& mod : obj->modifierStack) {
                    if (mod.id == modId) {
                        bool now = !mod.enabled;
                        doc.setModifierEnabled(objId, modId, now);
                        doc.evaluateObject(*obj);
                        std::cout << "Modifier " << modId << " " << (now ? "enabled" : "disabled")
                                  << " on " << obj->name << "\n";
                        break;
                    }
                }
            }

        } else if (cmd == "ls") {
            if (doc.objects().empty()) { std::cout << "  (no objects)\n"; }
            for (const auto& obj : doc.objects()) {
                std::cout << "  [" << obj.id << "] " << obj.name
                          << " (" << primName(obj.primitive) << ")"
                          << "  mods=" << obj.modifierStack.size()
                          << "  verts=" << obj.evaluatedMesh.vertices.size()
                          << "  tris=" << obj.evaluatedMesh.triangles.size() << "\n";
            }

        } else if (cmd == "info" && tok.size() >= 2) {
            auto objId = toObjId(tok[1]);
            const auto* obj = doc.findObject(objId);
            if (!obj) { std::cout << "Object not found.\n"; }
            else {
                auto bb = obj->evaluatedMesh.computeBoundingBox();
                std::cout << "Object [" << obj->id << "] '" << obj->name << "'\n"
                          << "  Primitive : " << primName(obj->primitive) << "\n"
                          << "  Vertices  : " << obj->evaluatedMesh.vertices.size() << "\n"
                          << "  Triangles : " << obj->evaluatedMesh.triangles.size() << "\n"
                          << "  Surface A : " << std::fixed << std::setprecision(4)
                          << obj->evaluatedMesh.computeSurfaceArea() << " m²\n"
                          << "  BBox      : (" << bb[0] << "," << bb[1] << "," << bb[2]
                          << ") – (" << bb[3] << "," << bb[4] << "," << bb[5] << ")\n"
                          << "  Modifiers :\n";
                if (obj->modifierStack.empty()) std::cout << "    (none)\n";
                for (const auto& mod : obj->modifierStack) {
                    std::cout << "    [" << mod.id << "] " << modName(mod.kind)
                              << (mod.enabled ? "" : " [DISABLED]") << "\n";
                }
            }

        } else if (cmd == "eval") {
            doc.evaluateAll();
            std::cout << "All objects re-evaluated.\n";

        } else if (cmd == "mat" && tok.size() >= 5) {
            PbrMaterial mat;
            mat.name = tok[1];
            mat.baseColor[0] = static_cast<float>(toDouble(tok[2]));
            mat.baseColor[1] = static_cast<float>(toDouble(tok[3]));
            mat.baseColor[2] = static_cast<float>(toDouble(tok[4]));
            if (tok.size() >= 6) mat.metallic  = static_cast<float>(toDouble(tok[5]));
            if (tok.size() >= 7) mat.roughness = static_cast<float>(toDouble(tok[6]));
            auto idx = doc.addMaterial(std::move(mat));
            const auto& m = doc.materials()[idx];
            std::cout << "Material [" << idx << "] '" << m.name << "'"
                      << "  rgb=(" << m.baseColor[0] << "," << m.baseColor[1] << "," << m.baseColor[2] << ")"
                      << "  metal=" << m.metallic << "  rough=" << m.roughness << "\n";

        } else if (cmd == "mats") {
            if (doc.materials().empty()) { std::cout << "  (no materials)\n"; }
            for (std::size_t i = 0; i < doc.materials().size(); ++i) {
                const auto& m = doc.materials()[i];
                std::cout << "  [" << i << "] " << m.name
                          << "  metal=" << m.metallic << "  rough=" << m.roughness << "\n";
            }

        } else if (cmd == "exportobj" && tok.size() >= 2) {
            auto objId = toObjId(tok[1]);
            auto obj = doc.exportObj(objId);
            if (obj.empty()) {
                std::cout << "Object not found or empty mesh.\n";
            } else if (tok.size() >= 3) {
                std::ofstream f(tok[2]);
                if (f) { f << obj; std::cout << "OBJ written to " << tok[2] << "\n"; }
                else   { std::cout << "ERROR: cannot write " << tok[2] << "\n"; }
            } else {
                std::cout << obj;
            }

        } else {
            std::cout << "Unknown command: " << cmd << "  (type 'help')\n";
        }

        std::cout << "studio> " << std::flush;
    }
    return 0;
}
