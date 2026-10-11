# MyDesk Surface changelog

## Unreleased

- Replaced the demonstration command-line entry point with a persistent Qt 6 precision-surfacing workspace.
- Added a domain-specific surface browser, object/CV property editor, sampled NURBS 3D viewport,
  control-net rendering, viewport picking, orbit, pan, zoom, curvature readouts, and zebra diagnostics.
- Connected curve, bilinear-patch, bicubic-patch, CV editing, object naming, seam inspection, and
  G0/G1/G2 seam alignment controls directly to the `SurfaceDocument` engine.
- Added atomic `.mysurface` save, reopen, Save As, unsaved-change protection, per-application window
  settings, and an explicitly labeled diagnostic-only IGES preview export.
- Extended document undo/redo from seam alignment to geometry creation, object naming, curve CV edits,
  and surface CV edits using complete document-state checkpoints.
- Added an offscreen Qt workflow regression covering UI creation, CV editing, undo/redo, save, reopen,
  and seam tool connection, plus engine tests for creation and CV edit history.
- Added product-local Qt runtime deployment to installed application packages.
- Added analytical first and second derivatives for rational tensor-product NURBS surfaces.
- Reworked normals, Gaussian curvature, and mean curvature to use exact basis derivatives instead of finite differences.
- Added sampled G2 seam diagnostics using cross-seam normal curvature.
- Added G0, G1, and G2 control-net alignment for adjacent non-rational bicubic Bezier patches, including parameter-domain scaling.
- Added undo and redo for seam-alignment edits.
- Replaced metadata-only native loading with complete curve and surface knot/control-net round trips and structural validation.
- Added regression coverage for known analytical derivatives, G2 alignment, undo/redo, persisted aligned surfaces, and complete curve/surface restoration.
