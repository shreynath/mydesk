// SPDX-License-Identifier: MPL-2.0
// MyDesk Simulation — linear static FEA engine implementation
// Reference: Zienkiewicz & Taylor "The Finite Element Method", vol. 1
//            Logan "A First Course in the Finite Element Method"
#include "mydesk/simulation/simulation_document.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include <nlohmann/json.hpp>

namespace mydesk::simulation {

namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;
using Json = nlohmann::json;

bool finite(double value) { return std::isfinite(value); }

const char* bc_kind_name(BCKind kind) {
    switch (kind) {
        case BCKind::FixedX: return "fixed_x";
        case BCKind::FixedY: return "fixed_y";
        case BCKind::FixedXY: return "fixed_xy";
        case BCKind::PrescribedX: return "prescribed_x";
        case BCKind::PrescribedY: return "prescribed_y";
    }
    throw std::invalid_argument("Unknown boundary-condition kind");
}

BCKind parse_bc_kind(const std::string& value) {
    if (value == "fixed_x") return BCKind::FixedX;
    if (value == "fixed_y") return BCKind::FixedY;
    if (value == "fixed_xy") return BCKind::FixedXY;
    if (value == "prescribed_x") return BCKind::PrescribedX;
    if (value == "prescribed_y") return BCKind::PrescribedY;
    throw std::invalid_argument("Unknown boundary-condition kind: " + value);
}

// Cyclic Jacobi diagonalization for a dense real symmetric matrix.
bool jacobi_eigen(std::vector<double> a, int n, std::vector<double>& values,
                  std::vector<double>& vectors) {
    vectors.assign(static_cast<std::size_t>(n * n), 0.0);
    for (int i = 0; i < n; ++i) vectors[static_cast<std::size_t>(i * n + i)] = 1.0;
    const int max_sweeps = std::max(40, n * n * 20);
    for (int sweep = 0; sweep < max_sweeps; ++sweep) {
        int p = 0, q = 0;
        double largest = 0.0;
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                const double v = std::abs(a[static_cast<std::size_t>(i * n + j)]);
                if (v > largest) { largest = v; p = i; q = j; }
            }
        }
        double diagonal_scale = 1.0;
        for (int i = 0; i < n; ++i)
            diagonal_scale = std::max(diagonal_scale, std::abs(a[static_cast<std::size_t>(i * n + i)]));
        if (largest <= diagonal_scale * 1e-12) {
            values.resize(static_cast<std::size_t>(n));
            for (int i = 0; i < n; ++i) values[static_cast<std::size_t>(i)] = a[static_cast<std::size_t>(i * n + i)];
            return true;
        }
        const double app = a[static_cast<std::size_t>(p * n + p)];
        const double aqq = a[static_cast<std::size_t>(q * n + q)];
        const double apq = a[static_cast<std::size_t>(p * n + q)];
        const double angle = 0.5 * std::atan2(2.0 * apq, aqq - app);
        const double c = std::cos(angle), s = std::sin(angle);
        for (int k = 0; k < n; ++k) {
            if (k == p || k == q) continue;
            const double akp = a[static_cast<std::size_t>(k * n + p)];
            const double akq = a[static_cast<std::size_t>(k * n + q)];
            a[static_cast<std::size_t>(k * n + p)] = a[static_cast<std::size_t>(p * n + k)] = c * akp - s * akq;
            a[static_cast<std::size_t>(k * n + q)] = a[static_cast<std::size_t>(q * n + k)] = s * akp + c * akq;
        }
        a[static_cast<std::size_t>(p * n + p)] = c*c*app - 2.0*s*c*apq + s*s*aqq;
        a[static_cast<std::size_t>(q * n + q)] = s*s*app + 2.0*s*c*apq + c*c*aqq;
        a[static_cast<std::size_t>(p * n + q)] = a[static_cast<std::size_t>(q * n + p)] = 0.0;
        for (int k = 0; k < n; ++k) {
            const double vkp = vectors[static_cast<std::size_t>(k * n + p)];
            const double vkq = vectors[static_cast<std::size_t>(k * n + q)];
            vectors[static_cast<std::size_t>(k * n + p)] = c * vkp - s * vkq;
            vectors[static_cast<std::size_t>(k * n + q)] = s * vkp + c * vkq;
        }
    }
    return false;
}
} // namespace

// ─── Document setup ──────────────────────────────────────────────────────────

SimulationDocument::SimulationDocument() = default;

std::uint32_t SimulationDocument::add_material(Material m) {
    if (!finite(m.youngs_modulus_pa) || m.youngs_modulus_pa <= 0.0 ||
        !finite(m.poissons_ratio) || m.poissons_ratio <= -1.0 || m.poissons_ratio >= 0.5 ||
        !finite(m.density_kg_m3) || m.density_kg_m3 <= 0.0 ||
        !finite(m.yield_strength_pa) || m.yield_strength_pa <= 0.0 ||
        !finite(m.ultimate_strength_pa) || m.ultimate_strength_pa <= 0.0) {
        throw std::invalid_argument("Material properties must be finite and physically valid");
    }
    auto id = static_cast<std::uint32_t>(materials_.size());
    materials_.push_back(std::move(m));
    return id;
}

