# MyDesk Surface changelog

## Unreleased

- Added analytical first and second derivatives for rational tensor-product NURBS surfaces.
- Reworked normals, Gaussian curvature, and mean curvature to use exact basis derivatives instead of finite differences.
- Added sampled G2 seam diagnostics using cross-seam normal curvature.
- Added G0, G1, and G2 control-net alignment for adjacent non-rational bicubic Bezier patches, including parameter-domain scaling.
- Added undo and redo for seam-alignment edits.
- Replaced metadata-only native loading with complete curve and surface knot/control-net round trips and structural validation.
- Added regression coverage for known analytical derivatives, G2 alignment, undo/redo, persisted aligned surfaces, and complete curve/surface restoration.
