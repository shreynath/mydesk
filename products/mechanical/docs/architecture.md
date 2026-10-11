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

`MyDeskMechanical` is a persistent Qt 6 desktop application, not a one-shot
command-line demonstration. Its model browser displays the actual ordered part
feature graph or assembly occurrences, constraints, and joints. The parameter
table edits real driving dimensions and immediately regenerates the OCCT B-rep;
joint values drive the assembly solver. Box-extrusion and hole commands create
real engine features. Part-to-assembly insertion uses the regenerated part as an
independent occurrence.

The viewport triangulates the evaluated OCCT shape, depth-sorts and shades the
triangles, and supports orbit, zoom, fit, and body selection. Native open/save
uses atomic `QSaveFile` replacement. STEP export works for part and assembly
documents and IGES export works for parts. UI-level snapshot history covers
feature creation, parameter edits, occurrence insertion, and driven joint edits.
Window geometry and dock layout persist through Qt settings.

This interface is an implemented modeling slice, not Inventor UI parity. It does
not yet provide interactive sketch geometry/constraints, face/edge picking,
feature dialogs for every engine feature, drawing sheets, subassembly editing,
or a hardware-accelerated OCCT selection context.

## Tests

`mydesk_mechanical_tests` verifies solid topology/volume, parameter-driven regeneration, native history reload, constraint restoration, bounded joint motion, undo/redo, transformed OCCT assembly bounds, and non-empty STEP output. `mydesk_mechanical_gui_tests` verifies the desktop session's real part creation/edit/cut workflow, UI-level undo/redo, atomic native save/reopen, STEP export, assembly insertion, assembly undo/redo, and assembly reopen.
