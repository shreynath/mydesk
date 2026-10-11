# MyDesk Simulation changelog

## 0.3.0

- Replaced the interactive command-line entry point with a persistent Qt 6 FEA pre/post-processing workspace.
- Added a simulation model browser for materials, nodes, CST elements, constraints, and nodal loads.
- Added editable material and nodal-coordinate properties with validated, transaction-backed undo/redo.
- Added an interactive finite-element viewport with entity selection, pan, zoom, fit, deformed shape,
  displacement contour, Von Mises contour, and modal-shape display modes.
- Connected benchmark creation, mesh/input authoring, linear-static solve, modal solve, native save/open,
  and atomic report export directly to `SimulationDocument`.
- Added an installed-runtime deployment rule and Qt workflow regression covering solve, edit, undo,
  persistence, and report export.

## 0.2.0

- Added undamped modal analysis using the CST consistent mass matrix, exact constraint elimination, Cholesky reduction, symmetric eigensolution, mass-normalized shapes, and numerical residuals.
- Added a closed-form two-degree-of-freedom modal regression benchmark.
- Added model-edit transactions with undo and redo.
- Added validated schema-v2 native project serialization, staged same-directory replacement, and backup files.
- Added interactive modal solve/report, native open/save, new-project, undo, and redo commands.
- Added physical validation for materials, nodes, elements, constraints, and loads.

## 0.1.0

- Added the initial 2D plane-stress CST linear-static solver and text report.
