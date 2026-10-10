# MyDesk Mechanical — architecture

Target: Autodesk Inventor-class parametric mechanical CAD.

## Document model

`PartDocument` owns:

- Named **parameters** (driving dimensions)
- Ordered **feature history** (`SketchRectangle`, `Extrude`, …)
- Explicit feature **dependencies**
- Cached OCCT `TopoDS_Shape` result after `regenerate()`

Sketches and features store parameter ids, not baked geometry. Editing `Sketch1.Width` and calling `regenerate()` rebuilds the B-rep.

## Computational engine

Open CASCADE Technology (OCCT) provides B-rep topology. The first vertical slice builds a rectangular profile extrusion as an OCCT solid (`BRepPrimAPI_MakeBox` as the prism of a rectangular sketch). Volume and solid checks use `BRepGProp`.

Future features (fillet, hole, boolean, revolve) must append to the same feature graph and emit/consume `TopoDS_Shape` bodies.

## Persistence and interchange

- Native: `.mydesk-mech` JSON storing parameters + feature history (not a mesh dump)
- STEP: `STEPControl_Writer` from the regenerated solid

## UI

`MyDeskMechanical` is currently a headless/CLI executable that regenerates and optionally writes STEP. A Qt modeling shell with feature tree and sketch UI is the next UI slice.

## Tests

`mydesk_mechanical_tests` verifies solid topology/volume, parameter-driven regeneration, native history reload, and non-empty STEP output.