Material& SimulationDocument::material(std::uint32_t id) {
    return materials_.at(id);
}

const Material& SimulationDocument::material(std::uint32_t id) const {
    return materials_.at(id);
}

std::uint32_t SimulationDocument::add_node(double x, double y) {
    if (!finite(x) || !finite(y)) throw std::invalid_argument("Node coordinates must be finite");
    auto id = static_cast<std::uint32_t>(nodes_.size());
    nodes_.push_back({id, x, y});
    return id;
}

std::uint32_t SimulationDocument::add_element(const std::array<std::uint32_t,3>& nids,
                                               std::uint32_t mat_id, double thickness_m) {
    if (mat_id >= materials_.size()) throw std::out_of_range("Element material does not exist");
    if (!finite(thickness_m) || thickness_m <= 0.0) throw std::invalid_argument("Element thickness must be positive");
    for (auto id : nids) if (id >= nodes_.size()) throw std::out_of_range("Element node does not exist");
    if (nids[0] == nids[1] || nids[1] == nids[2] || nids[2] == nids[0])
        throw std::invalid_argument("Element node IDs must be distinct");
    const auto& a = nodes_[nids[0]]; const auto& b = nodes_[nids[1]]; const auto& c = nodes_[nids[2]];
    if (std::abs((b.x-a.x)*(c.y-a.y) - (c.x-a.x)*(b.y-a.y)) <= 1e-15)
        throw std::invalid_argument("Element area must be nonzero");
    auto id = static_cast<std::uint32_t>(elements_.size());
    elements_.push_back({id, nids, mat_id, thickness_m});
    return id;
}

const Node2D& SimulationDocument::node(std::uint32_t id) const {
    return nodes_.at(id);
}

const Element2D& SimulationDocument::element(std::uint32_t id) const {
    return elements_.at(id);
}

void SimulationDocument::add_bc(const BoundaryCondition& bc) {
    if (bc.node_id >= nodes_.size()) throw std::out_of_range("Boundary-condition node does not exist");
    if (!finite(bc.value)) throw std::invalid_argument("Boundary-condition value must be finite");
    bcs_.push_back(bc);
}

void SimulationDocument::add_load(const NodalLoad& load) {
    if (load.node_id >= nodes_.size()) throw std::out_of_range("Load node does not exist");
    if (!finite(load.fx) || !finite(load.fy)) throw std::invalid_argument("Load components must be finite");
    loads_.push_back(load);
}

// ─── CST element stiffness matrix ────────────────────────────────────────────
// 2D plane-stress CST (Constant Strain Triangle), 3 nodes, 2 DOF/node = 6 DOF
// Returns row-major 6x6 stiffness matrix.

std::array<double, 36> SimulationDocument::element_stiffness(const Element2D& el) const {
    const Node2D& n0 = nodes_.at(el.node_ids[0]);
    const Node2D& n1 = nodes_.at(el.node_ids[1]);
    const Node2D& n2 = nodes_.at(el.node_ids[2]);
    const Material& mat = materials_.at(el.material_id);

    double x0 = n0.x, y0 = n0.y;
    double x1 = n1.x, y1 = n1.y;
    double x2 = n2.x, y2 = n2.y;

    // Area via cross product (signed — ensure CCW)
    double A2 = (x1-x0)*(y2-y0) - (x2-x0)*(y1-y0); // 2A
    // If CW, negate (we want positive area)
    if (A2 < 0) A2 = -A2;
    double A = A2 * 0.5;

    // Shape function derivatives (constant for CST)
    // N_i = (a_i + b_i*x + c_i*y) / 2A
    double b0 = y1-y2, b1 = y2-y0, b2 = y0-y1;
    double c0 = x2-x1, c1 = x0-x2, c2 = x1-x0;

    // Strain-displacement matrix B (3x6)
    // eps = [e_xx, e_yy, gamma_xy]^T = B * u
    // B = 1/(2A) * [[b0 0 b1 0 b2 0],
    //               [0 c0  0 c1  0 c2],
    //               [c0 b0 c1 b1 c2 b2]]
    double inv2A = 1.0 / (A2);
    double B[3][6] = {
        {inv2A*b0, 0,        inv2A*b1, 0,        inv2A*b2, 0},
        {0,        inv2A*c0, 0,        inv2A*c1, 0,        inv2A*c2},
        {inv2A*c0, inv2A*b0, inv2A*c1, inv2A*b1, inv2A*c2, inv2A*b2}
    };

    // Plane-stress constitutive matrix D (3x3)
    double E = mat.youngs_modulus_pa;
    double nu = mat.poissons_ratio;
    double fac = E / (1.0 - nu*nu);
    double D[3][3] = {
        {fac,      fac*nu,  0},
        {fac*nu,   fac,     0},
        {0,        0,       fac*(1.0-nu)*0.5}
    };

    // Ke = t * A * B^T * D * B  (6x6)
    double t = el.thickness_m;

    // Compute D*B first (3x6)
    double DB[3][6]{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 6; ++j)
            for (int k = 0; k < 3; ++k)
                DB[i][j] += D[i][k] * B[k][j];

    // Ke = t*A * B^T * DB (6x6)
    std::array<double, 36> Ke{};
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 6; ++j)
            for (int k = 0; k < 3; ++k)
                Ke[static_cast<std::size_t>(i*6+j)] += t * A * B[k][i] * DB[k][j];

    return Ke;
}

