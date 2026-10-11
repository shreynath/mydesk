# MyDesk Coordinate Changelog

## 0.2.0

- Added rigid oriented-box geometry with Euler construction and validated orthonormal bases.
- Added sweep-and-prune broad-phase filtering and observable run statistics.
- Added 15-axis SAT hard-clash narrow phase and exact OBB feature-distance clearance checks.
- Suppressed duplicate unordered pairs for overlapping selection sets.
- Preserved stable result IDs, comments, and review dispositions across reruns.
- Added undo/redo for clash review status and comments.
- Upgraded native persistence to roundtrip model sources, elements, oriented geometry, test
  definitions, selection sets, results, and review state.
- Added regression coverage for rotated-element false positives, clearance distance, broad-phase
  reduction, duplicate suppression, review undo/redo, reruns, and persistence.
