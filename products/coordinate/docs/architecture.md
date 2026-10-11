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
- **Exchange**:
  - Open BIM Collaboration Format (BCF 2.1 XML) issue and viewpoint export.
  - Native JSON session persistence (`mydesk-coord`, schema version 2) roundtrips model sources, elements, OBB bases, clash-test definitions, selection sets, results, and review dispositions.

## Current Geometry Boundary

The narrow phase supports rigid boxes with arbitrary yaw, pitch, and roll. Triangle meshes,
curved primitives, transformed model hierarchies, and compound solids remain future work; the
capability manifest therefore remains `FUNCTIONAL`, not `VERIFIED_PARITY`.

## Binary Separation
- **Engine**: `mydesk::coordinate_engine` (in `products/coordinate/engine/`)
- **Executable**: `MyDeskCoordinate` (in `products/coordinate/app/`)
- **Tests**: `mydesk_coordinate_tests` (in `products/coordinate/tests/`)
