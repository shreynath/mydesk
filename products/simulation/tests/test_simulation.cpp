// SPDX-License-Identifier: MPL-2.0
// MyDesk Simulation — regression tests
// Verification against analytical solutions where available.
#include "mydesk/simulation/simulation_document.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
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

// ─── Main ────────────────────────────────────────────────────────────────────

int main() {
    test_material_add();
    test_mesh_construction();
    test_single_element_uniaxial();
    test_two_element_bar();
    test_von_mises_computed();
    test_yield_detection();
    test_report_export();
    test_no_elements();

    if (g_failed == 0) {
        std::printf("All Simulation tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d Simulation test(s) FAILED.\n", g_failed);
    return 1;
}