// ─── Assemble global K ───────────────────────────────────────────────────────

std::vector<double> SimulationDocument::assemble_K() const {
    int n = static_cast<int>(nodes_.size()) * 2;  // total DOF
    std::vector<double> K(static_cast<std::size_t>(n*n), 0.0);

    for (const Element2D& el : elements_) {
        auto Ke = element_stiffness(el);
        // Global DOF indices for element nodes (2 DOF/node: 2i, 2i+1)
        std::array<int,6> dofs{};
        for (int ln = 0; ln < 3; ++ln) {
            dofs[static_cast<std::size_t>(2*ln)] =
                static_cast<int>(el.node_ids[static_cast<std::size_t>(ln)]) * 2;
            dofs[static_cast<std::size_t>(2*ln+1)] =
                static_cast<int>(el.node_ids[static_cast<std::size_t>(ln)]) * 2 + 1;
        }
        // Scatter into global K
        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < 6; ++j)
                K[static_cast<std::size_t>(dofs[static_cast<std::size_t>(i)]*n +
                                           dofs[static_cast<std::size_t>(j)])] +=
                    Ke[static_cast<std::size_t>(i*6+j)];
    }
    return K;
}

// Consistent translational mass matrix for a three-node triangle:
// rho*t*A/12 * [[2I,I,I],[I,2I,I],[I,I,2I]].
std::vector<double> SimulationDocument::assemble_M() const {
    const int n = static_cast<int>(nodes_.size()) * 2;
    std::vector<double> M(static_cast<std::size_t>(n*n), 0.0);
    for (const Element2D& el : elements_) {
        const auto& a = nodes_.at(el.node_ids[0]);
        const auto& b = nodes_.at(el.node_ids[1]);
        const auto& c = nodes_.at(el.node_ids[2]);
        const double area = 0.5 * std::abs((b.x-a.x)*(c.y-a.y) - (c.x-a.x)*(b.y-a.y));
        const double scale = materials_.at(el.material_id).density_kg_m3 * el.thickness_m * area / 12.0;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                const double coefficient = scale * (i == j ? 2.0 : 1.0);
                for (int component = 0; component < 2; ++component) {
                    const int row = static_cast<int>(el.node_ids[static_cast<std::size_t>(i)]) * 2 + component;
                    const int col = static_cast<int>(el.node_ids[static_cast<std::size_t>(j)]) * 2 + component;
                    M[static_cast<std::size_t>(row*n + col)] += coefficient;
                }
            }
        }
    }
    return M;
}

// ─── Apply boundary conditions (penalty method) ──────────────────────────────

void SimulationDocument::apply_bcs(std::vector<double>& K,
                                   std::vector<double>& F) const {
    int n = static_cast<int>(nodes_.size()) * 2;
    // Find a "large" penalty value relative to maximum diagonal
    double max_diag = 0.0;
    for (int i = 0; i < n; ++i) max_diag = std::max(max_diag, K[static_cast<std::size_t>(i*n+i)]);
    double penalty = max_diag * 1e14;
    if (penalty == 0.0) penalty = 1e20;

    for (const BoundaryCondition& bc : bcs_) {
        int base = static_cast<int>(bc.node_id) * 2;
        auto enforce = [&](int dof, double val){
            K[static_cast<std::size_t>(dof*n+dof)] += penalty;
            F[static_cast<std::size_t>(dof)] += penalty * val;
        };
        switch (bc.kind) {
            case BCKind::FixedX:        enforce(base,   0.0);       break;
            case BCKind::FixedY:        enforce(base+1, 0.0);       break;
            case BCKind::FixedXY:       enforce(base, 0.0);
                                        enforce(base+1, 0.0);       break;
            case BCKind::PrescribedX:   enforce(base,   bc.value);  break;
            case BCKind::PrescribedY:   enforce(base+1, bc.value);  break;
        }
    }
}

// ─── Gaussian elimination with partial pivoting ───────────────────────────────

