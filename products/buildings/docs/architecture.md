# MyDesk Buildings — architecture

Target: Autodesk Revit-class BIM.

## Document model

`ProjectDocument` is a **semantic** building model:

- Levels with elevations
- Wall/Door **types** (type parameters)
- Wall instances hosted to levels (location line + height)
- Door instances hosted to walls
- Rooms with boundaries
- Schedules derived from the semantic model

Changing a level elevation updates wall base/top elevations through accessors used by views, schedules, and IFC export. This is not a mesh bag.

## Interoperability

`exportIfc4()` emits IFC4 STEP text with `IfcProject`, `IfcSite`, `IfcBuilding`, `IfcBuildingStorey`, `IfcWallStandardCase`, `IfcDoor`, spatial containment, and wall property sets (length/base/top). Geometry representations and openings are subsequent slices.

Native `.mydesk-bim` JSON round-trips levels, types, walls, and hosted doors.

## UI

`MyDeskBuildings` currently launches a CLI summary of the project and schedules. A discipline-specific Qt shell (project browser, level views, properties) follows.

## Tests

Multistory project, wall schedule quantities, level elevation propagation, native round-trip of host relationships, and IFC entity presence.
