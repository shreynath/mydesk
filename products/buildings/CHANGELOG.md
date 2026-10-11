# MyDesk Buildings Changelog

## 0.3.0

- Replaced the demonstration CLI entry point with a persistent Qt 6 BIM authoring application.
- Added a semantic project browser and an editable properties palette connected directly to
  `ProjectDocument` levels, walls, hosted doors, hosted windows, and rooms.
- Added zoomable level-plan and axonometric building viewports with real selection and rendering of
  computed level/wall/opening geometry.
- Added two-click wall creation and geometry-based door/window placement on wall hosts.
- Added application-level snapshot transactions so creation and property edits participate in
  undo/redo alongside the engine's parametric dependency updates.
- Added guarded New/Open/Save/Save As, atomic native file writes, and IFC4 file export.
- Added controller integration coverage for authoring, host editing, undo/redo, native reopen, and
  IFC export, plus an offscreen Qt workflow test that creates a semantic wall and hosted door through
  the actual toolbar and viewport and verifies desktop undo.
- Added product-local Qt runtime and platform-plugin deployment to standalone install packages.

## 0.2.0

- Added wall top constraints to levels with independent top offsets.
- Added computed hosted door/window world positions that follow wall and level edits.
- Added associative aligned, vertical, and 3D dimensions over live element references.
- Added atomic validation that rejects level edits which would invert constrained walls.
- Added undo/redo for level elevations, wall geometry, and wall top constraints.
- Upgraded native persistence to schema version 2 for wall constraints, opening types, and
  associative dimensions while retaining schema version 1 compatibility.
- Added regression coverage for the complete level-to-wall-to-opening-to-dimension dependency chain,
  invalid-edit rollback, undo/redo, IFC output, and native roundtrip.
