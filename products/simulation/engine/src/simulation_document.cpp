// SPDX-License-Identifier: MPL-2.0
// MyDesk Simulation — linear static FEA engine implementation
// Reference: Zienkiewicz & Taylor "The Finite Element Method", vol. 1
//            Logan "A First Course in the Finite Element Method"
#include "mydesk/simulation/simulation_document.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace mydesk::simulation {

// ─── Document setup ──────────────────────────────────────────────────────────

SimulationDocument::SimulationDocument() = default;

std::uint32_t SimulationDocument::add_material(Material m) {
    auto id = static_cast<std::uint32_t>(materials_.size());
    materials_.push_back(std::move(m));
    return id;
}

Material& SimulationDocument::material(std::uint32_t id) {
    return materials_.at(id);
}

std::uint32_t SimulationDocument::add_node(double x, double y) {
    auto id = static_cast<std::uint32_t>(nodes_.size());
    nodes_.push_back({id, x, y});
    return id;
}

std::uint32_t SimulationDocument::add_element(const std::array<std::uint32_t,3>& nids,
                                               std::uint32_t mat_id, double thickness_m) {
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
    bcs_.push_back(bc);
}

void SimulationDocument::add_load(const NodalLoad& load) {
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
                Ke[i*6+j] += t * A * B[k][i] * DB[k][j];

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
            dofs[2*ln]   = static_cast<int>(el.node_ids[ln]) * 2;
            dofs[2*ln+1] = static_cast<int>(el.node_ids[ln]) * 2 + 1;
        }
        // Scatter into global K
        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < 6; ++j)
                K[static_cast<std::size_t>(dofs[i]*n + dofs[j])] += Ke[static_cast<std::size_t>(i*6+j)];
    }
    return K;
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
            std::size_t base = static_cast<std::size_t>(el.node_ids[ln]) * 2;
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

} // namespace mydesk::simulation
