# MyDesk Simulation architecture

MyDesk Simulation is an independently built structural analysis application. Its document, solver, persistence, command history, tests, executable, and release version live under `products/simulation`; it does not depend on another MyDesk application.

## Analysis model

The current discretization is a two-dimensional plane-stress constant-strain triangle (CST). Nodes have two translational degrees of freedom. Elements reference a linear-elastic isotropic material and a physical thickness. Material density participates in dynamic analysis; Young's modulus and Poisson's ratio define the constitutive matrix.

Linear static analysis assembles the dense global stiffness matrix, applies nodal forces and displacement constraints, solves by Gaussian elimination with partial pivoting, and recovers element stress and plane-stress Von Mises stress.

Undamped modal analysis assembles the same stiffness matrix and a consistent CST mass matrix. Constrained degrees of freedom are eliminated exactly. A Cholesky transformation converts `K phi = omega^2 M phi` into a real symmetric eigenproblem, which is solved by Jacobi rotations. Flexible modes are ordered by frequency, mass-normalized, expanded to the full mesh, and reported with an equilibrium residual. Rigid-body and numerical zero modes are excluded.

## Editing and persistence

Interactive model edits are transaction boundaries. Committed transactions capture materials, mesh, constraints, and loads and can be undone or redone. Solver results remain derived state and are invalidated by the application after an edit.

The native `.mdsim` representation is JSON with format identity `mydesk-simulation` and schema version 2. It persists all model inputs with structural type and reference validation. Save writes and reopens a same-directory temporary file before replacement and preserves the previous destination as `.bak`; POSIX replacement is atomic, while the portable fallback may briefly remove the destination after its backup exists.

## Desktop pre/post-processing

The standalone Qt 6 application owns a simulation model browser, validated material and nodal-coordinate property editing, domain creation commands for materials, nodes, CST elements, constraints, and nodal loads, and product-local undo/redo. A CPU finite-element viewport draws the actual CST connectivity and supports node/element selection, pan, zoom, fit, deformed overlays, displacement contours, Von Mises contours, and modal shape visualization. Static and modal result tables are derived from the same solver results shown by the viewport. Native `.mdsim` projects and text reports use independent file workflows.

## Current limits

This is a substantive solver slice, not Nastran parity. It supports small dense 2D CST models, linear isotropic plane stress, nodal loads, displacement constraints, linear static response, and undamped normal modes. It does not yet provide 3D solids or shells, sparse iterative solvers, contact, nonlinear or transient response, damping, frequency-response analysis, automatic meshing, nodal stress averaging, probe plots, animation export, or Nastran deck import/export. The CPU viewport is a diagnostic post-processor rather than a production GPU results system.