bool SimulationDocument::solve_linear(std::vector<double>& K,
                                      std::vector<double>& F,
                                      std::vector<double>& u, int n) {
    // Forward elimination
    for (int col = 0; col < n; ++col) {
        // Partial pivot
        int pivot = col;
        double max_val = std::abs(K[static_cast<std::size_t>(col*n+col)]);
        for (int row = col+1; row < n; ++row) {
            double v = std::abs(K[static_cast<std::size_t>(row*n+col)]);
            if (v > max_val) { max_val = v; pivot = row; }
        }
        if (max_val < 1e-30) return false;

        if (pivot != col) {
            for (int j = 0; j < n; ++j)
                std::swap(K[static_cast<std::size_t>(col*n+j)],
                          K[static_cast<std::size_t>(pivot*n+j)]);
            std::swap(F[static_cast<std::size_t>(col)], F[static_cast<std::size_t>(pivot)]);
        }

        double diag = K[static_cast<std::size_t>(col*n+col)];
        for (int row = col+1; row < n; ++row) {
            double factor = K[static_cast<std::size_t>(row*n+col)] / diag;
            for (int j = col; j < n; ++j)
                K[static_cast<std::size_t>(row*n+j)] -= factor * K[static_cast<std::size_t>(col*n+j)];
            F[static_cast<std::size_t>(row)] -= factor * F[static_cast<std::size_t>(col)];
        }
    }
    // Back substitution
    u.assign(static_cast<std::size_t>(n), 0.0);
    for (int row = n-1; row >= 0; --row) {
        double sum = F[static_cast<std::size_t>(row)];
        for (int j = row+1; j < n; ++j)
            sum -= K[static_cast<std::size_t>(row*n+j)] * u[static_cast<std::size_t>(j)];
        u[static_cast<std::size_t>(row)] = sum / K[static_cast<std::size_t>(row*n+row)];
    }
    return true;
}

// ─── Solve ───────────────────────────────────────────────────────────────────

SimulationResult SimulationDocument::solve() const {
    SimulationResult res;
    int ndof = static_cast<int>(nodes_.size()) * 2;
    if (ndof == 0) {
        res.message = "No nodes defined";
        return res;
    }
    if (elements_.empty()) {
        res.message = "No elements defined";
        return res;
    }

    // Assemble
    auto K = assemble_K();
    std::vector<double> F(static_cast<std::size_t>(ndof), 0.0);

    // Apply nodal loads
    for (const NodalLoad& ld : loads_) {
        std::size_t base = static_cast<std::size_t>(ld.node_id) * 2;
        F[base]   += ld.fx;
        F[base+1] += ld.fy;
    }

    // Apply BCs
    apply_bcs(K, F);

    // Solve
    std::vector<double> u;
    if (!solve_linear(K, F, u, ndof)) {
        res.message = "Singular stiffness matrix — check boundary conditions";
        return res;
    }

    // Node results
    res.node_results.reserve(nodes_.size());
    for (const Node2D& n : nodes_) {
        NodeResult nr;
        nr.node_id = n.id;
        nr.ux = u[static_cast<std::size_t>(n.id)*2];
        nr.uy = u[static_cast<std::size_t>(n.id)*2+1];
        nr.resultant_displacement = std::sqrt(nr.ux*nr.ux + nr.uy*nr.uy);
        res.max_displacement = std::max(res.max_displacement, nr.resultant_displacement);
        res.node_results.push_back(nr);
    }

    // Element results — recover stress from displacements
    res.element_results.reserve(elements_.size());
    for (const Element2D& el : elements_) {
        const Material& mat = materials_.at(el.material_id);

        // Gather element displacement vector
        double ue[6]{};
        for (int ln = 0; ln < 3; ++ln) {
            std::size_t base = static_cast<std::size_t>(el.node_ids[static_cast<std::size_t>(ln)]) * 2;
            ue[2*ln]   = u[base];
            ue[2*ln+1] = u[base+1];
        }

        const Node2D& n0 = nodes_.at(el.node_ids[0]);
        const Node2D& n1 = nodes_.at(el.node_ids[1]);
        const Node2D& n2 = nodes_.at(el.node_ids[2]);
        double A2 = (n1.x-n0.x)*(n2.y-n0.y) - (n2.x-n0.x)*(n1.y-n0.y);
        if (A2 < 0) A2 = -A2;
        double b0 = n1.y-n2.y, b1 = n2.y-n0.y, b2 = n0.y-n1.y;
        double c0 = n2.x-n1.x, c1 = n0.x-n2.x, c2 = n1.x-n0.x;
        double inv2A = 1.0 / A2;

        // Strain = B * ue
        double e_xx = inv2A*(b0*ue[0] + b1*ue[2] + b2*ue[4]);
        double e_yy = inv2A*(c0*ue[1] + c1*ue[3] + c2*ue[5]);
        double g_xy = inv2A*(c0*ue[0]+b0*ue[1] + c1*ue[2]+b1*ue[3] + c2*ue[4]+b2*ue[5]);

        // Stress = D * epsilon
        double E = mat.youngs_modulus_pa, nu = mat.poissons_ratio;
        double fac = E / (1.0 - nu*nu);
        double s_xx = fac*(e_xx + nu*e_yy);
        double s_yy = fac*(nu*e_xx + e_yy);
        double t_xy = fac*(1.0-nu)*0.5*g_xy;

        // Von Mises (plane stress)
        double vm = std::sqrt(s_xx*s_xx - s_xx*s_yy + s_yy*s_yy + 3.0*t_xy*t_xy);

        ElementResult er;
        er.element_id = el.id;
        er.sigma_xx = s_xx; er.sigma_yy = s_yy; er.tau_xy = t_xy;
        er.von_mises = vm;
        er.yielded   = vm >= mat.yield_strength_pa;

        res.max_von_mises = std::max(res.max_von_mises, vm);
        res.element_results.push_back(er);
    }

    res.converged = true;
    res.message = "Linear static solution converged";
    return res;
}

