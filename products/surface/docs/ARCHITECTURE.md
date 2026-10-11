# MyDesk Surface — Architecture

MyDesk Surface is a dedicated precision Class-A industrial and automotive surfacing application targeting the commercial capabilities of Autodesk Alias.

The current implementation is a partial compatibility foundation and does not claim complete Autodesk Alias parity.

## Scope & Target Functionality
- **Commercial Target**: Autodesk Alias (2025/2026).
- **Core Engine Subsystems**:
  - **NURBS Curves & Precision Evaluation**:
    - Arbitrary degree $p$ NURBS curves with rational weights and non-uniform knot vectors.
    - Cox-de Boor recursive evaluation of coordinates and 1st/2nd derivatives.
    - Curvature computation and curvature combs with configurable scale and sample density.
  - **NURBS Tensor-Product Surfaces**:
    - Bi-parametric $S(u, v)$ surfaces with $(p, q)$ degrees and control vertex grids $(M \times N)$.
    - Analytical rational first and second derivatives, including the quotient-rule weight terms required by NURBS.
    - Surface normals, tangent planes, and first/second fundamental forms derived from those analytical derivatives, including at patch boundaries.
    - Gaussian curvature $K(u, v)$ and mean curvature $H(u, v)$ evaluation across the domain.
  - **Class-A Continuity Verification**:
    - Boundary seam diagnostics between adjacent patches:
      - **G0 (Positional)**: Maximum edge gap tolerance verification.
      - **G1 (Tangency)**: Normal vector angular deviation across seam.
      - **G2 (Curvature)**: Cross-seam normal-curvature alignment across the shared boundary.
    - G0/G1/G2 seam alignment for adjacent non-rational bicubic Bezier patches by solving boundary, first-derivative, and second-derivative control rows.
    - Alignment edits participate in document undo/redo.
  - **Surface Quality Analysis**:
    - Zebra stripe reflection line simulation for highlight line continuity inspection.
  - **Interoperability & Persistence**:
    - Versioned native `.mysurface` JSON persistence preserving curve knots/control points and complete surface knot vectors/control grids.
    - Qt file workflow uses `QSaveFile` replacement semantics so a failed write does not replace the previous native document.
- **Desktop application**:
  - Persistent Qt 6 main window with independent preferences under the `MyDeskSurface` application identity.
  - Surface browser separating curves and surface patches, an editable control-vertex property panel, and a seam-analysis panel.
  - CPU tessellation viewport evaluates the actual NURBS engine for shaded patches, wire/control nets,
    curves, normals used by zebra shading, selection, orbit, pan, and zoom.
  - Creation, naming, control-vertex editing, seam alignment, save/open, and undo/redo operate on the
    same `SurfaceDocument`; there is no parallel demonstration scene.
- **Binary**: `MyDeskSurface`.
- **Identity**: Bundle `app.mydesk.surface`.

## Current limitations

- Seam alignment currently supports the shared U boundary of non-rational bicubic Bezier patches. General NURBS degree, rational-weight, knot-insertion, and arbitrary-edge alignment are not implemented yet.
- The current IGES method is only an export preview and is not a conforming multi-entity IGES writer. STEP exchange is not implemented.
- The viewport is a CPU diagnostic renderer. GPU tessellation, high-quality antialiasing, material rendering,
  topology-aware sub-object picking, manipulators, construction history, and production render output are not implemented.
- Trimming, filleting, blending, draft analysis, surface intersections, general rebuild/knot editing, and production tessellation remain future work.
