// SPDX-License-Identifier: MPL-2.0
// MyDesk Simulation v0.1.0 — CST FEA application
// Interactive command-line interface for linear static structural analysis.
#include "mydesk/simulation/simulation_document.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mydesk::simulation;

static void printBanner() {
    std::cout << R"(
╔══════════════════════════════════════════════════════╗
║      MyDesk Simulation  v0.1.0 — FEA System          ║
║  Open-source professional structural analysis tool   ║
╚══════════════════════════════════════════════════════╝
Solver: 2D Plane-Stress CST (Constant Strain Triangle)
        Gaussian elimination with partial pivoting
        Von Mises yield criterion
)" << '\n';
}

static void printHelp() {
    std::cout <<
        "Commands:\n"
        "  mat <name> <E_GPa> <nu> <yield_MPa>   Add material\n"
        "  node <x_m> <y_m>                       Add node (metres)\n"
        "  elem <n0> <n1> <n2> <mat_id> [thick_m] Add CST element\n"
        "  fix <node_id> <dof>     Fix DOF: x | y | xy\n"
        "  load <node_id> <Fx_N> <Fy_N>          Apply nodal force\n"
        "  nodes                   List nodes\n"
        "  elements                List elements\n"
        "  solve                   Run linear static solve\n"
        "  report [output.txt]     Print analysis report\n"
        "  demo cantilever         Load cantilever beam benchmark\n"
        "  demo bracket            Load L-bracket benchmark\n"
        "  help                    Show this help\n"
        "  exit                    Exit\n\n";
}

static std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream ss(line);
    std::string tok;
    while (ss >> tok) tokens.push_back(tok);
    return tokens;
}

static double toDouble(const std::string& s) { return std::stod(s); }
static std::uint32_t toU32(const std::string& s) { return static_cast<std::uint32_t>(std::stoul(s)); }

// ─── Pre-built benchmarks ────────────────────────────────────────────────────

/// Two-element cantilever: fixed at left, unit Y load at right.
/// Reference: Zienkiewicz & Taylor, Example 4.2
static void loadCantilever(SimulationDocument& doc) {
    doc = SimulationDocument{};

    Material steel;
    steel.name              = "Steel";
    steel.youngs_modulus_pa = 200e9;
    steel.poissons_ratio    = 0.3;
    steel.yield_strength_pa = 250e6;
    auto matId = doc.add_material(steel);

    // 4 nodes: rectangle 0.2m × 0.1m
    //  3---2
    //  |   |
    //  0---1
    const auto n0 = doc.add_node(0.0, 0.0);
    const auto n1 = doc.add_node(0.2, 0.0);
    const auto n2 = doc.add_node(0.2, 0.1);
    const auto n3 = doc.add_node(0.0, 0.1);

    doc.add_element({n0, n1, n2}, matId, 0.01); // lower-right triangle
    doc.add_element({n0, n2, n3}, matId, 0.01); // upper-left triangle

    // Fix left edge
    doc.add_bc({n0, BCKind::FixedXY, 0.0});
    doc.add_bc({n3, BCKind::FixedXY, 0.0});

    // Apply -1000 N in Y at right edge (shared between n1, n2)
    doc.add_load({n1, 0.0, -500.0});
    doc.add_load({n2, 0.0, -500.0});

    std::cout << "Loaded cantilever benchmark: 2 CST elements, 4 nodes, steel 200 GPa, -1000 N tip load.\n";
}

