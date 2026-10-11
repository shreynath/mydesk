# MyDesk Coordinate Changelog

## 0.3.0

- Replaced the demonstration-only command-line entry point with a persistent Qt 6 desktop
  application and independently bundled Qt runtime deployment.
- Added a federated model browser, discipline-aware interactive 3D box/OBB viewport, element
  picking, orbit, pan, zoom, fit, and clash-point focus.
- Added real coordination-box creation and an engine-bound property inspector for names,
  categories, and geometric extents; property edits participate in undo/redo.
- Added interactive hard/clearance test setup, execution, result inspection, review dispositions,
  comments, and stable rerun handling.
- Added atomic native open/save/save-as plus BCF VisualizationInfo XML and CSV report export.
- Added an offscreen Qt integration test covering the model tree, viewport construction, clash
  results, property editing, and document undo.

Known limitations: source IFC/NWC/NWD/OBJ/STEP geometry is recorded by path but is not imported;
geometry creation is currently box/OBB based; BCF export is VisualizationInfo XML rather than a
ZIP topic archive; the viewport uses a CPU painter renderer and does not yet implement mesh
sectioning, saved viewpoints, redlines, or measurement markup.

## 0.2.0

- Added rigid oriented-box geometry with Euler construction and validated orthonormal bases.
- Added sweep-and-prune broad-phase filtering and observable run statistics.
- Added 15-axis SAT hard-clash narrow phase and exact OBB feature-distance clearance checks.
- Suppressed duplicate unordered pairs for overlapping selection sets.
- Preserved stable result IDs, comments, and review dispositions across reruns.
- Added undo/redo for clash review status and comments.
- Upgraded native persistence to roundtrip model sources, elements, oriented geometry, test
  definitions, selection sets, results, and review state.
- Added regression coverage for rotated-element false positives, clearance distance, broad-phase
  reduction, duplicate suppression, review undo/redo, reruns, and persistence.
