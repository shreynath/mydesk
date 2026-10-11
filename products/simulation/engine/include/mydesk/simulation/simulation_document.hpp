// SPDX-License-Identifier: MPL-2.0
// MyDesk Simulation — linear static FEA engine
// Provides: material library, solid/shell mesh, BCs, CST element stiffness
//           assembly, Gaussian elimination solver, stress/displacement output.
#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

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

// A real-valued undamped normal mode.  The shape is mass-normalized, so
// phi^T M phi = 1.  Rigid-body modes are not returned.
struct ModalMode {
    std::size_t mode_number{};
    double eigenvalue_rad2_s2{};
    double frequency_hz{};
    std::vector<NodeResult> shape;
    double relative_residual{}; ///< ||K phi - lambda M phi|| / max(||K phi||, eps)
};

struct ModalResult {
    bool converged{false};
    std::string message;
    std::vector<ModalMode> modes;
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
    const Material& material(std::uint32_t id) const;
    std::size_t   material_count() const noexcept { return materials_.size(); }
    void set_material(std::uint32_t id, Material material);

    // Mesh
    std::uint32_t add_node(double x, double y);
    std::uint32_t add_element(const std::array<std::uint32_t,3>& nodes,
                               std::uint32_t material_id,
                               double thickness_m = 0.01);
    std::size_t node_count()    const noexcept { return nodes_.size(); }
    std::size_t element_count() const noexcept { return elements_.size(); }
    const Node2D&    node(std::uint32_t id) const;
    const Element2D& element(std::uint32_t id) const;
    void set_node_coordinates(std::uint32_t id, double x, double y);

    // Boundary conditions and loads
    void add_bc(const BoundaryCondition& bc);
    void add_load(const NodalLoad& load);
    std::size_t boundary_condition_count() const noexcept { return bcs_.size(); }
    std::size_t load_count() const noexcept { return loads_.size(); }
    const BoundaryCondition& boundary_condition(std::size_t index) const { return bcs_.at(index); }
    const NodalLoad& load(std::size_t index) const { return loads_.at(index); }

    // Solve (linear static, plane-stress, CST elements)
    SimulationResult solve() const;

    // Undamped free-vibration analysis, K phi = omega^2 M phi.  Dirichlet
    // constraints are eliminated exactly and the CST consistent mass matrix
    // includes the material density and element thickness.
    ModalResult solve_modal(std::size_t requested_modes = 6) const;

    // Report export (plain text)
    std::string export_report(const SimulationResult& res) const;
    std::string export_modal_report(const ModalResult& res) const;

    // Command transaction boundary used by interactive clients.  A transaction
    // captures all model inputs; results are intentionally derived, not stored.
    void begin_transaction(std::string label);
    void commit_transaction();
    void rollback_transaction();
    bool undo();
    bool redo();
    bool can_undo() const noexcept { return !undo_stack_.empty(); }
    bool can_redo() const noexcept { return !redo_stack_.empty(); }
    bool transaction_active() const noexcept { return transaction_active_; }
    const std::string& last_history_label() const noexcept { return last_history_label_; }

    // Native schema-v2 project persistence.
    std::string serialize_native() const;
    static SimulationDocument deserialize_native(const std::string& text);
    void save_native_file(const std::filesystem::path& path) const;
    static SimulationDocument load_native_file(const std::filesystem::path& path);

private:
    std::vector<Material>        materials_;
    std::vector<Node2D>          nodes_;
    std::vector<Element2D>       elements_;
    std::vector<BoundaryCondition> bcs_;
    std::vector<NodalLoad>         loads_;

    struct State {
        std::vector<Material> materials;
        std::vector<Node2D> nodes;
        std::vector<Element2D> elements;
        std::vector<BoundaryCondition> bcs;
        std::vector<NodalLoad> loads;
        std::string label;
    };
    std::vector<State> undo_stack_;
    std::vector<State> redo_stack_;
    std::optional<State> transaction_before_;
    bool transaction_active_{false};
    std::string transaction_label_;
    std::string last_history_label_;

    State capture_state(std::string label = {}) const;
    void restore_state(const State& state);

    // Internal helpers
    // Returns 6x6 element stiffness matrix (ke) for a CST element
    std::array<double, 36> element_stiffness(const Element2D& el) const;

    // Assemble global stiffness matrix (dense, for small problems)
    std::vector<double> assemble_K() const;
    std::vector<double> assemble_M() const;

    // Apply BCs (penalty method)
    void apply_bcs(std::vector<double>& K, std::vector<double>& F) const;

    // Gaussian elimination with partial pivoting
    // Solves K * u = F in-place, returning displacement vector
    static bool solve_linear(std::vector<double>& K, std::vector<double>& F,
                              std::vector<double>& u, int n);
};

} // namespace mydesk::simulation
