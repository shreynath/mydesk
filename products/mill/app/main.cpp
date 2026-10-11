// SPDX-License-Identifier: MPL-2.0
// MyDesk Mill v0.1.0 — PowerMill-class CAM application
// Interactive command-line interface for toolpath programming.
#include "mydesk/mill/mill_document.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mydesk::mill;

static void printBanner() {
    std::cout << R"(
╔══════════════════════════════════════════════════════╗
║         MyDesk Mill  v0.1.0 — CAM System             ║
║  Open-source Autodesk PowerMill class alternative    ║
╚══════════════════════════════════════════════════════╝
)" << '\n';
}

static void printHelp() {
    std::cout <<
        "Commands:\n"
        "  stock <xmin> <ymin> <zmin> <xmax> <ymax> <zmax>\n"
        "                        Define stock bounding box (mm)\n"
        "  addtool <name> <dia_mm> <flutes> [ballend]  Add cutting tool\n"
        "  tools                 List tools\n"
        "  feedspeed <idx>       Show calculated feed/speed for tool\n"
        "  raster <tool_idx> <stepover_mm> <stepdown_mm> <z_top> <z_bot>\n"
        "                        Generate raster finishing pass\n"
        "  profile <tool_idx> <stepdown_mm> <z_top> <z_bot>\n"
        "                        Generate profile pass\n"
        "  parallel <tool_idx> <stepover_mm> <stepdown_mm> <z_top> <z_bot>\n"
        "                        Generate parallel offset pass\n"
        "  pencil <tool_idx> <z_bot>\n"
        "                        Generate pencil finishing pass\n"
        "  boundary <name> <x1> <y1> <x2> <y2> <x3> <y3> [...]\n"
        "                        Add a selected convex machining boundary\n"
        "  drill <tool_idx> <z_bot> <x1> <y1> [x2 y2 ...]\n"
        "                        Drill explicitly selected hole centres\n"
        "  toolpaths             List generated toolpaths\n"
        "  info <idx>            Show toolpath statistics\n"
        "  gcode [output.nc]     Export all toolpaths as G-code\n"
        "  clear                 Clear all toolpaths\n"
        "  help                  Show this help\n"
        "  exit                  Exit\n\n";
}

static std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream ss(line);
    std::string tok;
    while (ss >> tok) tokens.push_back(tok);
    return tokens;
}

