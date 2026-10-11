// SPDX-License-Identifier: MPL-2.0
// MyDesk Simulation — regression tests
// Verification against analytical solutions where available.
#include "mydesk/simulation/simulation_document.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

using namespace mydesk::simulation;

static int g_failed = 0;
#define CHECK(cond) \
    do { if (!(cond)) { \
        std::fprintf(stderr, "FAIL [line %d]: %s\n", __LINE__, #cond); \
        ++g_failed; \
    } } while(0)
#define CHECK_NEAR(a, b, eps) \
    do { double _a = (a), _b = (b), _e = (eps); \
         if (std::abs(_a - _b) > _e) { \
             std::fprintf(stderr, "FAIL [line %d]: |%.6g - %.6g| > %.6g\n", \
                          __LINE__, _a, _b, _e); \
             ++g_failed; \
    } } while(0)

// ─── Material library ────────────────────────────────────────────────────────

static void test_material_add() {
    SimulationDocument doc;
    Material m; m.name = "Steel"; m.youngs_modulus_pa = 210e9;
    auto id = doc.add_material(m);
    CHECK(id == 0);
    CHECK(doc.material_count() == 1);
    CHECK(doc.material(0).name == "Steel");
}

// ─── Mesh construction ───────────────────────────────────────────────────────

static void test_mesh_construction() {
    SimulationDocument doc;
    doc.add_material({});
    auto n0 = doc.add_node(0, 0);
    auto n1 = doc.add_node(1, 0);
    auto n2 = doc.add_node(0, 1);
    CHECK(n0 == 0); CHECK(n1 == 1); CHECK(n2 == 2);
    CHECK(doc.node_count() == 3);

    auto e0 = doc.add_element({n0,n1,n2}, 0, 0.01);
    CHECK(e0 == 0);
    CHECK(doc.element_count() == 1);
}

static void test_input_validation() {
    SimulationDocument doc;
    Material invalid;
    invalid.density_kg_m3 = 0.0;
    bool rejected = false;
    try { (void)doc.add_material(invalid); }
    catch (const std::invalid_argument&) { rejected = true; }
    CHECK(rejected);

    doc.add_material({});
    const auto n0 = doc.add_node(0.0, 0.0);
    const auto n1 = doc.add_node(1.0, 0.0);
    const auto n2 = doc.add_node(2.0, 0.0);
    rejected = false;
    try { (void)doc.add_element({n0, n1, n2}, 0, 1.0); }
    catch (const std::invalid_argument&) { rejected = true; }
    CHECK(rejected);
}

// ─── Single CST element — uniaxial tension patch test ────────────────────────
// A single triangle:  n0=(0,0), n1=(1,0), n2=(0,1)
// Fix n0 in XY, n2 in X.  Apply Fx=1000 N at n1.
// Expected: all displacements are positive in x, no negative x displacement for n1.
// This is a qualitative patch test — the node does move in the load direction.

static void test_single_element_uniaxial() {
    SimulationDocument doc;

    Material steel;
    steel.name = "Steel"; steel.youngs_modulus_pa = 210e9;
    steel.poissons_ratio = 0.3; steel.yield_strength_pa = 250e6;
    doc.add_material(steel);

    auto n0 = doc.add_node(0.0, 0.0);
    auto n1 = doc.add_node(1.0, 0.0);
    auto n2 = doc.add_node(0.0, 1.0);
    doc.add_element({n0, n1, n2}, 0, 0.01);

    doc.add_bc({n0, BCKind::FixedXY});
    doc.add_bc({n2, BCKind::FixedX});

    doc.add_load({n1, 1000.0, 0.0});

    auto res = doc.solve();
    CHECK(res.converged);
    // n1 (loaded node) must displace in +X
    const NodeResult& nr1 = res.node_results[n1];
    CHECK(nr1.ux > 0.0);
    // n0 (fully fixed) must have near-zero displacement
    CHECK_NEAR(res.node_results[n0].ux, 0.0, 1e-10);
    CHECK_NEAR(res.node_results[n0].uy, 0.0, 1e-10);
}

// ─── Two-element bar — symmetry ──────────────────────────────────────────────
// Two CST triangles forming a unit square under pure tension.
// All left-edge nodes fixed in X; all right-edge nodes get Fx load.
// Right nodes should have positive X displacement; left nodes ~zero.
// With E=1, nu=0, t=1: analytical u_x at loaded end ~ F / (E * A / L) = F/1 = F.

static void test_two_element_bar() {
    SimulationDocument doc;

    Material mat;
    mat.name = "Test"; mat.youngs_modulus_pa = 1.0;
    mat.poissons_ratio = 0.0; mat.yield_strength_pa = 1e30;
    doc.add_material(mat);

    // Square: (0,0) (1,0) (1,1) (0,1)
    auto n0 = doc.add_node(0.0, 0.0); // bottom-left
    auto n1 = doc.add_node(1.0, 0.0); // bottom-right
    auto n2 = doc.add_node(1.0, 1.0); // top-right
    auto n3 = doc.add_node(0.0, 1.0); // top-left

    // Triangle 0: n0, n1, n2 (CCW)
    doc.add_element({n0, n1, n2}, 0, 1.0);
    // Triangle 1: n0, n2, n3 (CCW)
    doc.add_element({n0, n2, n3}, 0, 1.0);

    // Fix left edge in X (n0, n3)
    doc.add_bc({n0, BCKind::FixedXY});
    doc.add_bc({n3, BCKind::FixedX});

    // Apply load at right edge
    doc.add_load({n1, 0.5, 0.0});
    doc.add_load({n2, 0.5, 0.0});

    auto res = doc.solve();
    CHECK(res.converged);

    // Both right-edge nodes must displace in +X
    CHECK(res.node_results[n1].ux > 0.0);
    CHECK(res.node_results[n2].ux > 0.0);
    // Max displacement should be positive
    CHECK(res.max_displacement > 0.0);
}

// ─── Von Mises stress recovery ───────────────────────────────────────────────

static void test_von_mises_computed() {
    SimulationDocument doc;

    Material mat;
    mat.name = "Aluminium"; mat.youngs_modulus_pa = 70e9;
    mat.poissons_ratio = 0.33; mat.yield_strength_pa = 270e6;
    doc.add_material(mat);

    auto n0 = doc.add_node(0.0, 0.0);
    auto n1 = doc.add_node(0.01, 0.0);
    auto n2 = doc.add_node(0.0, 0.01);
    doc.add_element({n0, n1, n2}, 0, 0.001);

    doc.add_bc({n0, BCKind::FixedXY});
    doc.add_bc({n2, BCKind::FixedX});
    doc.add_load({n1, 10000.0, 0.0});  // large enough to get measurable stress

    auto res = doc.solve();
    CHECK(res.converged);
    CHECK(res.element_results.size() == 1);
    CHECK(res.element_results[0].von_mises > 0.0);
}

// ─── Yield detection ─────────────────────────────────────────────────────────

static void test_yield_detection() {
    SimulationDocument doc;

    // Very soft, very low yield material so small load causes yield
    Material mat;
    mat.name = "SoftMat"; mat.youngs_modulus_pa = 1e6;
    mat.poissons_ratio = 0.0; mat.yield_strength_pa = 1.0;  // 1 Pa yield!
    doc.add_material(mat);

    auto n0 = doc.add_node(0.0, 0.0);
    auto n1 = doc.add_node(1.0, 0.0);
    auto n2 = doc.add_node(0.0, 1.0);
    doc.add_element({n0, n1, n2}, 0, 1.0);

    doc.add_bc({n0, BCKind::FixedXY});
    doc.add_bc({n2, BCKind::FixedX});
    doc.add_load({n1, 1e6, 0.0});  // huge force => vm >> 1 Pa yield

    auto res = doc.solve();
    CHECK(res.converged);
    CHECK(res.element_results[0].yielded);
}

// ─── Report export ───────────────────────────────────────────────────────────

static void test_report_export() {
    SimulationDocument doc;
    Material m; m.name = "Steel"; m.youngs_modulus_pa = 210e9;
    m.yield_strength_pa = 250e6;
    doc.add_material(m);

    auto n0 = doc.add_node(0,0), n1 = doc.add_node(1,0), n2 = doc.add_node(0,1);
    doc.add_element({n0,n1,n2}, 0, 0.01);
    doc.add_bc({n0, BCKind::FixedXY}); doc.add_bc({n2, BCKind::FixedX});
    doc.add_load({n1, 1000.0, 0.0});

    auto res = doc.solve();
    auto report = doc.export_report(res);
    CHECK(report.find("CONVERGED") != std::string::npos);
    CHECK(report.find("Node Displacements") != std::string::npos);
    CHECK(report.find("Element Stresses") != std::string::npos);
}

// ─── Edge cases ──────────────────────────────────────────────────────────────

static void test_no_elements() {
    SimulationDocument doc;
    doc.add_node(0,0);
    auto res = doc.solve();
    CHECK(!res.converged);
}

// ─── Modal analysis — closed-form two-DOF CST benchmark ─────────────────────
// With nu=0, a unit right triangle fixed at nodes 0 and 2 leaves node 1's two
// translations free. K={E/2,E/4}, M={rho/12,rho/12}, so omega^2={6E/rho,3E/rho}.

static void test_modal_cst_analytical_frequencies() {
    SimulationDocument doc;
    Material mat;
    mat.name = "Modal benchmark";
    mat.youngs_modulus_pa = 1200.0;
    mat.poissons_ratio = 0.0;
    mat.density_kg_m3 = 3.0;
    mat.yield_strength_pa = 1e30;
    mat.ultimate_strength_pa = 1e30;
    doc.add_material(mat);
    const auto n0 = doc.add_node(0.0, 0.0);
    const auto n1 = doc.add_node(1.0, 0.0);
    const auto n2 = doc.add_node(0.0, 1.0);
    doc.add_element({n0, n1, n2}, 0, 1.0);
    doc.add_bc({n0, BCKind::FixedXY});
    doc.add_bc({n2, BCKind::FixedXY});

    const auto result = doc.solve_modal(2);
    CHECK(result.converged);
    CHECK(result.modes.size() == 2);
    CHECK_NEAR(result.modes[0].eigenvalue_rad2_s2, 1200.0, 1e-8);
    CHECK_NEAR(result.modes[1].eigenvalue_rad2_s2, 2400.0, 1e-8);
    CHECK_NEAR(result.modes[0].frequency_hz, std::sqrt(1200.0)/(2.0*3.14159265358979323846), 1e-10);
    CHECK(result.modes[0].relative_residual < 1e-10);
    CHECK(result.modes[1].relative_residual < 1e-10);
    CHECK_NEAR(result.modes[0].shape[n0].resultant_displacement, 0.0, 1e-12);

    const auto report = doc.export_modal_report(result);
    CHECK(report.find("Undamped Modal Analysis") != std::string::npos);
    CHECK(report.find("Frequency(Hz)") != std::string::npos);
}

static void test_transaction_undo_redo() {
    SimulationDocument doc;
    doc.begin_transaction("add benchmark node");
    doc.add_node(0.0, 0.0);
    doc.commit_transaction();
    CHECK(doc.node_count() == 1);
    CHECK(doc.undo());
    CHECK(doc.node_count() == 0);
    CHECK(doc.last_history_label() == "add benchmark node");
    CHECK(doc.redo());
    CHECK(doc.node_count() == 1);

    doc.begin_transaction("rolled back node");
    doc.add_node(1.0, 0.0);
    doc.rollback_transaction();
    CHECK(doc.node_count() == 1);
}

static void test_native_persistence_roundtrip_and_validation() {
    SimulationDocument doc;
    Material mat;
    mat.name = "Steel \"modal\"";
    const auto material_id = doc.add_material(mat);
    const auto n0 = doc.add_node(0.0, 0.0);
    const auto n1 = doc.add_node(1.0, 0.0);
    const auto n2 = doc.add_node(0.0, 1.0);
    doc.add_element({n0, n1, n2}, material_id, 0.02);
    doc.add_bc({n0, BCKind::FixedXY});
    doc.add_load({n1, 25.0, -5.0});

    const auto encoded = doc.serialize_native();
    CHECK(encoded.find("\"schema_version\": 2") != std::string::npos);
    const auto restored = SimulationDocument::deserialize_native(encoded);
    CHECK(restored.material_count() == 1);
    CHECK(restored.material(0).name == mat.name);
    CHECK(restored.node_count() == 3);
    CHECK(restored.element_count() == 1);
    CHECK(restored.boundary_condition_count() == 1);
    CHECK(restored.load_count() == 1);

    const auto unique = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
        ("mydesk-simulation-persistence-" + std::to_string(unique) + ".mdsim");
    const auto backup = std::filesystem::path(path.string() + ".bak");
    std::error_code ec;
    std::filesystem::remove(path, ec);
    std::filesystem::remove(backup, ec);
    doc.save_native_file(path);
    doc.save_native_file(path);
    CHECK(std::filesystem::exists(path));
    CHECK(std::filesystem::exists(backup));
    CHECK(SimulationDocument::load_native_file(path).element_count() == 1);
    std::filesystem::remove(path, ec);
    std::filesystem::remove(backup, ec);

    bool rejected = false;
    try {
        (void)SimulationDocument::deserialize_native("{\"format\":\"mydesk-simulation\",\"schema_version\":99}");
    } catch (const std::invalid_argument&) { rejected = true; }
    CHECK(rejected);
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main() {
    // "test_material_add"
    test_material_add();
    // "test_mesh_construction"
    test_mesh_construction();
    // "test_input_validation"
    test_input_validation();
    // "test_single_element_uniaxial"
    test_single_element_uniaxial();
    // "test_two_element_bar"
    test_two_element_bar();
    // "test_von_mises_computed"
    test_von_mises_computed();
    // "test_yield_detection"
    test_yield_detection();
    // "test_report_export"
    test_report_export();
    // "test_no_elements"
    test_no_elements();
    // "test_modal_cst_analytical_frequencies"
    test_modal_cst_analytical_frequencies();
    // "test_transaction_undo_redo"
    test_transaction_undo_redo();
    // "test_native_persistence_roundtrip_and_validation"
    test_native_persistence_roundtrip_and_validation();

    if (g_failed == 0) {
        std::printf("All Simulation tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d Simulation test(s) FAILED.\n", g_failed);
    return 1;
}
