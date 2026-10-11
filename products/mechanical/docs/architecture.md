# MyDesk Mechanical — architecture

Target: Autodesk Inventor-class parametric mechanical CAD.

## Document model

`PartDocument` owns:

- Named **parameters** (driving dimensions)
- Ordered **feature history** (`SketchRectangle`, `Extrude`, …)
- Explicit feature **dependencies**
- Cached OCCT `TopoDS_Shape` result after `regenerate()`

Sketches and features store parameter ids, not baked geometry. Editing `Sketch1.Width` and calling `regenerate()` rebuilds the B-rep.

`AssemblyDocument` owns independent part occurrences, placement constraints, rigid/revolute/slider joints, and BOM data. A joint records normalized motion axis, child origin offset, travel limits, and current drive position. Solving updates the occurrence transform; assembly shape generation applies both rotation and translation to the underlying OCCT B-rep.

## Computational engine

Open CASCADE Technology (OCCT) provides B-rep topology for extrude, revolve, sweep, loft, fillet, chamfer, shell, pattern, mirror, hole, rib, and emboss feature evaluation. Volume and surface checks use `BRepGProp`.

Assembly motion currently covers single-level global-axis rigid, revolute, and slider joints. Joint drive edits have bounded positions and an undo/redo history. General hierarchical transform composition, face/edge reference selection, degrees-of-freedom analysis, contact, adaptive parts, and dynamic simulation remain future work.

## Persistence and interchange

- Native part: `.mydesk-mech` JSON storing parameters + feature history (not a mesh dump)
- Native assembly: schema-v2 `.mydesk-assy` JSON storing complete occurrence part documents, transforms, constraints, joints, limits, and drive positions
- STEP: `STEPControl_Writer` from the regenerated solid

## UI

`MyDeskMechanical` is currently a headless/CLI executable that regenerates and optionally writes STEP. A Qt modeling shell with feature tree and sketch UI is the next UI slice.

## Tests

`mydesk_mechanical_tests` verifies solid topology/volume, parameter-driven regeneration, native history reload, constraint restoration, bounded joint motion, undo/redo, transformed OCCT assembly bounds, and non-empty STEP output.
