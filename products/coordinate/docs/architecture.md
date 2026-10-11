# MyDesk Coordinate Architecture

MyDesk Coordinate (`MyDeskCoordinate`) is an independent, standalone desktop model coordination and clash detection application designed for complete behavioral parity with Autodesk Navisworks.

## Domain Model
- **Aggregated Models**: Multi-discipline federated container aggregating architectural, structural, MEP, and civil geometry models.
- **Hierarchical Coordination Elements**: discipline/category-tagged elements with a cached axis-aligned broad-phase box and optional rigid oriented-box (`OBB`) narrow-phase geometry.
- **Clash Detection Engine**:
  - **Broad Phase**: X-axis sweep-and-prune with expanded Y/Z interval rejection for clearance tests. Run statistics expose raw pairs, candidates, and narrow-phase checks.
  - **Hard Clash Detection**: Uses the separating axis theorem over 15 face and edge-cross axes for rigid OBBs, rejecting rotated-element false positives left by AABB overlap. Legacy AABB elements retain overlap-depth semantics.
  - **Clearance Clash Detection**: Computes exact OBB feature distance using vertex/face and edge/edge closest points, including a witness midpoint for the result location.
  - **Pair Identity**: Treats a clash as an unordered element pair, preventing duplicate results when selection sets overlap. Reruns preserve result IDs, review state, and comments for surviving pairs.
  - **Clash Matrix & Results**: Categorizes clashes by status (`New`, `Active`, `Reviewed`, `Approved`, `Resolved`) with intersection coordinates $(x, y, z)$ and penetration distance.
  - **Review Transactions**: Status/comment edits participate in a product-local undo/redo history.
  - **Element Transactions**: Changes to names, categories, and coordination-box extents use the
    same document-owned undo/redo path. Editing the extents of a rotated OBB explicitly converts
    it to an axis-aligned box; rotation-aware manipulators are not yet implemented.
- **Exchange**:
  - Open BIM Collaboration Format (BCF 2.1 XML) issue and viewpoint export.
  - Native JSON session persistence (`mydesk-coord`, schema version 2) roundtrips model sources, elements, OBB bases, clash-test definitions, selection sets, results, and review dispositions.

## Qt Desktop Application

The `MyDeskCoordinate` executable is a product-specific Qt 6 desktop application rather than a
suite launcher or a console fixture. Its widgets are projections of `CoordinateDocument` state:

- The federated model tree presents model/discipline/element hierarchy and drives selection.
- The painter-based 3D coordination viewport draws actual AABB and OBB vertices from the engine,
  supports picking, orbit, pan, zoom, fit, and clash-point focus, and colors geometry by discipline.
- The property inspector edits the selected element through `CoordinateDocument::updateElement`.
- Clash Detective creates and runs hard/clearance tests against selected models, lists engine
  results, and edits review status/comments through the document undo stack.
- File actions use `QSaveFile` for atomic `.mydesk-coord` saves and provide native reopen,
  BCF VisualizationInfo XML export, and a CSV clash report.
- `QSettings` persists application window/dock layout independently for Coordinate.

The application target is a macOS bundle / Windows GUI executable where appropriate. Qt's deploy
script is installed with the standalone product so its framework and platform-plugin runtime are
collected into the package when supported by the host Qt installation.

## Current Geometry Boundary

The narrow phase supports rigid boxes with arbitrary yaw, pitch, and roll. Triangle meshes,
curved primitives, transformed model hierarchies, and compound solids remain future work; the
capability manifest therefore remains `FUNCTIONAL`, not `VERIFIED_PARITY`.

Appending a federated model currently records its source path and discipline; the application does
not parse IFC, NWC, NWD, OBJ, or STEP source geometry. Users can create and edit coordination boxes
that exercise the actual clash engine. Native format parsing is schema-aware but remains a small
product-local parser rather than a general JSON/IFC validation stack. Saved viewpoints, section
planes, measurement/redline markup, issue assignment, and mesh rendering are also not implemented.

## Binary Separation
- **Engine**: `mydesk::coordinate_engine` (in `products/coordinate/engine/`)
- **Executable**: `MyDeskCoordinate` (in `products/coordinate/app/`)
- **Tests**: `mydesk_coordinate_tests` and `mydesk_coordinate_ui_tests` (in
  `products/coordinate/tests/`); the latter runs Qt offscreen and verifies document/widget binding.
