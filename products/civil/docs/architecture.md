# MyDesk Civil Architecture

MyDesk Civil (`MyDeskCivil`) is an independent, standalone desktop civil engineering application designed for complete behavioral parity with Autodesk Civil 3D.

## Domain Model
- **COGO Points**: 3D coordinate geometry points (point number, Easting X, Northing Y, Elevation Z, raw description).
- **TIN Surface Engine**:
  - Delaunay Triangulation computed via Bowyer-Watson triangulation algorithm.
  - Continuous surface elevation evaluation with barycentric triangle interpolation.
  - Average End Area and Prismoidal Cut/Fill earthwork volume computation against reference datum elevations.
  - Algorithmic contour generation with user-defined elevation intervals.
- **Horizontal Alignment**:
  - Tangent-to-curve continuous alignment path.
  - Engineering station formatting (`STA 00+00.00`).
  - Bidirectional coordinate transformation: $(x, y) \leftrightarrow (\text{Station}, \text{Offset})$.
- **Vertical Profile**:
  - Vertical Point of Intersection (PVI) stationing.
  - Symmetrical parabolic vertical curve computation for crest and sag curves.
- **Exchange**:
  - LandXML 1.2 interoperability with schema-compliant `<LandXML>` generation.
  - Native JSON persistence format (`mydesk-civil`).

## Binary Separation
- **Engine**: `mydesk::civil_engine` (in `products/civil/engine/`)
- **Executable**: `MyDeskCivil` (in `products/civil/app/`)
- **Tests**: `mydesk_civil_tests` (in `products/civil/tests/`)
