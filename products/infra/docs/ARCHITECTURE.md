# MyDesk Infra — Architecture

MyDesk Infra is a dedicated standalone conceptual infrastructure design application targeting the capabilities of Autodesk InfraWorks.

## Scope & Target Functionality

- **Commercial target**: Autodesk InfraWorks (2025/2026).
- **Implemented engine subsystems**:
  - **Geospatial Terrain & Elevation (DEM)**:
    - Gridded Digital Elevation Models held in memory and in native projects.
    - Bilinear elevation interpolation across easting/northing coordinates.
    - Coordinate Reference System identifiers; projection transformation is not yet implemented.
  - **Component Roads & Corridors**:
    - Editable 3D centerline alignments, stationing, design speed, number of lanes, lane widths, and analysis shoulder width.
    - Road grade and piecewise-linear vertical alignment evaluation.
    - Terrain-aware corridor sampling, exact sign-change integration of cross-section cut/fill areas, and average-end-area earthwork volumes.
  - **Civil Structures**:
    - Bridge records associated with validated road station ranges.
    - Pier counts and deck thickness.
  - **3D City Elements & Land Use**:
    - Draped building footprints with extruded roof heights and occupancy categories.
  - **Interoperability & Persistence**:
    - Versioned native `.myinfra` JSON persistence with structural parsing, schema validation, version-1 migration, atomic file replacement, and complete roads/bridges/buildings round trips.
    - GeoJSON feature collection export for roads and buildings.
- **Editing and Transactions**:
  - Model-changing operations checkpoint document state for undo/redo.
  - Road centerlines remain editable; deleting a road also removes dependent bridges and can be undone as one transaction.
- **Application Integration**:
  - The Qt workspace owns a model browser, infrastructure properties, and a conceptual 3D viewport.
  - Terrain is rendered as an elevation-shaded DEM mesh; roads, bridge decks, and extruded
    building footprints are drawn from the same `InfraDocument` used for analysis and persistence.
  - Creation commands generate an editable terrain context, component road, bridge, and city
    building. Selection and property changes call validated engine transactions.
  - Corridor earthwork results are calculated from the selected road and active DEM. Native
    `.myinfra` and GeoJSON use independent open/save/export workflows.

## Current limits

This is a substantive conceptual-planning slice, not InfraWorks parity. The earthwork engine
currently evaluates a constant-width, level formation surface. Superelevation, daylight side
slopes, material strata, haul balancing, tunnels, water features, drainage, LandXML, raster/point
cloud import, coordinate transformations, proposal comparison, cloud collaboration, and
large-city GPU qualification remain incomplete. Road centerline vertices are editable through the
engine but do not yet have viewport grips; bridge and building geometry use scoped parametric
records rather than production structural or architectural models.

- **Binary**: `MyDeskInfra`.
- **Identity**: Bundle `app.mydesk.infra`.
