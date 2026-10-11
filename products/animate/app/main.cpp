// SPDX-License-Identifier: MPL-2.0
// MyDesk Animate v0.1.0 — Maya-class DCC application
// Interactive command-line interface for scene authoring and animation.
#include "mydesk/animate/animate_document.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mydesk::animate;

static void printBanner() {
    std::cout << R"(
╔══════════════════════════════════════════════════════╗
║       MyDesk Animate  v0.1.0 — DCC System            ║
║   Open-source Autodesk Maya class alternative        ║
╚══════════════════════════════════════════════════════╝
)" << '\n';
}

static void printHelp() {
    std::cout <<
        "Scene graph commands:\n"
        "  addnode <name> <type>               Add node (transform|mesh|camera|light|joint)\n"
        "  settrans <id> <tx> <ty> <tz>        Set translation\n"
        "  setrot <id> <rx_deg> <ry_deg> <rz_deg>  Set euler rotation (deg → quat)\n"
        "  setscale <id> <sx> <sy> <sz>        Set scale\n"
        "  parent <child_id> <parent_id>       Set parent node\n"
        "  ls                                  List all nodes\n"
        "  info <id>                           Show node details\n"
        "\nAnimation commands:\n"
        "  key <node_id> <channel> <frame> <value>\n"
        "                  Add keyframe (channel: tx|ty|tz|rx|ry|rz|sx|sy|sz)\n"
        "  eval <node_id> <frame>              Evaluate node transform at frame\n"
        "  timeline [start] [end] [fps]        Show or set timeline\n"
        "\nMesh commands:\n"
        "  addmesh <node_id> <box|sphere>      Add procedural mesh geometry\n"
        "  meshinfo <mesh_idx>                 Mesh statistics\n"
        "\nCamera/Light commands:\n"
        "  addcam <node_id> [fov_deg]          Add camera to node\n"
        "  addlight <node_id> <point|dir|spot> [intensity]\n"
        "                                      Add light to node\n"
        "\nExport commands:\n"
        "  exportusd [output.usda]             Export scene as USD ASCII\n"
        "  exportabc [output.abc]              Export minimal Alembic\n"
        "\n  help                              Show this help\n"
        "  exit                              Exit\n\n";
}

static std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream ss(line);
    std::string tok;
    while (ss >> tok) tokens.push_back(tok);
    return tokens;
}

static double toDouble(const std::string& s) { return std::stod(s); }
static NodeId toNodeId(const std::string& s) { return static_cast<NodeId>(std::stoul(s)); }

static constexpr double DEG2RAD = 3.14159265358979323846 / 180.0;

// Simple Euler XYZ → quaternion conversion
static Quat eulerToQuat(double rx, double ry, double rz) noexcept {
    double cx = std::cos(rx * DEG2RAD * 0.5), sx = std::sin(rx * DEG2RAD * 0.5);
    double cy = std::cos(ry * DEG2RAD * 0.5), sy = std::sin(ry * DEG2RAD * 0.5);
    double cz = std::cos(rz * DEG2RAD * 0.5), sz = std::sin(rz * DEG2RAD * 0.5);
    return {
        sx*cy*cz + cx*sy*sz,
        cx*sy*cz - sx*cy*sz,
        cx*cy*sz + sx*sy*cz,
        cx*cy*cz - sx*sy*sz
    };
}

static const char* kindName(NodeKind k) {
    switch (k) {
        case NodeKind::Transform: return "transform";
        case NodeKind::Mesh:      return "mesh";
        case NodeKind::Camera:    return "camera";
        case NodeKind::Light:     return "light";
        case NodeKind::Joint:     return "joint";
        case NodeKind::Locator:   return "locator";
    }
    return "unknown";
}

static NodeKind parseKind(const std::string& s) {
    if (s == "mesh")      return NodeKind::Mesh;
    if (s == "camera")    return NodeKind::Camera;
    if (s == "light")     return NodeKind::Light;
    if (s == "joint")     return NodeKind::Joint;
    if (s == "locator")   return NodeKind::Locator;
    return NodeKind::Transform;
}