// ─── Undamped modal analysis ────────────────────────────────────────────────

ModalResult SimulationDocument::solve_modal(std::size_t requested_modes) const {
    ModalResult result;
    if (requested_modes == 0) {
        result.message = "At least one mode must be requested";
        return result;
    }
    if (nodes_.empty() || elements_.empty()) {
        result.message = "A modal solve requires nodes and elements";
        return result;
    }

    const int ndof = static_cast<int>(nodes_.size()) * 2;
    std::vector<bool> constrained(static_cast<std::size_t>(ndof), false);
    for (const auto& bc : bcs_) {
        const int base = static_cast<int>(bc.node_id) * 2;
        switch (bc.kind) {
            case BCKind::FixedX:
            case BCKind::PrescribedX: constrained[static_cast<std::size_t>(base)] = true; break;
            case BCKind::FixedY:
            case BCKind::PrescribedY: constrained[static_cast<std::size_t>(base + 1)] = true; break;
            case BCKind::FixedXY:
                constrained[static_cast<std::size_t>(base)] = true;
                constrained[static_cast<std::size_t>(base + 1)] = true;
                break;
        }
    }
    std::vector<int> free_dofs;
    for (int i = 0; i < ndof; ++i) if (!constrained[static_cast<std::size_t>(i)]) free_dofs.push_back(i);
    if (free_dofs.empty()) {
        result.message = "All degrees of freedom are constrained";
        return result;
    }

    const auto full_k = assemble_K();
    const auto full_m = assemble_M();
    const int n = static_cast<int>(free_dofs.size());
    std::vector<double> k(static_cast<std::size_t>(n*n)), m(static_cast<std::size_t>(n*n));
    for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j) {
        k[static_cast<std::size_t>(i*n+j)] = full_k[static_cast<std::size_t>(
            free_dofs[static_cast<std::size_t>(i)]*ndof + free_dofs[static_cast<std::size_t>(j)])];
        m[static_cast<std::size_t>(i*n+j)] = full_m[static_cast<std::size_t>(
            free_dofs[static_cast<std::size_t>(i)]*ndof + free_dofs[static_cast<std::size_t>(j)])];
    }

    // Cholesky factor M=L*L^T. Failure generally means orphan nodes or invalid density.
    std::vector<double> l(static_cast<std::size_t>(n*n), 0.0);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j <= i; ++j) {
            double sum = m[static_cast<std::size_t>(i*n+j)];
            for (int q = 0; q < j; ++q) sum -= l[static_cast<std::size_t>(i*n+q)] * l[static_cast<std::size_t>(j*n+q)];
            if (i == j) {
                if (sum <= 1e-20 || !finite(sum)) {
                    result.message = "Mass matrix is not positive definite; check density and orphan nodes";
                    return result;
                }
                l[static_cast<std::size_t>(i*n+j)] = std::sqrt(sum);
            } else {
                l[static_cast<std::size_t>(i*n+j)] = sum / l[static_cast<std::size_t>(j*n+j)];
            }
        }
    }

    // Explicit L^-1, obtained by forward substitution one unit column at a time.
    std::vector<double> inv_l(static_cast<std::size_t>(n*n), 0.0);
    for (int column = 0; column < n; ++column) {
        for (int row = 0; row < n; ++row) {
            double rhs = row == column ? 1.0 : 0.0;
            for (int q = 0; q < row; ++q) rhs -= l[static_cast<std::size_t>(row*n+q)] * inv_l[static_cast<std::size_t>(q*n+column)];
            inv_l[static_cast<std::size_t>(row*n+column)] = rhs / l[static_cast<std::size_t>(row*n+row)];
        }
    }
    std::vector<double> left_product(static_cast<std::size_t>(n*n), 0.0);
    std::vector<double> transformed(static_cast<std::size_t>(n*n), 0.0);
    for (int i = 0; i < n; ++i) for (int b = 0; b < n; ++b) for (int a = 0; a < n; ++a)
        left_product[static_cast<std::size_t>(i*n+b)] +=
            inv_l[static_cast<std::size_t>(i*n+a)] * k[static_cast<std::size_t>(a*n+b)];
    for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j) for (int b = 0; b < n; ++b)
        transformed[static_cast<std::size_t>(i*n+j)] +=
            left_product[static_cast<std::size_t>(i*n+b)] * inv_l[static_cast<std::size_t>(j*n+b)];

    std::vector<double> eigenvalues, eigenvectors;
    if (!jacobi_eigen(std::move(transformed), n, eigenvalues, eigenvectors)) {
        result.message = "Symmetric eigensolver did not converge";
        return result;
    }
    std::vector<int> order(static_cast<std::size_t>(n));
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return eigenvalues[static_cast<std::size_t>(a)] < eigenvalues[static_cast<std::size_t>(b)];
    });
    const double max_eigenvalue = *std::max_element(eigenvalues.begin(), eigenvalues.end());
    const double positive_threshold = std::max(
        1e-12, std::abs(max_eigenvalue) * std::numeric_limits<double>::epsilon() * 100.0 * n);

    for (int eig_index : order) {
        const double lambda = eigenvalues[static_cast<std::size_t>(eig_index)];
        if (lambda <= positive_threshold || !finite(lambda)) continue; // rigid mode or numerical noise
        std::vector<double> phi(static_cast<std::size_t>(n), 0.0);
        for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j)
            phi[static_cast<std::size_t>(i)] += inv_l[static_cast<std::size_t>(j*n+i)] *
                                                 eigenvectors[static_cast<std::size_t>(j*n+eig_index)];
        double modal_mass = 0.0;
        for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j)
            modal_mass += phi[static_cast<std::size_t>(i)] * m[static_cast<std::size_t>(i*n+j)] * phi[static_cast<std::size_t>(j)];
        if (!(modal_mass > 0.0)) continue;
        for (double& value : phi) value /= std::sqrt(modal_mass);

        double residual_sq = 0.0, kphi_sq = 0.0;
        for (int i = 0; i < n; ++i) {
            double kphi = 0.0, mphi = 0.0;
            for (int j = 0; j < n; ++j) {
                kphi += k[static_cast<std::size_t>(i*n+j)] * phi[static_cast<std::size_t>(j)];
                mphi += m[static_cast<std::size_t>(i*n+j)] * phi[static_cast<std::size_t>(j)];
            }
            residual_sq += (kphi-lambda*mphi)*(kphi-lambda*mphi);
            kphi_sq += kphi*kphi;
        }
        ModalMode mode;
        mode.mode_number = result.modes.size() + 1;
        mode.eigenvalue_rad2_s2 = lambda;
        mode.frequency_hz = std::sqrt(lambda) / (2.0*kPi);
        mode.relative_residual = std::sqrt(residual_sq) / std::max(std::sqrt(kphi_sq), 1e-30);
        std::vector<double> full_phi(static_cast<std::size_t>(ndof), 0.0);
        for (int i = 0; i < n; ++i)
            full_phi[static_cast<std::size_t>(free_dofs[static_cast<std::size_t>(i)])] = phi[static_cast<std::size_t>(i)];
        for (const auto& node_value : nodes_) {
            const double ux = full_phi[static_cast<std::size_t>(node_value.id)*2];
            const double uy = full_phi[static_cast<std::size_t>(node_value.id)*2+1];
            mode.shape.push_back({node_value.id, ux, uy, std::hypot(ux, uy)});
        }
        result.modes.push_back(std::move(mode));
        if (result.modes.size() == requested_modes) break;
    }
    if (result.modes.empty()) {
        result.message = "No positive flexible modes found; check constraints";
        return result;
    }
    result.converged = true;
    result.message = "Undamped modal solution converged";
    return result;
}

