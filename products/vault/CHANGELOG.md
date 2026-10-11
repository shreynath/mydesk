# MyDesk Vault changelog

## 0.2.0

- Replaced the command-line client with a persistent Qt 6 engineering PDM workspace.
- Added an engine-backed repository tree and searchable file table, editable metadata and lifecycle
  properties, version history, recursive BOM and where-used panes, and a complete audit browser.
- Connected add, check-out, check-in, undo check-out, lifecycle transition, rollback, dependency, and
  metadata commands to product-local undo/redo snapshots.
- Added native repository new/open/save/save-as, reopen-last-repository settings, unsaved-change
  protection, atomic BOM CSV and version-content export, and independent runtime deployment.
- Added dependency-cycle rejection, recursive where-used traversal, and recursive BOM quantity
  aggregation with engine and Qt workflow regression coverage.

## Unreleased

- Store immutable version payloads with FIPS-tested SHA-256 hashes and real UTC timestamps.
- Restore historical versions, locks, dependency links, lifecycle state, and audit events from the native repository format.
- Replace substring-based loading with a versioned structural JSON parser and schema validation.
- Encode arbitrary file bytes safely and reject malformed, dangling, or hash-tampered repositories.
- Migrate the earlier metadata-only schema and save through validated temporary files with `.bak` retention.

