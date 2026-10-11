# MyDesk Mechanical Changelog

## Unreleased

- Added bounded rigid, revolute, and slider assembly joints with normalized axes and drive positions.
- Applied joint rotations and translations to occurrence B-reps during OCCT compound assembly generation.
- Added undo/redo for joint drive edits.
- Replaced incomplete assembly loading with schema-v2 restoration of occurrence IDs, embedded parametric parts, transforms, grounded state, constraints, joints, limits, and current positions.
- Added regression coverage for constraint persistence, revolute and slider solving, joint limits, transformed B-rep bounds, undo/redo, and driven-motion round trips.

## [0.1.0] - 2026-10-10

### Added
- Parametric cylindrical hole boolean cut feature (`addHole`, `FeatureKind::Hole`) using OpenCASCADE `BRepAlgoAPI_Cut` and `BRepPrimAPI_MakeCylinder`.
- Full Inventor-class `AssemblyDocument` with component occurrences, 3D translation/rotation coordinates, grounded instances, and constraints (Mate, Flush, Insert).
- Assembly solver evaluating 3D offsets and positioning occurrences.
- Compound B-rep assembly shape generation using `TopoDS_Compound` and `BRepBuilderAPI_Transform`.
- Multi-component bill of materials (BOM) quantity and volume roll-up.
- Assembly STEP file export using `STEPControl_Writer`.
- Native assembly persistence format (`.mydesk-assy`).
- Automated regression test coverage in `test_mechanical.cpp` verifying feature regeneration, boolean subtraction, constraint solving, BOM rollups, and STEP output.
- Manifest capability tracking in `mechanical-inventor-2025.json` for `INV-HOLE` and `INV-ASSEMBLY`.