static double toDouble(const std::string& s) { return std::stod(s); }
static std::size_t toUSize(const std::string& s) { return std::stoull(s); }

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    printBanner();
    printHelp();

    MillDocument doc;

    // Sensible defaults
    doc.set_stock({{0, 0, -20}, {150, 100, 0}});
    std::cout << "Default stock: 150×100×20 mm\n\n";

    std::cout << "mill> " << std::flush;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) { std::cout << "mill> " << std::flush; continue; }
        auto tok = tokenize(line);
        if (tok.empty()) { std::cout << "mill> " << std::flush; continue; }

        const std::string& cmd = tok[0];

        if (cmd == "exit" || cmd == "quit") {
            std::cout << "Exiting MyDesk Mill.\n";
            break;

        } else if (cmd == "help") {
            printHelp();

        } else if (cmd == "stock" && tok.size() >= 7) {
            StockBox s;
            s.min_corner = {toDouble(tok[1]), toDouble(tok[2]), toDouble(tok[3])};
            s.max_corner = {toDouble(tok[4]), toDouble(tok[5]), toDouble(tok[6])};
            doc.set_stock(s);
            std::cout << "Stock set: ("
                      << s.min_corner.x << "," << s.min_corner.y << "," << s.min_corner.z
                      << ") → ("
                      << s.max_corner.x << "," << s.max_corner.y << "," << s.max_corner.z
                      << ") mm\n";

        } else if (cmd == "addtool" && tok.size() >= 4) {
            Tool t;
            t.name        = tok[1];
            t.diameter_mm = toDouble(tok[2]);
            t.flutes      = static_cast<double>(toUSize(tok[3]));
            if (tok.size() >= 5 && tok[4] == "ballend") {
                t.kind = ToolKind::BallEndmill;
                t.corner_radius_mm = t.diameter_mm * 0.5;
            }
            t.shank_diameter_mm  = t.diameter_mm;
            t.cutting_length_mm  = t.diameter_mm * 4.0;
            t.overall_length_mm  = t.diameter_mm * 8.0;
            auto idx = doc.add_tool(t);
            auto fs  = doc.calc_feed_speed(idx);
            std::cout << "Added tool [" << idx << "] " << t.name
                      << " D=" << t.diameter_mm << "mm "
                      << (t.kind == ToolKind::BallEndmill ? "BallEnd" : "FlatEnd")
                      << " → " << std::fixed << std::setprecision(0)
                      << fs.rpm << " RPM  " << fs.feed_mm_min << " mm/min\n";

        } else if (cmd == "tools") {
            if (doc.tool_count() == 0) {
                std::cout << "  (no tools defined)\n";
            }
            for (std::size_t i = 0; i < doc.tool_count(); ++i) {
                const auto& t = doc.tool(i);
                auto fs = doc.calc_feed_speed(i);
                std::cout << "  [" << i << "] " << t.name
                          << "  D=" << t.diameter_mm << "mm"
                          << "  flutes=" << static_cast<int>(t.flutes)
                          << "  " << std::fixed << std::setprecision(0)
                          << fs.rpm << "rpm  " << fs.feed_mm_min << "mm/min\n";
            }

        } else if (cmd == "feedspeed" && tok.size() >= 2) {
            std::size_t idx = toUSize(tok[1]);
            if (idx >= doc.tool_count()) {
                std::cout << "Tool index out of range\n";
            } else {
                auto fs = doc.calc_feed_speed(idx);
                const auto& t = doc.tool(idx);
                std::cout << "Tool [" << idx << "] " << t.name << "\n"
                          << "  Surface speed : " << t.surface_speed_m_min << " m/min\n"
                          << "  Spindle RPM   : " << std::fixed << std::setprecision(0) << fs.rpm << "\n"
                          << "  Feed rate     : " << fs.feed_mm_min << " mm/min\n";
            }

        } else if (cmd == "boundary" && tok.size() >= 8 && tok.size() % 2 == 0) {
            MachiningBoundary boundary;
            boundary.name = tok[1];
            for (std::size_t i = 2; i + 1 < tok.size(); i += 2) {
                boundary.polygon.push_back({toDouble(tok[i]), toDouble(tok[i + 1])});
            }
            const std::size_t idx = doc.add_boundary(std::move(boundary));
            std::cout << "Added machining boundary [" << idx << "] with "
                      << doc.boundary(idx).polygon.size() << " vertices\n";

        } else if ((cmd == "raster" || cmd == "parallel" || cmd == "profile"
                    || cmd == "pencil" || cmd == "drill") && tok.size() >= 2) {
            OperationParams p;
            p.tool_index = toUSize(tok[1]);
            if (p.tool_index >= doc.tool_count()) {
                std::cout << "Tool index out of range\n";
                std::cout << "mill> " << std::flush;
                continue;
            }

            if (cmd == "raster" && tok.size() >= 6) {
                p.strategy    = Strategy::RasterZigZag;
                p.stepover_mm = toDouble(tok[2]);
                p.stepdown_mm = toDouble(tok[3]);
                p.z_top_mm    = toDouble(tok[4]);
                p.z_bottom_mm = toDouble(tok[5]);
            } else if (cmd == "parallel" && tok.size() >= 6) {
                p.strategy    = Strategy::ParallelOffset;
                p.stepover_mm = toDouble(tok[2]);
                p.stepdown_mm = toDouble(tok[3]);
                p.z_top_mm    = toDouble(tok[4]);
                p.z_bottom_mm = toDouble(tok[5]);
            } else if (cmd == "profile" && tok.size() >= 5) {
                p.strategy    = Strategy::Profile;
                p.stepdown_mm = toDouble(tok[2]);
                p.z_top_mm    = toDouble(tok[3]);
                p.z_bottom_mm = toDouble(tok[4]);
            } else if (cmd == "pencil" && tok.size() >= 3) {
                p.strategy    = Strategy::PencilMilling;
                p.z_bottom_mm = toDouble(tok[2]);
            } else if (cmd == "drill" && tok.size() >= 5 && tok.size() % 2 == 1) {
                p.strategy    = Strategy::Drilling;
                p.z_bottom_mm = toDouble(tok[2]);
                for (std::size_t i = 3; i + 1 < tok.size(); i += 2) {
                    p.drill_points.push_back({toDouble(tok[i]), toDouble(tok[i + 1])});
                }
            } else {
                std::cout << "Insufficient arguments. Type 'help'.\n";
                std::cout << "mill> " << std::flush;
                continue;
            }

            p.safe_z_mm = p.z_top_mm + 10.0;
            auto idx = doc.generate(p);
            const auto& tp = doc.toolpath(idx);
            std::cout << "Generated toolpath [" << idx << "] " << tp.name
                      << "  moves=" << tp.moves.size()
                      << "  cut_length=" << std::fixed << std::setprecision(1)
                      << tp.total_cut_length_mm() << "mm"
                      << "  est=" << std::setprecision(2) << tp.estimated_time_min() << "min\n";

        } else if (cmd == "toolpaths") {
            if (doc.toolpath_count() == 0) {
                std::cout << "  (no toolpaths)\n";
            }
            for (std::size_t i = 0; i < doc.toolpath_count(); ++i) {
                const auto& tp = doc.toolpath(i);
                std::cout << "  [" << i << "] " << tp.name
                          << "  tool=" << tp.params.tool_index
                          << "  moves=" << tp.moves.size()
                          << "  length=" << std::fixed << std::setprecision(1)
                          << tp.total_cut_length_mm() << "mm\n";
            }

        } else if (cmd == "info" && tok.size() >= 2) {
            std::size_t idx = toUSize(tok[1]);
            if (idx >= doc.toolpath_count()) {
                std::cout << "Toolpath index out of range\n";
            } else {
                const auto& tp = doc.toolpath(idx);
                std::cout << "Toolpath [" << idx << "] " << tp.name << "\n"
                          << "  Strategy   : " << tp.params.tool_index << "\n"
                          << "  Tool index : " << tp.params.tool_index << "\n"
                          << "  Stepdown   : " << tp.params.stepdown_mm << " mm\n"
                          << "  Stepover   : " << tp.params.stepover_mm << " mm\n"
                          << "  Z range    : " << tp.params.z_bottom_mm
                          << " to " << tp.params.z_top_mm << " mm\n"
                          << "  Moves      : " << tp.moves.size() << "\n"
                          << "  Cut length : " << std::fixed << std::setprecision(2)
                          << tp.total_cut_length_mm() << " mm\n"
                          << "  Est. time  : " << tp.estimated_time_min() << " min\n";
            }

        } else if (cmd == "gcode") {
            if (doc.toolpath_count() == 0) {
                std::cout << "No toolpaths to export.\n";
            } else {
                GCodeOptions opts;
                std::string gcode = doc.export_gcode(opts);
                if (tok.size() >= 2) {
                    std::ofstream f(tok[1]);
                    if (f) {
                        f << gcode;
                        std::cout << "G-code written to " << tok[1]
                                  << " (" << gcode.size() << " bytes)\n";
                    } else {
                        std::cout << "ERROR: cannot write " << tok[1] << "\n";
                    }
                } else {
                    std::cout << gcode;
                }
            }

        } else if (cmd == "clear") {
            // Rebuild document keeping tools and stock
            std::vector<Tool> savedTools;
            for (std::size_t i = 0; i < doc.tool_count(); ++i)
                savedTools.push_back(doc.tool(i));
            auto savedStock = doc.stock();
            doc = MillDocument{};
            doc.set_stock(savedStock);
            for (auto& t : savedTools) doc.add_tool(std::move(t));
            std::cout << "Toolpaths cleared. Tools and stock preserved.\n";

        } else {
            std::cout << "Unknown command: " << cmd << "  (type 'help')\n";
        }

        std::cout << "mill> " << std::flush;
    }
    return 0;
}
