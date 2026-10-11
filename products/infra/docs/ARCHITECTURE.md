# MyDesk Infra — Architecture

MyDesk Infra is a dedicated standalone conceptual infrastructure design application targeting the capabilities of Autodesk InfraWorks.

## Scope & Target Functionality
- **Commercial Target**: Autodesk InfraWorks (2025/2026).
- **Core Engine Subsystems**:
  - **Geospatial Terrain & Elevation (DEM)**:
    - High-resolution gridded Digital Elevation Models (DEM).
    - Bilinear elevation interpolation across easting/northing coordinates.
    - Coordinate Reference Systems (CRS) and geospatial projection mapping.
  - **Component Roads & Corridors**:
    - Parametric centerline alignments, stationing, design speed, number of lanes, lane widths, and shoulder configurations.
    - Road grade and vertical alignment computation.
  - **Civil Structures (Bridges & Tunnels)**:
    - Parametric bridge spans over terrain depressions or water bodies.
    - Pier counts, deck clearance, and tunnel excavation corridors.
  - **3D City Elements & Land Use**:
    - Draped building footprints with extruded roof heights and occupancy categories.
    - Water features and lake/river polygons with constant or graded water elevation.
  - **Interoperability & Persistence**:
    - Native `.myinfra` JSON persistence.
    - GeoJSON feature collection export and LandXML interoperability.
- **Binary**: `MyDeskInfra`.
- **Identity**: Bundle `app.mydesk.infra`.
