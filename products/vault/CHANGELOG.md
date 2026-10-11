# MyDesk Vault changelog

## Unreleased

- Store immutable version payloads with FIPS-tested SHA-256 hashes and real UTC timestamps.
- Restore historical versions, locks, dependency links, lifecycle state, and audit events from the native repository format.
- Replace substring-based loading with a versioned structural JSON parser and schema validation.
- Encode arbitrary file bytes safely and reject malformed, dangling, or hash-tampered repositories.
- Migrate the earlier metadata-only schema and save through validated temporary files with `.bak` retention.

