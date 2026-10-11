# MyDesk Draft LT — Architecture

MyDesk Draft LT is a dedicated, independently installable desktop 2D drafting application targeting the Autodesk AutoCAD LT professional market.

## Scope & Commercial Replacement Focus
- **Target Counterpart**: Autodesk AutoCAD LT (2025/2026).
- **Core Capabilities**:
  - Full professional 2D drafting engine (lines, arcs, circles, polylines, splines, hatches, dimensions).
  - Layer management with ByLayer/ByBlock property inheritance, linetypes, lineweights, and frozen states.
  - DXF and native `.mydesk` round-trip persistence.
  - Command line prompt with standard AutoCAD-compatible aliases (`L`, `C`, `PL`, `LA`, `DLI`, etc.).
  - Layout / Paper-space drafting and vector plotting.
  - Persistent named layer states shared with the Draft core: `LAYERSTATE`/`LAS` captures graphical and plot properties plus the current layer, restores them through undo/redo, and stores them in native schema 7.
  - Explicit omission of 3D solid modeling and advanced ACIS commands to match the AutoCAD LT specification.
- **Binary**: `MyDeskDraftLT`.
- **Identity**: Bundle `app.mydesk.draftlt`.

## Layer-state limits

The implemented layer-state subset stores complete drawing-level snapshots. It does not yet import or export AutoCAD `.las` files, apply selective property masks, store viewport-specific overrides, or force layers created after capture off during restore.
