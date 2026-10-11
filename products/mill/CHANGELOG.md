# MyDesk Mill Changelog

## 0.3.0

- Replaced the command-line demonstration entry point with a persistent Qt 6 CAM desktop application.
- Added a CAM project browser for stock, cutting tools, selected boundaries, and calculated operations,
  plus editable domain property panels and dependency-aware path regeneration.
- Added a CPU-rendered top/isometric machining viewport with stock display, cutting/rapid path styling,
  toolpath hit selection, orbit, pan, zoom, and fit controls.
- Added product-local undo/redo for tools, stock, boundaries, operations, and calculated paths.
- Added validated schema-v1 `.mymill` persistence with staged file replacement, save-as, reopen, and
  unsaved-change protection.
- Added transactional generic Fanuc-style G-code export in metric or converted inch units; machine-
  specific posts and controller certification remain outside this release.
- Added an automated Qt authoring workflow and runtime deployment rules for independent installation.

## 0.2.0

- Added strictly convex, user-selected planar machining boundaries with exact cutter-radius offsets.
- Clipped raster lanes to selected geometry and generated successive contour offsets from that geometry.
- Corrected depth generation so the first cutting level is one stepdown below stock top and the final
  level is exactly the requested bottom.
- Replaced the hard-coded centre drill with explicit selected hole centres.
- Added validation for stock, tools, operation depths, cutter clearance, generated moves, and G-code
  posting.
- Added regression coverage for selected geometry, depth levels, invalid-operation rejection, path
  validation, and multi-hole drilling.
