# MyDesk Vault — Architecture

MyDesk Vault is a dedicated Engineering Product Data Management (PDM) and revision control system targeting the commercial capabilities of Autodesk Vault.

## Scope & Target Functionality
- **Commercial Target**: Autodesk Vault (2025/2026).
- **Core Engine Subsystems**:
  - **Content-Addressable Repository**:
    - File identity tracking with cryptographic SHA-256 content hashing.
    - Check-out / Check-in file locking semantics to prevent concurrent editing collisions.
    - Historical versioning and release revision management (Rev A, B, C...).
  - **Engineering Lifecycle Workflows**:
    - Lifecycle states: `WorkInProgress`, `ForReview`, `Released`, `Obsolete`.
    - Automated transition enforcement (Released revisions become read-only).
  - **CAD Relationship & Dependency Graph**:
    - Multi-level assembly tree relationship modeling (Parent assemblies reference sub-assemblies and component parts).
    - Where-Used dependency queries (locate all assemblies containing a part).
    - Multi-level hierarchical Bill of Materials (BOM) extraction.
  - **Security & Audit Trails**:
    - Audit log tracking check-ins, state transitions, and user modifications.
  - **Persistence**:
    - Versioned native `.myvault` schema parsed structurally with JSON for Modern C++.
    - Base64-safe binary payload persistence with SHA-256 verification on load.
    - Complete version, lock, dependency, and audit-trail round trips plus schema-v1 migration.
    - Validated same-directory temporary writes with atomic rename and prior-version `.bak` recovery files.
- **Desktop application**:
  - Persistent Qt 6 repository workspace with a vault hierarchy, searchable engineering-file grid,
    version history, editable metadata and lifecycle controls, recursive BOM and where-used panes, and
    a complete audit trail.
  - Add, check-out/check-in, undo check-out, rollback, dependency editing, lifecycle transitions,
    native repository persistence, BOM CSV export, and version retrieval call the same
    `VaultRepository` used by the engine tests.
  - Product-local UI history snapshots make successful repository mutations undoable and redoable;
    unsuccessful lock, lifecycle, and graph operations do not enter history.
- **Binary**: `MyDeskVault`.
- **Identity**: Bundle `app.mydesk.vault`.

## Current limitations

- Authentication, users, roles, authorization, approval workflows, database transactions, multi-user networking, and synchronization are not implemented.
- Native saves use validated same-directory replacement and backups, but do not yet issue platform-specific `fsync` calls or preserve unknown schema fields.
- Content currently lives in the repository document rather than an external deduplicated content-addressed blob store.
- The current desktop is a single-user local repository client. It does not provide server-backed
  optimistic transactions, offline replication, conflict resolution, job processing, CAD add-ins,
  thumbnail generation, or commercial Vault file-format interoperability.
