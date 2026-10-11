# MyDesk Buildings Changelog

## 0.2.0

- Added wall top constraints to levels with independent top offsets.
- Added computed hosted door/window world positions that follow wall and level edits.
- Added associative aligned, vertical, and 3D dimensions over live element references.
- Added atomic validation that rejects level edits which would invert constrained walls.
- Added undo/redo for level elevations, wall geometry, and wall top constraints.
- Upgraded native persistence to schema version 2 for wall constraints, opening types, and
  associative dimensions while retaining schema version 1 compatibility.
- Added regression coverage for the complete level-to-wall-to-opening-to-dimension dependency chain,
  invalid-edit rollback, undo/redo, IFC output, and native roundtrip.
