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
    - Role-based permissions (`Administrator`, `Engineer`, `Viewer`).
    - Immutable audit log tracking check-ins, state transitions, and user modifications.
  - **Persistence**:
    - Native `.myvault` repository catalog and audit trail persistence.
- **Binary**: `MyDeskVault`.
- **Identity**: Bundle `app.mydesk.vault`.
