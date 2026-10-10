# MyDesk Coordinate Architecture

MyDesk Coordinate (`MyDeskCoordinate`) is an independent, standalone desktop model coordination and clash detection application designed for complete behavioral parity with Autodesk Navisworks.

## Domain Model
- **Aggregated Models**: Multi-discipline federated container aggregating architectural, structural, MEP, and civil geometry models.
- **Hierarchical Coordination Elements**: 3D bounding boxes (Axis-Aligned Bounding Box `AABB`) with discipline tags and categories.
- **Clash Detection Engine**:
  - **Hard Clash Detection**: Detects overlapping 3D solids where interference depth exceeds tolerance thresholds.
  - **Clearance Clash Detection**: Evaluates geometric proximity buffer distances between isolated systems (e.g., ductwork clearance around high-voltage conduits).
  - **Clash Matrix & Results**: Categorizes clashes by status (`New`, `Active`, `Reviewed`, `Approved`, `Resolved`) with intersection coordinates $(x, y, z)$ and penetration distance.
- **Exchange**:
  - Open BIM Collaboration Format (BCF 2.1 XML) issue and viewpoint export.
  - Native JSON session persistence (`mydesk-coord`).

## Binary Separation
- **Engine**: `mydesk::coordinate_engine` (in `products/coordinate/engine/`)
- **Executable**: `MyDeskCoordinate` (in `products/coordinate/app/`)
- **Tests**: `mydesk_coordinate_tests` (in `products/coordinate/tests/`)
