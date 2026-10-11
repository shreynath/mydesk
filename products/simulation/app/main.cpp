// SPDX-License-Identifier: MPL-2.0
// MyDesk Simulation v0.2.0 — structural FEA application
// Interactive command-line interface for static and modal analysis.
#include "mydesk/simulation/simulation_document.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace mydesk::simulation;

static void printBanner() {
    std::cout << R"(
╔══════════════════════════════════════════════════════╗
║      MyDesk Simulation  v0.2.0 — FEA System          ║
║  Open-source professional structural analysis tool   ║
╚══════════════════════════════════════════════════════╝
Solver: 2D Plane-Stress CST (Constant Strain Triangle)
        Gaussian elimination with partial pivoting
        Von Mises yield criterion
        Undamped modal analysis with consistent mass
)" << '\n';
}

static void printHelp() {
    std::cout <<
        "Commands:\n"
        "  mat <name> <E_GPa> <nu> <yield_MPa> [density_kg_m3]  Add material\n"
        "  node <x_m> <y_m>                       Add node (metres)\n"
        "  elem <n0> <n1> <n2> <mat_id> [thick_m] Add CST element\n"
        "  fix <node_id> <dof>     Fix DOF: x | y | xy\n"
        "  load <node_id> <Fx_N> <Fy_N>          Apply nodal force\n"
        "  nodes                   List nodes\n"
        "  elements                List elements\n"
        "  solve                   Run linear static solve\n"
        "  modal [mode_count]      Solve undamped natural frequencies\n"
        "  report [output.txt]     Print analysis report\n"
        "  modalreport [file.txt]  Print last modal analysis report\n"
        "  save <project.mdsim>    Atomically save native schema-v2 project\n"
        "  open <project.mdsim>    Open and validate native project\n"
        "  new                     Clear the current project\n"
        "  undo | redo             Undo or redo the last model command\n"
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

template <typename Action>
static void transact(SimulationDocument& doc, const std::string& label, Action&& action) {
    doc.begin_transaction(label);
    try {
        action();
        doc.commit_transaction();
    } catch (...) {
        doc.rollback_transaction();
        throw;
    }
}

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
    ModalResult lastModalResult;
    bool haveSolution = false;
    bool haveModalSolution = false;

    std::cout << "sim> " << std::flush;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) { std::cout << "sim> " << std::flush; continue; }
        auto tok = tokenize(line);
        if (tok.empty()) { std::cout << "sim> " << std::flush; continue; }

        const std::string& cmd = tok[0];

        try {
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
            if (tok.size() >= 6) m.density_kg_m3 = toDouble(tok[5]);
            std::uint32_t id{};
            transact(doc, "add material", [&] { id = doc.add_material(m); });
            std::cout << "Material [" << id << "] " << m.name
                      << "  E=" << toDouble(tok[2]) << " GPa"
                      << "  nu=" << m.poissons_ratio
                      << "  yield=" << toDouble(tok[4]) << " MPa\n";
            haveSolution = false;
            haveModalSolution = false;

        } else if (cmd == "node" && tok.size() >= 3) {
            std::uint32_t id{};
            transact(doc, "add node", [&] { id = doc.add_node(toDouble(tok[1]), toDouble(tok[2])); });
            std::cout << "Node [" << id << "] at ("
                      << toDouble(tok[1]) << ", " << toDouble(tok[2]) << ") m\n";
            haveSolution = false;
            haveModalSolution = false;

        } else if (cmd == "elem" && tok.size() >= 5) {
            auto n0 = toU32(tok[1]), n1 = toU32(tok[2]), n2 = toU32(tok[3]);
            auto mat = toU32(tok[4]);
            double thick = (tok.size() >= 6) ? toDouble(tok[5]) : 0.01;
            std::uint32_t id{};
            transact(doc, "add element", [&] { id = doc.add_element({n0, n1, n2}, mat, thick); });
            std::cout << "Element [" << id << "] nodes=(" << n0 << "," << n1 << "," << n2
                      << ")  mat=" << mat << "  t=" << thick << " m\n";
            haveSolution = false;
            haveModalSolution = false;

        } else if (cmd == "fix" && tok.size() >= 3) {
            auto nid = toU32(tok[1]);
            BCKind kind = BCKind::FixedXY;
            if      (tok[2] == "x")  kind = BCKind::FixedX;
            else if (tok[2] == "y")  kind = BCKind::FixedY;
            else if (tok[2] == "xy") kind = BCKind::FixedXY;
            else throw std::invalid_argument("fix DOF must be x, y, or xy");
            transact(doc, "add boundary condition", [&] { doc.add_bc({nid, kind, 0.0}); });
            std::cout << "Fixed node " << nid << " DOF " << tok[2] << "\n";
            haveSolution = false;
            haveModalSolution = false;

        } else if (cmd == "load" && tok.size() >= 4) {
            auto nid = toU32(tok[1]);
            transact(doc, "add nodal load", [&] { doc.add_load({nid, toDouble(tok[2]), toDouble(tok[3])}); });
            std::cout << "Load on node " << nid
                      << ": Fx=" << toDouble(tok[2]) << " N"
                      << "  Fy=" << toDouble(tok[3]) << " N\n";
            haveSolution = false;
            haveModalSolution = false;

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

        } else if (cmd == "modal") {
            const std::size_t count = tok.size() >= 2 ? static_cast<std::size_t>(std::stoul(tok[1])) : 6;
            std::cout << "Assembling stiffness and consistent mass matrices...\n";
            lastModalResult = doc.solve_modal(count);
            haveModalSolution = lastModalResult.converged;
            if (lastModalResult.converged) {
                std::cout << "Modal solution converged with " << lastModalResult.modes.size() << " flexible mode(s).\n";
                for (const auto& mode : lastModalResult.modes)
                    std::cout << "  Mode " << mode.mode_number << ": " << std::fixed << std::setprecision(6)
                              << mode.frequency_hz << " Hz  residual=" << std::scientific
                              << mode.relative_residual << "\n";
            } else {
                std::cout << "ERROR: " << lastModalResult.message << "\n";
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

        } else if (cmd == "modalreport") {
            if (!haveModalSolution) {
                std::cout << "No valid modal solution — run 'modal' first.\n";
            } else {
                const auto report = doc.export_modal_report(lastModalResult);
                if (tok.size() >= 2) {
                    std::ofstream file(tok[1]);
                    if (file) { file << report; std::cout << "Modal report written to " << tok[1] << "\n"; }
                    else { std::cout << "ERROR: cannot write " << tok[1] << "\n"; }
                } else {
                    std::cout << report;
                }
            }

        } else if (cmd == "save" && tok.size() >= 2) {
            doc.save_native_file(tok[1]);
            std::cout << "Saved validated simulation project to " << tok[1] << "\n";

        } else if (cmd == "open" && tok.size() >= 2) {
            doc = SimulationDocument::load_native_file(tok[1]);
            haveSolution = false;
            haveModalSolution = false;
            std::cout << "Opened " << tok[1] << ": " << doc.node_count() << " nodes, "
                      << doc.element_count() << " elements.\n";

        } else if (cmd == "new") {
            doc = SimulationDocument{};
            haveSolution = false;
            haveModalSolution = false;
            std::cout << "Started a new simulation project.\n";

        } else if (cmd == "undo" || cmd == "redo") {
            const bool changed = cmd == "undo" ? doc.undo() : doc.redo();
            if (changed) {
                haveSolution = false;
                haveModalSolution = false;
                std::cout << (cmd == "undo" ? "Undid: " : "Redid: ") << doc.last_history_label() << "\n";
            } else {
                std::cout << "Nothing to " << cmd << ".\n";
            }

        } else if (cmd == "demo" && tok.size() >= 2) {
            if (tok[1] == "cantilever") {
                loadCantilever(doc);
                haveSolution = false;
                haveModalSolution = false;
                std::cout << "Run 'solve' to compute.\n";
            } else if (tok[1] == "bracket") {
                loadBracket(doc);
                haveSolution = false;
                haveModalSolution = false;
                std::cout << "Run 'solve' to compute.\n";
            } else {
                std::cout << "Unknown demo. Use: cantilever | bracket\n";
            }

        } else {
            std::cout << "Unknown command: " << cmd << "  (type 'help')\n";
        }
        } catch (const std::exception& error) {
            std::cout << "ERROR: " << error.what() << "\n";
        }

        std::cout << "sim> " << std::flush;
    }
    return 0;
}
