# MyDesk Surface — Architecture

MyDesk Surface is a dedicated precision Class-A industrial and automotive surfacing application targeting the commercial capabilities of Autodesk Alias.

## Scope & Target Functionality
- **Commercial Target**: Autodesk Alias (2025/2026).
- **Core Engine Subsystems**:
  - **NURBS Curves & Precision Evaluation**:
    - Arbitrary degree $p$ NURBS curves with rational weights and non-uniform knot vectors.
    - Cox-de Boor recursive evaluation of coordinates and 1st/2nd derivatives.
    - Curvature computation and curvature combs with configurable scale and sample density.
  - **NURBS Tensor-Product Surfaces**:
    - Bi-parametric $S(u, v)$ surfaces with $(p, q)$ degrees and control vertex grids $(M \times N)$.
    - Surface normals, tangent planes, and first/second fundamental forms.
    - Gaussian curvature $K(u, v)$ and Mean curvature $H(u, v)$ evaluation across the domain.
  - **Class-A Continuity Verification**:
    - Boundary seam diagnostics between adjacent patches:
      - **G0 (Positional)**: Maximum edge gap tolerance verification.
      - **G1 (Tangency)**: Normal vector angular deviation across seam.
      - **G2 (Curvature)**: Principal normal curvature alignment across shared boundary.
  - **Surface Quality Analysis**:
    - Zebra stripe reflection line simulation for highlight line continuity inspection.
  - **Interoperability & Persistence**:
    - Native `.mysurface` JSON persistence.
    - IGES (Entity 128 Rational B-Spline Surface) and STEP neutral export.
- **Binary**: `MyDeskSurface`.
- **Identity**: Bundle `app.mydesk.surface`.
