# MyDesk Civil architecture

MyDesk Civil is an independently built civil engineering design application. Its domain model,
desktop interface, persistence, tests, executable, and release version live under `products/civil`.
It does not require another MyDesk product at runtime.

## Domain model

- COGO points preserve point number, survey coordinates, elevation, and raw description.
- TIN surfaces use incremental Bowyer-Watson Delaunay triangulation in the easting/northing plane.
  They provide barycentric elevation queries, triangle contour intersections, and datum-based
  planar-triangle cut/fill estimates.
- Horizontal alignments contain tangent and circular-curve entities, cumulative stationing,
  coordinate-at-station evaluation, and tangent station/offset projection.
- Profiles contain station/elevation PVIs and evaluate tangent grades plus the implemented
  symmetrical parabolic vertical-curve case.

## Editing and persistence

The Qt application owns undo and redo as complete serialized document transactions. Point edits
update every referencing surface and recompute its Delaunay topology. The native
`.mydesk-civil` JSON format has an explicit identity and schema version and persists COGO points,
surface source references and faces, alignment geometry, profiles, stable IDs, and names.
LandXML 1.2 export writes CgPoints, surface points/faces, tangent and curve alignment geometry,
and profile PVIs. Import is not implemented.

## Desktop workspace

The main window combines a product-local Civil Project Browser with a CPU plan viewport and a
domain property inspector. The viewport draws survey points, elevation-shaded TIN faces,
generated contours, and sampled horizontal alignments; it supports selection, pan, zoom, and fit.
The property system edits COGO coordinates and object names, evaluates cut/fill against a datum,
and performs alignment station/offset inquiry. Creation commands add COGO points, build a surface
from project points, create an alignment, and create a two-PVI design profile.

## Current limits

This is a substantive civil-design slice, not Civil 3D parity. Surface support does not yet include
breaklines, exclusion boundaries, point groups, survey-network adjustment, watershed analysis,
or robust contour stitching. Volume calculation treats each triangle by signed mean elevation and
does not split triangles where the datum crosses a face. Alignment station/offset projection is
currently tangent-only; spirals, compound geometry constraints, station equations, labels, and
superelevation are absent. Profiles lack a graphical profile editor. Corridors, assemblies,
grading objects, parcels, pressure/pipe networks, quantity takeoff, LandXML import, and DWG
interoperability remain unimplemented.
