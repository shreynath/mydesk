# MyDesk Mill Changelog

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