static Mesh makeBoxMesh() {
    // Unit cube centred at origin
    Mesh m;
    m.positions = {
        {-0.5,-0.5,-0.5},{0.5,-0.5,-0.5},{0.5,0.5,-0.5},{-0.5,0.5,-0.5},
        {-0.5,-0.5, 0.5},{0.5,-0.5, 0.5},{0.5,0.5, 0.5},{-0.5,0.5, 0.5}
    };
    m.triangles = {
        {0,1,2},{0,2,3}, // -Z
        {4,5,6},{4,6,7}, // +Z
        {0,4,7},{0,7,3}, // -X
        {1,5,6},{1,6,2}, // +X
        {0,1,5},{0,5,4}, // -Y
        {3,2,6},{3,6,7}  // +Y
    };
    return m;
}

static Mesh makeSphereMesh(int latDiv = 8, int lonDiv = 12) {
    Mesh m;
    constexpr double PI = 3.14159265358979323846;
    for (int lat = 0; lat <= latDiv; ++lat) {
        double phi = PI * lat / latDiv;
        for (int lon = 0; lon <= lonDiv; ++lon) {
            double theta = 2.0 * PI * lon / lonDiv;
            double x = std::sin(phi) * std::cos(theta);
            double y = std::cos(phi);
            double z = std::sin(phi) * std::sin(theta);
            m.positions.push_back({x, y, z});
            m.normals.push_back({x, y, z});
        }
    }
    for (int lat = 0; lat < latDiv; ++lat) {
        for (int lon = 0; lon < lonDiv; ++lon) {
            auto v0 = static_cast<std::uint32_t>(lat * (lonDiv+1) + lon);
            auto v1 = v0 + 1;
            auto v2 = static_cast<std::uint32_t>((lat+1) * (lonDiv+1) + lon);
            auto v3 = v2 + 1;
            m.triangles.push_back({v0, v2, v1});
            m.triangles.push_back({v1, v2, v3});
        }
    }
    return m;
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    printBanner();
    printHelp();

    AnimateDocument doc;
    std::cout << "animate> " << std::flush;
    std::string line;

    while (std::getline(std::cin, line)) {
        if (line.empty()) { std::cout << "animate> " << std::flush; continue; }
        auto tok = tokenize(line);
        if (tok.empty()) { std::cout << "animate> " << std::flush; continue; }

        const std::string& cmd = tok[0];

        if (cmd == "exit" || cmd == "quit") {
            std::cout << "Exiting MyDesk Animate.\n";
            break;

        } else if (cmd == "help") {
            printHelp();

        } else if (cmd == "addnode" && tok.size() >= 3) {
            auto kind = parseKind(tok[2]);
            auto id   = doc.add_node(tok[1], kind);
            std::cout << "Node [" << id << "] '" << tok[1] << "' (" << kindName(kind) << ")\n";

        } else if (cmd == "settrans" && tok.size() >= 5) {
            auto id = toNodeId(tok[1]);
            auto& n = doc.node(id);
            n.translate = {toDouble(tok[2]), toDouble(tok[3]), toDouble(tok[4])};
            std::cout << "Node " << id << " translate set\n";

        } else if (cmd == "setrot" && tok.size() >= 5) {
            auto id = toNodeId(tok[1]);
            auto& n = doc.node(id);
            n.rotate = eulerToQuat(toDouble(tok[2]), toDouble(tok[3]), toDouble(tok[4]));
            std::cout << "Node " << id << " rotation set\n";

        } else if (cmd == "setscale" && tok.size() >= 5) {
            auto id = toNodeId(tok[1]);
            auto& n = doc.node(id);
            n.scale = {toDouble(tok[2]), toDouble(tok[3]), toDouble(tok[4])};
            std::cout << "Node " << id << " scale set\n";

        } else if (cmd == "parent" && tok.size() >= 3) {
            auto child  = toNodeId(tok[1]);
            auto parent = toNodeId(tok[2]);
            doc.node(child).parent = parent;
            doc.node(parent).children.push_back(child);
            std::cout << "Node " << child << " → parent " << parent << "\n";

        } else if (cmd == "ls") {
            if (doc.node_count() == 0) { std::cout << "  (empty scene)\n"; }
            auto roots = doc.root_nodes();
            for (auto rid : roots) {
                const auto& n = doc.node(rid);
                std::cout << "  [" << n.id << "] " << n.name << " (" << kindName(n.kind) << ")"
                          << "  children=" << n.children.size()
                          << (n.anim.has_value() ? " [animated]" : "") << "\n";
            }

        } else if (cmd == "info" && tok.size() >= 2) {
            auto id = toNodeId(tok[1]);
            const auto& n = doc.node(id);
            std::cout << "Node [" << n.id << "] '" << n.name << "'\n"
                      << "  Kind      : " << kindName(n.kind) << "\n"
                      << "  Parent    : " << (n.parent == kInvalidNode ? "(root)" : std::to_string(n.parent)) << "\n"
                      << "  Children  : " << n.children.size() << "\n"
                      << "  Translate : " << n.translate.x << ", " << n.translate.y << ", " << n.translate.z << "\n"
                      << "  Scale     : " << n.scale.x << ", " << n.scale.y << ", " << n.scale.z << "\n"
                      << "  Animated  : " << (n.anim.has_value() ? "yes" : "no") << "\n";

        } else if (cmd == "key" && tok.size() >= 5) {
            auto id      = toNodeId(tok[1]);
            const auto& ch = tok[2];
            double frame = toDouble(tok[3]);
            double val   = toDouble(tok[4]);

            auto& n = doc.node(id);
            if (!n.anim.has_value()) n.anim.emplace();

            double t = doc.timeline().frame_to_time(frame);
            Keyframe kf;
            kf.time = t; kf.value = val;

            auto& a = n.anim.value();
            AnimCurve* curve = nullptr;
            if      (ch == "tx") curve = &a.tx;
            else if (ch == "ty") curve = &a.ty;
            else if (ch == "tz") curve = &a.tz;
            else if (ch == "rx") curve = &a.rx;
            else if (ch == "ry") curve = &a.ry;
            else if (ch == "rz") curve = &a.rz;
            else if (ch == "sx") curve = &a.sx;
            else if (ch == "sy") curve = &a.sy;
            else if (ch == "sz") curve = &a.sz;

            if (curve) {
                curve->add_key(kf);
                std::cout << "Key added: node " << id << " " << ch
                          << " frame " << frame << " = " << val << "\n";
            } else {
                std::cout << "Unknown channel '" << ch << "'. Use: tx|ty|tz|rx|ry|rz|sx|sy|sz\n";
            }

        } else if (cmd == "eval" && tok.size() >= 3) {
            auto id    = toNodeId(tok[1]);
            double fr  = toDouble(tok[2]);
            double t   = doc.timeline().frame_to_time(fr);
            auto mats  = doc.world_matrices(t);
            if (mats.count(id)) {
                const auto& m = mats[id];
                std::cout << "Node " << id << " world matrix at frame " << fr << ":\n"
                          << std::fixed << std::setprecision(4)
                          << "  [" << m.m[0] << " " << m.m[4] << " " << m.m[8]  << " " << m.m[12] << "]\n"
                          << "  [" << m.m[1] << " " << m.m[5] << " " << m.m[9]  << " " << m.m[13] << "]\n"
                          << "  [" << m.m[2] << " " << m.m[6] << " " << m.m[10] << " " << m.m[14] << "]\n"
                          << "  [" << m.m[3] << " " << m.m[7] << " " << m.m[11] << " " << m.m[15] << "]\n"
                          << "  Translation: " << m.m[12] << ", " << m.m[13] << ", " << m.m[14] << "\n";
            } else {
                std::cout << "Node " << id << " not found in world matrices.\n";
            }

        } else if (cmd == "timeline") {
            if (tok.size() >= 4) {
                doc.timeline().start_frame = toDouble(tok[1]);
                doc.timeline().end_frame   = toDouble(tok[2]);
                doc.timeline().fps         = toDouble(tok[3]);
                std::cout << "Timeline: " << doc.timeline().start_frame
                          << " – " << doc.timeline().end_frame
                          << " @ " << doc.timeline().fps << " fps\n";
            } else {
                std::cout << "Timeline: " << doc.timeline().start_frame
                          << " – " << doc.timeline().end_frame
                          << " @ " << doc.timeline().fps << " fps ("
                          << (doc.timeline().end_frame - doc.timeline().start_frame) / doc.timeline().fps
                          << " s)\n";
            }

        } else if (cmd == "addmesh" && tok.size() >= 3) {
            auto nid  = toNodeId(tok[1]);
            Mesh mesh;
            if (tok[2] == "sphere") mesh = makeSphereMesh();
            else                    mesh = makeBoxMesh();
            mesh.node_id = nid;
            auto idx = doc.add_mesh(std::move(mesh));
            const auto& added = doc.mesh(idx);
            std::cout << "Mesh [" << idx << "] added to node " << nid
                      << ": " << added.positions.size() << " verts, "
                      << added.triangles.size() << " tris\n";

        } else if (cmd == "meshinfo" && tok.size() >= 2) {
            auto idx = static_cast<std::size_t>(std::stoull(tok[1]));
            if (idx >= doc.mesh_count()) { std::cout << "Mesh index out of range\n"; }
            else {
                const auto& m = doc.mesh(idx);
                std::cout << "Mesh [" << idx << "]\n"
                          << "  Vertices  : " << m.positions.size() << "\n"
                          << "  Triangles : " << m.triangles.size() << "\n"
                          << "  Normals   : " << m.normals.size() << "\n"
                          << "  UVs       : " << m.uvs.size() << "\n"
                          << "  Skinned   : " << (m.skin.has_value() ? "yes" : "no") << "\n"
                          << "  Node      : " << m.node_id << "\n";
            }

        } else if (cmd == "addcam" && tok.size() >= 2) {
            auto nid = toNodeId(tok[1]);
            Camera cam;
            cam.node_id = nid;
            if (tok.size() >= 3) cam.fov_y_deg = toDouble(tok[2]);
            auto idx = doc.add_camera(cam);
            std::cout << "Camera [" << idx << "] on node " << nid
                      << "  fov=" << cam.fov_y_deg << "°\n";

        } else if (cmd == "addlight" && tok.size() >= 3) {
            auto nid = toNodeId(tok[1]);
            Light light;
            light.node_id = nid;
            if      (tok[2] == "dir")   light.kind = LightKind::Directional;
            else if (tok[2] == "spot")  light.kind = LightKind::Spot;
            else if (tok[2] == "area")  light.kind = LightKind::Area;
            else                        light.kind = LightKind::Point;
            if (tok.size() >= 4) light.intensity = toDouble(tok[3]);
            auto idx = doc.add_light(light);
            std::cout << "Light [" << idx << "] " << tok[2]
                      << " intensity=" << light.intensity << " on node " << nid << "\n";

        } else if (cmd == "exportusd") {
            auto usda = doc.export_usda();
            if (tok.size() >= 2) {
                std::ofstream f(tok[1]);
                if (f) { f << usda; std::cout << "USD written to " << tok[1] << "\n"; }
                else   { std::cout << "ERROR: cannot write " << tok[1] << "\n"; }
            } else {
                std::cout << usda;
            }

        } else if (cmd == "exportabc") {
            auto abc = doc.export_alembic_minimal();
            if (tok.size() >= 2) {
                std::ofstream f(tok[1], std::ios::binary);
                if (f) {
                    f.write(reinterpret_cast<const char*>(abc.data()),
                            static_cast<std::streamsize>(abc.size()));
                    std::cout << "Alembic written to " << tok[1]
                              << " (" << abc.size() << " bytes)\n";
                } else {
                    std::cout << "ERROR: cannot write " << tok[1] << "\n";
                }
            } else {
                std::cout << "(binary Alembic — specify output file)\n";
            }

        } else {
            std::cout << "Unknown command: " << cmd << "  (type 'help')\n";
        }

        std::cout << "animate> " << std::flush;
    }
    return 0;
}
