// SPDX-License-Identifier: MPL-2.0
// MyDesk Simulation — linear static FEA engine
// Provides: material library, solid/shell mesh, BCs, CST element stiffness
//           assembly, Gaussian elimination solver, stress/displacement output.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace mydesk::simulation {

// ────────────────────────────────────────────────────────────────────────────
// Material
// ────────────────────────────────────────────────────────────────────────────
struct Material {
    std::string name;
    double youngs_modulus_pa{210e9};   ///< Young's modulus (Pa)
    double poissons_ratio{0.3};
    double density_kg_m3{7850.0};
    double yield_strength_pa{250e6};
    double ultimate_strength_pa{400e6};
};

// ────────────────────────────────────────────────────────────────────────────
// Mesh (plane-stress CST triangles — Constant Strain Triangle)
// Each node has 2 DOF: ux, uy
// ────────────────────────────────────────────────────────────────────────────
struct Node2D {
    std::uint32_t id{};
    double x{}, y{};  ///< coordinates (m)
};

struct Element2D {
    std::uint32_t id{};
    std::array<std::uint32_t, 3> node_ids{};  ///< global node IDs (CCW)
    std::uint32_t material_id{};
    double thickness_m{0.01};
};

// ────────────────────────────────────────────────────────────────────────────
// Boundary conditions
// ────────────────────────────────────────────────────────────────────────────
enum class BCKind : std::uint8_t { FixedX, FixedY, FixedXY, PrescribedX, PrescribedY };

struct BoundaryCondition {
    std::uint32_t node_id{};
    BCKind        kind{BCKind::FixedXY};
    double        value{0.0};  ///< prescribed displacement (m), ignored for fixed
};

// ────────────────────────────────────────────────────────────────────────────
// Loads
// ────────────────────────────────────────────────────────────────────────────
struct NodalLoad {
    std::uint32_t node_id{};
    double fx{}, fy{};  ///< force (N)
};

// ────────────────────────────────────────────────────────────────────────────
// Results
// ────────────────────────────────────────────────────────────────────────────
struct NodeResult {
    std::uint32_t node_id{};
    double ux{}, uy{};                 ///< displacement (m)
    double resultant_displacement{};   ///< sqrt(ux^2+uy^2)
};

struct ElementResult {
    std::uint32_t element_id{};
    double sigma_xx{}, sigma_yy{}, tau_xy{};     ///< stress components (Pa)
    double von_mises{};                           ///< Von Mises stress (Pa)
    bool   yielded{false};                        ///< yield criterion exceeded
};

struct SimulationResult {
    bool converged{false};
    std::string message;
    std::vector<NodeResult>    node_results;
    std::vector<ElementResult> element_results;
    double max_displacement{};
    double max_von_mises{};
};

// ────────────────────────────────────────────────────────────────────────────
// Document
// ────────────────────────────────────────────────────────────────────────────
class SimulationDocument {
public:
    SimulationDocument();

    // Materials
    std::uint32_t add_material(Material m);
    Material&     material(std::uint32_t id);
    std::size_t   material_count() const noexcept { return materials_.size(); }

    // Mesh
    std::uint32_t add_node(double x, double y);
    std::uint32_t add_element(const std::array<std::uint32_t,3>& nodes,
                               std::uint32_t material_id,
                               double thickness_m = 0.01);
    std::size_t node_count()    const noexcept { return nodes_.size(); }
    std::size_t element_count() const noexcept { return elements_.size(); }
    const Node2D&    node(std::uint32_t id) const;
    const Element2D& element(std::uint32_t id) const;

    // Boundary conditions and loads
    void add_bc(const BoundaryCondition& bc);
    void add_load(const NodalLoad& load);

    // Solve (linear static, plane-stress, CST elements)
    SimulationResult solve() const;

    // Report export (plain text)
    std::string export_report(const SimulationResult& res) const;

private:
    std::vector<Material>        materials_;
    std::vector<Node2D>          nodes_;
    std::vector<Element2D>       elements_;
    std::vector<BoundaryCondition> bcs_;
    std::vector<NodalLoad>         loads_;

    // Internal helpers
    // Returns 6x6 element stiffness matrix (ke) for a CST element
    std::array<double, 36> element_stiffness(const Element2D& el) const;

    // Assemble global stiffness matrix (dense, for small problems)
    std::vector<double> assemble_K() const;

    // Apply BCs (penalty method)
    void apply_bcs(std::vector<double>& K, std::vector<double>& F) const;

    // Gaussian elimination with partial pivoting
    // Solves K * u = F in-place, returning displacement vector
    static bool solve_linear(std::vector<double>& K, std::vector<double>& F,
                              std::vector<double>& u, int n);
};

} // namespace mydesk::simulation
