# MyDesk Animate Changelog

## 0.2.0

- Replaced the demonstration command-line entry point with a persistent Qt 6 DCC workspace.
- Added an engine-backed scene Outliner, shaded mesh viewport, Channel Box, timeline playback,
  frame scrubbing, dope-sheet key listing, primitive creation, selection, and undo/redo.
- Added numerical transform editing and per-channel keyframe authoring through the real scene graph.
- Added validated schema-v1 `.myanimate` persistence for hierarchy, transforms, animation curves,
  meshes, skin weights, skeletons, cameras, lights, and timeline settings, with staged saves and backups.
- Added hierarchy-safe reparent/delete operations and procedural box/sphere mesh creation.
- Corrected first-key initialization so a newly animated channel retains its timeline-start value.
- Added engine and offscreen Qt regressions for editing, persistence, hierarchy safety, primitive
  creation, scene-tree synchronization, Channel Box changes, keyframing, and desktop undo.
