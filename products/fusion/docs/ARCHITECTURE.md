# MyDesk Fusion — Architecture

MyDesk Fusion is an integrated parametric design, direct modeling, CAM, and electronics integration system targeting the commercial capabilities of Autodesk Fusion.

The current implementation is a partial compatibility foundation. It does not claim complete Autodesk Fusion parity.

## Scope & Target Functionality
- **Commercial Target**: Autodesk Fusion (current releases).
- **Core Engine Subsystems**:
  - **Parametric History Timeline**:
    - Linear chronological timeline tracking rectangle sketches, solid extrusions, and direct-modeling modifications.
    - Timeline rollback and replay / regeneration engine.
    - Feature suppression and modification propagation.
    - Extrusions are evaluated by Open CASCADE into genuine B-rep solids. Volume, area, bounding boxes, validity, and unique solid/shell/face/edge/vertex counts are read from the evaluated topology rather than copied from scalar formulas.
  - **Direct Modeling**:
    - Planar end-face push/pull of the currently supported prismatic body, with persistent source-feature resolution during timeline replay.
  - **CAM & Manufacturing**:
    - Selected-body 2.5D rectangular pocket raster generation and standard RS-274 G-code post-processing.
    - Tool-center boundaries are derived from the selected B-rep bounds and cutter radius.
    - Stepover controls XY raster spacing; independent stepdown controls Z levels. Invalid cutter, depth, engagement, and boundary combinations fail regeneration instead of emitting unsafe fixed coordinates.
  - **ECAD/MCAD Interoperability**:
    - Printed circuit board outline modeling, keep-out zones, mounting hole integration, and 3D component packaging.
  - **Persistence & Exchange**:
    - Native editable `.myfusion` JSON format.
- **Binary**: `MyDeskFusion`.
- **Identity**: Bundle `app.mydesk.fusion`.

## Desktop application

The Qt 6 application is a persistent product-specific workspace rather than a
demonstration launcher. Its browser separates design, manufacturing, and
electronics features. Selecting a feature exposes the parameters supported by
its engine timeline entry; applying a change regenerates the OCCT body and
rejects invalid geometry. The horizontal timeline shows chronological order,
suppression, and rollback state. Undo/redo uses document snapshots and covers
creation, parameter changes, naming, suppression, and cursor movement.

The interactive viewport visualizes evaluated body bounds and overlays the
actual generated rapid and feed toolpath points. It supports body picking,
pan, zoom, and fit. Native `.myfusion` writes and G-code postprocessing are
atomic, and the application keeps its own window preferences. The desktop
workflow is exercised headlessly by an automated Qt test and supports
`--packaging-smoke` for installed-bundle startup verification.

## Current limitations

- Sketch support is currently limited to axis-aligned rectangles and extrusion to a single positive direction.
- Push/pull currently supports the planar end face of a rectangular prism; arbitrary face offsets and general topology naming remain future work.
- CAM currently supports a flat, axis-aligned rectangular face derived from the selected body. Stock models, holders, rest machining, arbitrary pocket islands, collision checking, and machine kinematics are not implemented yet.
- Native persistence retains editable timeline inputs but is not yet a versioned schema with general B-rep payloads.
- Neutral STEP/STL exchange is not implemented yet.
- The viewport currently displays a selectable axonometric body envelope rather
  than face-level OCCT tessellation; arbitrary face/sub-element picking is not implemented.
- Desktop authoring exposes only the feature types already supported by the engine.