/// Simple L-bracket: 4-element domain
static void loadBracket(SimulationDocument& doc) {
    doc = SimulationDocument{};

    Material aluminium;
    aluminium.name              = "Aluminium6061";
    aluminium.youngs_modulus_pa = 69e9;
    aluminium.poissons_ratio    = 0.33;
    aluminium.yield_strength_pa = 276e6;
    auto matId = doc.add_material(aluminium);

    //  5---4
    //  |   |
    //  3---2---1
    //      |   |
    //      0---1  (repeat at bottom)
    // Simplified 4-triangle L:
    const auto n0 = doc.add_node(0.0, 0.0);
    const auto n1 = doc.add_node(0.1, 0.0);
    const auto n2 = doc.add_node(0.1, 0.1);
    const auto n3 = doc.add_node(0.0, 0.1);
    const auto n4 = doc.add_node(0.1, 0.2);
    const auto n5 = doc.add_node(0.0, 0.2);

    doc.add_element({n0, n1, n2}, matId, 0.005);
    doc.add_element({n0, n2, n3}, matId, 0.005);
    doc.add_element({n3, n2, n4}, matId, 0.005);
    doc.add_element({n3, n4, n5}, matId, 0.005);

    // Fix bottom edge
    doc.add_bc({n0, BCKind::FixedXY, 0.0});
    doc.add_bc({n1, BCKind::FixedXY, 0.0});

    // Apply horizontal load at top-right
    doc.add_load({n4, 5000.0, 0.0});
    doc.add_load({n5, 5000.0, 0.0});

    std::cout << "Loaded L-bracket benchmark: 4 CST elements, 6 nodes, aluminium 69 GPa, 10 kN horizontal load.\n";
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    printBanner();
    printHelp();

    SimulationDocument doc;
    SimulationResult lastResult;
    bool haveSolution = false;

    std::cout << "sim> " << std::flush;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) { std::cout << "sim> " << std::flush; continue; }
        auto tok = tokenize(line);
        if (tok.empty()) { std::cout << "sim> " << std::flush; continue; }

        const std::string& cmd = tok[0];

        if (cmd == "exit" || cmd == "quit") {
            std::cout << "Exiting MyDesk Simulation.\n";
            break;

        } else if (cmd == "help") {
            printHelp();

        } else if (cmd == "mat" && tok.size() >= 5) {
            Material m;
            m.name              = tok[1];
            m.youngs_modulus_pa = toDouble(tok[2]) * 1e9; // input in GPa
            m.poissons_ratio    = toDouble(tok[3]);
            m.yield_strength_pa = toDouble(tok[4]) * 1e6; // input in MPa
            auto id = doc.add_material(m);
            std::cout << "Material [" << id << "] " << m.name
                      << "  E=" << toDouble(tok[2]) << " GPa"
                      << "  nu=" << m.poissons_ratio
                      << "  yield=" << toDouble(tok[4]) << " MPa\n";
            haveSolution = false;

        } else if (cmd == "node" && tok.size() >= 3) {
            auto id = doc.add_node(toDouble(tok[1]), toDouble(tok[2]));
            std::cout << "Node [" << id << "] at ("
                      << toDouble(tok[1]) << ", " << toDouble(tok[2]) << ") m\n";
            haveSolution = false;

        } else if (cmd == "elem" && tok.size() >= 5) {
            auto n0 = toU32(tok[1]), n1 = toU32(tok[2]), n2 = toU32(tok[3]);
            auto mat = toU32(tok[4]);
            double thick = (tok.size() >= 6) ? toDouble(tok[5]) : 0.01;
            auto id = doc.add_element({n0, n1, n2}, mat, thick);
            std::cout << "Element [" << id << "] nodes=(" << n0 << "," << n1 << "," << n2
                      << ")  mat=" << mat << "  t=" << thick << " m\n";
            haveSolution = false;

        } else if (cmd == "fix" && tok.size() >= 3) {
            auto nid = toU32(tok[1]);
            BCKind kind = BCKind::FixedXY;
            if      (tok[2] == "x")  kind = BCKind::FixedX;
            else if (tok[2] == "y")  kind = BCKind::FixedY;
            else if (tok[2] == "xy") kind = BCKind::FixedXY;
            doc.add_bc({nid, kind, 0.0});
            std::cout << "Fixed node " << nid << " DOF " << tok[2] << "\n";
            haveSolution = false;

        } else if (cmd == "load" && tok.size() >= 4) {
            auto nid = toU32(tok[1]);
            doc.add_load({nid, toDouble(tok[2]), toDouble(tok[3])});
            std::cout << "Load on node " << nid
                      << ": Fx=" << toDouble(tok[2]) << " N"
                      << "  Fy=" << toDouble(tok[3]) << " N\n";
            haveSolution = false;

        } else if (cmd == "nodes") {
            for (std::size_t i = 0; i < doc.node_count(); ++i) {
                const auto& n = doc.node(static_cast<std::uint32_t>(i));
                std::cout << "  [" << n.id << "] (" << n.x << ", " << n.y << ") m\n";
            }
            if (doc.node_count() == 0) std::cout << "  (none)\n";

        } else if (cmd == "elements") {
            for (std::size_t i = 0; i < doc.element_count(); ++i) {
                const auto& el = doc.element(static_cast<std::uint32_t>(i));
                std::cout << "  [" << el.id << "] nodes=("
                          << el.node_ids[0] << "," << el.node_ids[1] << "," << el.node_ids[2]
                          << ")  mat=" << el.material_id
                          << "  t=" << el.thickness_m << " m\n";
            }
            if (doc.element_count() == 0) std::cout << "  (none)\n";

        } else if (cmd == "solve") {
            std::cout << "Assembling global stiffness matrix (" << doc.node_count()
                      << " nodes, " << doc.element_count() << " elements)...\n";
            lastResult = doc.solve();
            haveSolution = lastResult.converged;
            if (lastResult.converged) {
                std::cout << "Solution converged.\n"
                          << "  Max displacement : " << std::scientific << std::setprecision(4)
                          << lastResult.max_displacement << " m ("
                          << lastResult.max_displacement * 1000.0 << " mm)\n"
                          << "  Max Von Mises    : " << lastResult.max_von_mises / 1e6 << " MPa\n";
                // Quick yield check
                int yielded = 0;
                for (const auto& er : lastResult.element_results)
                    if (er.yielded) ++yielded;
                if (yielded)
                    std::cout << "  WARNING: " << yielded << " element(s) exceeded yield stress!\n";
                else
                    std::cout << "  All elements within elastic range.\n";
            } else {
                std::cout << "ERROR: " << lastResult.message << "\n";
            }

        } else if (cmd == "report") {
            if (!haveSolution) {
                std::cout << "No valid solution — run 'solve' first.\n";
            } else {
                auto report = doc.export_report(lastResult);
                if (tok.size() >= 2) {
                    std::ofstream f(tok[1]);
                    if (f) { f << report; std::cout << "Report written to " << tok[1] << "\n"; }
                    else   { std::cout << "ERROR: cannot write " << tok[1] << "\n"; }
                } else {
                    std::cout << report;
                }
            }

        } else if (cmd == "demo" && tok.size() >= 2) {
            if (tok[1] == "cantilever") {
                loadCantilever(doc);
                haveSolution = false;
                std::cout << "Run 'solve' to compute.\n";
            } else if (tok[1] == "bracket") {
                loadBracket(doc);
                haveSolution = false;
                std::cout << "Run 'solve' to compute.\n";
            } else {
                std::cout << "Unknown demo. Use: cantilever | bracket\n";
            }

        } else {
            std::cout << "Unknown command: " << cmd << "  (type 'help')\n";
        }

        std::cout << "sim> " << std::flush;
    }
    return 0;
}
