# MyDesk Mechanical Changelog

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
