# MyDesk Civil changelog

## 0.2.0

- Replaced the demonstration console entry point with a persistent Qt 6 civil design workspace.
- Added a civil project browser, editable COGO properties, TIN/contour plan viewport, surface
  earthwork analysis, alignment station/offset inquiry, and profile inspection.
- Connected point, surface, alignment, and profile creation to native save/open, LandXML export,
  document-level undo/redo, and viewport selection.
- Replaced the point-order triangle fan with Bowyer-Watson Delaunay triangulation.
- Expanded native JSON persistence to preserve points, TIN definitions, alignment entities,
  profiles, stable identifiers, and next-ID state.
- Added a Qt workflow regression and deployed-application construction smoke test.

## 0.1.0

- Added the first COGO, TIN, alignment, profile, native persistence, and LandXML engine slice.
