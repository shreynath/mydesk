# MyDesk Infra changelog

## 0.3.0

- Replaced the project-summary command line with a persistent Qt 6 conceptual infrastructure workspace.
- Added an infrastructure model browser and interactive CPU 3D viewport for DEM terrain,
  component roads, bridges, and extruded city buildings.
- Added engine-backed creation, selection, road/bridge/building property editing, corridor
  earthwork analysis, undo/redo, native save/open, and GeoJSON export.
- Added validated road, bridge, and building property update operations to the document engine.
- Added deployed-runtime bundling, installed UI construction smoke, and a Qt workflow regression.

## 0.2.0

- Added terrain-aware component-road profile sampling and corridor cut/fill volume analysis.
- Added transactional road centerline editing, dependency-aware road deletion, and undo/redo.
- Replaced substring-based project loading with versioned structural JSON parsing and validation.
- Added complete road, bridge, building, and DEM round trips plus atomic `.myinfra` file saving.
- Replaced the hard-coded executable demo with real project opening, summaries, and earthwork analysis.
