# MyDesk Fusion changelog

## Unreleased

- Replaced the demonstration console entry point with a persistent Qt 6 CAD/CAM workspace.
- Added a component browser, editable parametric timeline, feature properties, OCCT body viewport, and CAM toolpath overlay.
- Connected feature parameter edits, suppression, timeline changes, undo/redo, native project save/open, and G-code postprocessing to the Fusion document engine.
- Added atomic `.myfusion` and G-code writes, per-application window preferences, and installed-bundle smoke startup.
- Added automated desktop workflow coverage for authoring, editing, manufacturing, electronics, undo/redo, persistence, and export.
- Added model-level feature editing and snapshot history with validation rollback.

## 0.1.0

- Replaced scalar-only rectangular extrusions with Open CASCADE B-rep solids.
- Added kernel validity checks, unique topology counts, and kernel-derived volume, surface area, and bounds.
- Regenerated supported push/pull results as valid B-rep bodies with persistent feature references.
- Replaced fixed 50 × 50 CAM coordinates with cutter-compensated raster paths derived from a selected body's actual bounds.
- Separated XY stepover from Z stepdown and reject invalid tool, depth, engagement, or boundary inputs.
- Added regression coverage for topology, mass properties, push/pull regeneration, selected-geometry G-code, independent depth levels, invalid CAM inputs, and native round-trip B-rep regeneration.