// ─── Report ──────────────────────────────────────────────────────────────────

std::string SimulationDocument::export_report(const SimulationResult& res) const {
    std::ostringstream ss;
    ss << "=== MyDesk Simulation — Linear Static Analysis Report ===\n";
    ss << "Status : " << (res.converged ? "CONVERGED" : "FAILED") << "\n";
    ss << "Message: " << res.message << "\n\n";

    ss << "--- Summary ---\n";
    ss << "Nodes   : " << nodes_.size() << "\n";
    ss << "Elements: " << elements_.size() << "\n";
    ss << "Max displacement (m): " << res.max_displacement << "\n";
    ss << "Max Von Mises (Pa)  : " << res.max_von_mises << "\n\n";

    ss << "--- Node Displacements ---\n";
    ss << "NodeID  Ux(m)          Uy(m)          Resultant(m)\n";
    for (const NodeResult& nr : res.node_results) {
        ss << nr.node_id << "\t"
           << nr.ux << "\t" << nr.uy << "\t" << nr.resultant_displacement << "\n";
    }

    ss << "\n--- Element Stresses ---\n";
    ss << "ElemID  Sxx(Pa)        Syy(Pa)        Txy(Pa)        VM(Pa)         Yielded\n";
    for (const ElementResult& er : res.element_results) {
        ss << er.element_id << "\t"
           << er.sigma_xx << "\t" << er.sigma_yy << "\t"
           << er.tau_xy   << "\t" << er.von_mises << "\t"
           << (er.yielded ? "YES" : "no") << "\n";
    }
    return ss.str();
}

