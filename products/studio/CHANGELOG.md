# MyDesk Studio Changelog

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
