# MyDesk

**MyDesk** is a family of **standalone**, free and open-source desktop applications for Autodesk-class engineering workflows. Each product installs and ships on its own — there is no suite launcher or mega-workspace.

> Independent project. Not affiliated with, sponsored by, or endorsed by Autodesk Inc. Vendor names appear only for nominative workflow comparison.

Architecture decision: [`docs/DECISIONS/ADR-0001-standalone-products.md`](docs/DECISIONS/ADR-0001-standalone-products.md). Target inventory: [`products/catalog.yaml`](products/catalog.yaml).

## What’s real today

| Capability | Status |
|---|---|
| **MyDesk Draft** (`MyDeskDraft`) | **alpha** — 2D CAD vertical slice |
| Document model + transactional undo/redo | **working** |
| Commands: LINE, CIRCLE, PLINE, RECTANG, MOVE/COPY/ROTATE/SCALE, TRIM/EXTEND/OFFSET/FILLET/CHAMFER, LAYER/LA | **working** (tested) |
| Window/crossing selection, F3/F7/F8/F9 | **working** |
| Native `.mydesk` round-trip | **working** |
| DXF import/export (LINE/CIRCLE/ARC/LWPOLYLINE/LAYER) | **partial** |
| DWG / RVT / ObjectARX / other products | **not implemented** (catalog stubs only) |

Draft status: [`products/draft/docs/`](products/draft/docs/) and [`docs/FEATURE_MATRIX.md`](docs/FEATURE_MATRIX.md).

## Product family (independent apps)

| Product | Target workflow | Maturity |
|---|---|---|
| **MyDesk Draft** | AutoCAD / LT class 2D CAD | alpha |
| MyDesk Mechanical | Inventor class | catalog-stub |
| MyDesk Fusion | Fusion class | catalog-stub |
| MyDesk Buildings | Revit class BIM | catalog-stub |
| MyDesk Civil / Coordinate / Infra / … | See catalog | catalog-stub |

## Build MyDesk Draft

### Requirements

- CMake ≥ 3.24, Ninja, C++20 compiler
- Qt 6 (Widgets) for the desktop app

### macOS (Homebrew)

```bash
brew install cmake ninja qt@6
cmake --preset default
cmake --build --preset default
ctest --preset default
open build/default/products/draft/app/MyDeskDraft.app
```

Release preset: `cmake --preset draft-macos-release && cmake --build --preset draft-macos-release`.

### Headless (engine tests only)

```bash
cmake --preset headless
cmake --build --preset headless
ctest --preset headless
```

### Quick Draft exercise

1. Launch **MyDesk Draft**.
2. `L 0,0 100,50` then `C 40,25 15`.
3. `LAYER NEW Walls` / `LA MAKE Doors` / `LAYER OFF Walls`.
4. File → Save `.mydesk`; optional Export DXF.

Sample: [`samples/draft/hello_line.mydesk`](samples/draft/hello_line.mydesk).

## Repository map

```text
products/draft/app/   MyDesk Draft Qt app (MyDeskDraft)
products/catalog.yaml Standalone product inventory
core/                 Shared geometry, document, commands, exchange
modules/draft/        Draft helpers (keymap, snaps)
tests/                Unit + integration
docs/                 Architecture, ADRs, matrices
```

## License

MPL-2.0 for original code. Notices: `docs/legal/`. Read `AGENTS.md` before contributing.