std::string SimulationDocument::export_modal_report(const ModalResult& res) const {
    std::ostringstream ss;
    ss << "=== MyDesk Simulation — Undamped Modal Analysis Report ===\n";
    ss << "Status : " << (res.converged ? "CONVERGED" : "FAILED") << "\n";
    ss << "Message: " << res.message << "\n";
    ss << "Mass formulation: consistent CST translational mass\n";
    ss << "Mode\tFrequency(Hz)\tOmega^2(rad^2/s^2)\tRelative residual\n";
    for (const auto& mode : res.modes) {
        ss << mode.mode_number << '\t' << mode.frequency_hz << '\t'
           << mode.eigenvalue_rad2_s2 << '\t' << mode.relative_residual << '\n';
    }
    return ss.str();
}

// ─── Transactions and undo/redo ────────────────────────────────────────────

SimulationDocument::State SimulationDocument::capture_state(std::string label) const {
    return {materials_, nodes_, elements_, bcs_, loads_, std::move(label)};
}

void SimulationDocument::restore_state(const State& state) {
    materials_ = state.materials;
    nodes_ = state.nodes;
    elements_ = state.elements;
    bcs_ = state.bcs;
    loads_ = state.loads;
}

void SimulationDocument::begin_transaction(std::string label) {
    if (transaction_active_) throw std::logic_error("A simulation transaction is already active");
    if (label.empty()) throw std::invalid_argument("Transaction label must not be empty");
    transaction_before_ = capture_state(label);
    transaction_label_ = std::move(label);
    transaction_active_ = true;
}

void SimulationDocument::commit_transaction() {
    if (!transaction_active_ || !transaction_before_) throw std::logic_error("No simulation transaction is active");
    undo_stack_.push_back(std::move(*transaction_before_));
    redo_stack_.clear();
    last_history_label_ = transaction_label_;
    transaction_before_.reset();
    transaction_label_.clear();
    transaction_active_ = false;
}

void SimulationDocument::rollback_transaction() {
    if (!transaction_active_ || !transaction_before_) throw std::logic_error("No simulation transaction is active");
    restore_state(*transaction_before_);
    transaction_before_.reset();
    transaction_label_.clear();
    transaction_active_ = false;
}

bool SimulationDocument::undo() {
    if (transaction_active_) throw std::logic_error("Cannot undo during a simulation transaction");
    if (undo_stack_.empty()) return false;
    State target = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    redo_stack_.push_back(capture_state(target.label));
    restore_state(target);
    last_history_label_ = target.label;
    return true;
}

bool SimulationDocument::redo() {
    if (transaction_active_) throw std::logic_error("Cannot redo during a simulation transaction");
    if (redo_stack_.empty()) return false;
    State target = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    undo_stack_.push_back(capture_state(target.label));
    restore_state(target);
    last_history_label_ = target.label;
    return true;
}

// ─── Native schema-v2 persistence ──────────────────────────────────────────

std::string SimulationDocument::serialize_native() const {
    Json root = {{"format", "mydesk-simulation"}, {"schema_version", 2}};
    root["materials"] = Json::array();
    for (std::size_t id = 0; id < materials_.size(); ++id) {
        const auto& m = materials_[id];
        root["materials"].push_back({{"id", id}, {"name", m.name},
            {"youngs_modulus_pa", m.youngs_modulus_pa}, {"poissons_ratio", m.poissons_ratio},
            {"density_kg_m3", m.density_kg_m3}, {"yield_strength_pa", m.yield_strength_pa},
            {"ultimate_strength_pa", m.ultimate_strength_pa}});
    }
    root["nodes"] = Json::array();
    for (const auto& n : nodes_) root["nodes"].push_back({{"id", n.id}, {"x", n.x}, {"y", n.y}});
    root["elements"] = Json::array();
    for (const auto& e : elements_) root["elements"].push_back({{"id", e.id}, {"node_ids", e.node_ids},
                                                                 {"material_id", e.material_id}, {"thickness_m", e.thickness_m}});
    root["boundary_conditions"] = Json::array();
    for (const auto& bc : bcs_) root["boundary_conditions"].push_back({{"node_id", bc.node_id},
                                                                        {"kind", bc_kind_name(bc.kind)}, {"value", bc.value}});
    root["loads"] = Json::array();
    for (const auto& load : loads_) root["loads"].push_back({{"node_id", load.node_id}, {"fx", load.fx}, {"fy", load.fy}});
    return root.dump(2);
}

