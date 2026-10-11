# MyDesk Buildings — architecture

Target: Autodesk Revit-class BIM.

## Document model

`ProjectDocument` is a **semantic** building model:

- Levels with elevations
- Wall/Door **types** (type parameters)
- Wall instances hosted to levels (location line + height)
- Door instances hosted to walls
- Optional wall top constraints to a second level with independent base/top offsets
- Hosted door/window positions evaluated from normalized wall references
- Associative dimensions evaluated from live element references
- Rooms with boundaries
- Schedules derived from the semantic model

Dependency evaluation is live rather than copied: changing a level updates constrained wall height,
moving a wall updates hosted opening coordinates, and associative dimensions immediately reflect
the resulting positions. Constraint edits are validated before mutation, so a level move that would
invert a wall is rejected atomically. Level, wall geometry, and wall top-constraint edits participate
in product-local undo/redo history. Views, schedules, and IFC export consume the same computed wall
elevations. This is not a mesh bag.

## Interoperability

`exportIfc4()` emits IFC4 STEP text with `IfcProject`, `IfcSite`, `IfcBuilding`, `IfcBuildingStorey`, `IfcWallStandardCase`, `IfcDoor`, spatial containment, and wall property sets (length/base/top). Geometry representations and openings are subsequent slices.

Native `.mydesk-bim` JSON schema version 2 round-trips levels, wall and opening types, walls,
base/top constraints, hosted doors/windows, rooms, and associative dimensions. Undo history is
session-local and intentionally is not serialized.

## UI

`MyDeskBuildings` currently launches a CLI summary of the project and schedules. A discipline-specific Qt shell (project browser, level views, properties) follows.

## Tests

Multistory project, wall schedule quantities, two-level wall constraint propagation, hosted-opening
placement, associative dimensions, invalid-edit rollback, undo/redo, native round-trip of dependency
relationships, and IFC entity presence.

## Current boundary

This slice supports linear wall hosts and point references for walls, doors, windows, and structural
members. Face/edge stable-reference topology, equality constraint chains, temporary dimensions,
locked dimensions, and a general-purpose constraint solver remain future work. The manifest records
the dependency capability as `FUNCTIONAL`, not complete Revit parity.
