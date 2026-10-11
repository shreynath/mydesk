# MyDesk Studio Changelog

## 0.3.0

- Replaced the command-line demonstration entry point with a persistent Qt desktop application.
- Added an engine-connected shaded mesh viewport with orbit, pan, zoom, frame-all, and object selection.
- Added a scene explorer, primitive creation and parameter editor, numerical transform editor, and
  functioning modifier-stack command panel.
- Connected new/open/save/save-as, selected-object OBJ export, delete, undo, and redo to the document.
- Added persistent window/dock/theme preferences and unsaved-change protection.
- Added translation, XYZ rotation, and per-axis scale evaluation, undo/redo, schema-v3 persistence, and
  schema-v1/v2 migration.
- Added offscreen desktop workflow coverage for creation, transform editing, modifier insertion, scene
  explorer synchronization, and undo/redo.

## 0.2.0

- Added transactional undo/redo for scene, primitive, object, material, and modifier-stack edits.
- Added modifier parameter editing, stack reordering/removal, object deletion, and primitive editing.
- Corrected TurboSmooth iteration handling and removed reference invalidation during subdivision.
- Replaced substring-based persistence with validated schema-v2 structural JSON and schema-v1 migration.
- Added complete supported-material persistence, stable allocator restoration, validated staged saves,
  prior-file backups, and native file loading.
- Connected new/open/save, undo/redo, and stack-edit operations to the interactive application.
- Added regression coverage for stack transactions, malformed input, complete round-trips, IDs, materials,
  backups, and multi-iteration subdivision.