SimulationDocument SimulationDocument::deserialize_native(const std::string& text) {
    try {
        const Json root = Json::parse(text);
        if (!root.is_object() || root.value("format", std::string{}) != "mydesk-simulation")
            throw std::invalid_argument("Not a MyDesk Simulation project");
        if (root.value("schema_version", 0) != 2)
            throw std::invalid_argument("Unsupported MyDesk Simulation schema version");
        SimulationDocument document;
        const auto& materials = root.at("materials");
        const auto& nodes = root.at("nodes");
        const auto& elements = root.at("elements");
        const auto& boundary_conditions = root.at("boundary_conditions");
        const auto& loads = root.at("loads");
        if (!materials.is_array() || !nodes.is_array() || !elements.is_array() ||
            !boundary_conditions.is_array() || !loads.is_array())
            throw std::invalid_argument("Simulation project collections must be arrays");
        for (std::size_t i = 0; i < materials.size(); ++i) {
            const auto& item = materials.at(i);
            if (item.at("id").get<std::size_t>() != i) throw std::invalid_argument("Material IDs must be contiguous");
            Material m;
            m.name = item.at("name").get<std::string>();
            m.youngs_modulus_pa = item.at("youngs_modulus_pa").get<double>();
            m.poissons_ratio = item.at("poissons_ratio").get<double>();
            m.density_kg_m3 = item.at("density_kg_m3").get<double>();
            m.yield_strength_pa = item.at("yield_strength_pa").get<double>();
            m.ultimate_strength_pa = item.at("ultimate_strength_pa").get<double>();
            document.add_material(std::move(m));
        }
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            const auto& item = nodes.at(i);
            if (item.at("id").get<std::size_t>() != i) throw std::invalid_argument("Node IDs must be contiguous");
            document.add_node(item.at("x").get<double>(), item.at("y").get<double>());
        }
        for (std::size_t i = 0; i < elements.size(); ++i) {
            const auto& item = elements.at(i);
            if (item.at("id").get<std::size_t>() != i) throw std::invalid_argument("Element IDs must be contiguous");
            document.add_element(item.at("node_ids").get<std::array<std::uint32_t,3>>(),
                                 item.at("material_id").get<std::uint32_t>(), item.at("thickness_m").get<double>());
        }
        for (const auto& item : boundary_conditions)
            document.add_bc({item.at("node_id").get<std::uint32_t>(),
                             parse_bc_kind(item.at("kind").get<std::string>()), item.at("value").get<double>()});
        for (const auto& item : loads)
            document.add_load({item.at("node_id").get<std::uint32_t>(), item.at("fx").get<double>(), item.at("fy").get<double>()});
        return document;
    } catch (const nlohmann::json::exception& error) {
        throw std::invalid_argument(std::string("Invalid MyDesk Simulation project: ") + error.what());
    }
}

void SimulationDocument::save_native_file(const std::filesystem::path& path) const {
    if (path.empty()) throw std::invalid_argument("Simulation project path must not be empty");
    const auto temporary = std::filesystem::path(path.string() + ".tmp");
    const auto backup = std::filesystem::path(path.string() + ".bak");
    const bool destination_existed = std::filesystem::exists(path);
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output || !(output << serialize_native()) || !output.flush())
            throw std::runtime_error("Could not write temporary simulation project: " + temporary.string());
    }
    // Read and validate the exact bytes before replacing the user's project.
    (void)load_native_file(temporary);
    std::error_code ec;
    if (destination_existed) {
        std::filesystem::copy_file(path, backup, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            std::filesystem::remove(temporary);
            throw std::runtime_error("Could not create simulation project backup: " + ec.message());
        }
    }
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        std::error_code remove_error;
        std::filesystem::remove(path, remove_error);
        if (remove_error) {
            std::filesystem::remove(temporary);
            throw std::runtime_error("Could not replace simulation project: " + remove_error.message());
        }
        ec.clear();
        std::filesystem::rename(temporary, path, ec);
    }
    if (ec) {
        std::filesystem::remove(temporary);
        if (destination_existed) {
            std::error_code recovery_error;
            std::filesystem::copy_file(backup, path, std::filesystem::copy_options::overwrite_existing, recovery_error);
        }
        throw std::runtime_error("Could not replace simulation project: " + ec.message());
    }
}

SimulationDocument SimulationDocument::load_native_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Could not open simulation project: " + path.string());
    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input.good() && !input.eof()) throw std::runtime_error("Could not read simulation project: " + path.string());
    return deserialize_native(buffer.str());
}

} // namespace mydesk::simulation
