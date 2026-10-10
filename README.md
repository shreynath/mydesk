# MyDesk

**MyDesk** is an open-source, offline-first engineering and design platform — a clean-room suite aimed at Autodesk-class CAD/BIM/CAM/CAE workflows, built as one shared engine with modular domain apps.

> Independent project. Not affiliated with, sponsored by, or endorsed by Autodesk Inc. Autodesk product names appear only for nominative workflow comparison.

Inspired by the depth and honesty culture of [ArtCraft / storytold](https://github.com/storytold) open creative tools (PhotoCraft, VectorCraft, …), MyDesk applies the same discipline — engine-first commands, compatibility matrices, and no pretend parity — to the engineering desk.

## What’s real today (v0.1+)

| Capability | Status |
|---|---|
| Cross-platform CMake monorepo (C++20) | **working** |
| Document model + transactional undo/redo | **working** |
| Draft commands: LINE, CIRCLE, PLINE, RECTANG, MOVE/COPY/ROTATE/SCALE, TRIM/EXTEND/OFFSET/FILLET/CHAMFER | **working** (tested) |
| Window/crossing selection, Shift toggle, hover, F3/F7/F8/F9 | **working** |
| Qt 6 Draft workspace | **working** (macOS verified here) |
| Native `.mydesk` v2 round-trip | **working** |
| DXF import/export (LINE/CIRCLE/ARC/LWPOLYLINE/LAYER) | **partial** — reports warn |
| DWG / RVT / Fusion native / ObjectARX | **not implemented** |
| Mechanical / BIM / Civil / CAM modules | **planned** |

Authoritative status: [`docs/FEATURE_MATRIX.md`](docs/FEATURE_MATRIX.md) and [`docs/COMPATIBILITY_MATRIX.md`](docs/COMPATIBILITY_MATRIX.md).

## Product family (planned modules)

| Module | Autodesk-class workflow |
|---|---|
| **Draft** | AutoCAD-class 2D precision drafting |
| **Mechanical** | Fusion / Inventor parametric design |
| **Buildings** | Revit-class BIM |
| **Civil** | Civil 3D / infra |
| **Manufacture** | CAM toolpaths |
| **Analyze** | Simulation (open solvers) |
| **Coordinate** | Federated model review |
| **Create** | DCC bridges (prefer Blender interop) |
| **Electronics** | ECAD–MCAD (KiCad-oriented) |
| **Data** | Project history / collaboration |

## Build

### Requirements

- CMake ≥ 3.24, Ninja, C++20 compiler
- Qt 6 (Widgets) for the desktop app
- Optional later: Open CASCADE Technology for B-rep

### macOS (Homebrew)

```bash
brew install cmake ninja qt@6
cmake --preset default
cmake --build --preset default
ctest --preset default
open build/default/apps/desktop/MyDesk.app   # or run the MyDesk binary produced by the build
```

### Headless (tests only, no Qt UI)

```bash
cmake --preset headless
cmake --build --preset headless
ctest --preset headless
```

### Quick Draft exercise

1. Launch **MyDesk**.
2. Type `L` then click two points (or `L 0,0 100,50`).
3. Type `C` then center + radius (`C 40,25 15`).
4. `U` / redo; File → Save as `.mydesk`; optional Export DXF.

Sample file: [`samples/draft/hello_line.mydesk`](samples/draft/hello_line.mydesk).

## Repository map

```text
apps/desktop/       Qt shell (MyDesk)
core/               geometry, document, commands, units, exchange
modules/draft/      keymap, snaps (Draft domain)
tests/              unit + integration
docs/               architecture, roadmap, matrices, ADRs
samples/            redistributable drawings
.github/workflows/  CI matrix
```

## Documentation for humans and agents

| Doc | Purpose |
|---|---|
| [`AGENTS.md`](AGENTS.md) | Binding rules for AI agents and contributors |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | Engine layout and dependency rules |
| [`docs/ROADMAP.md`](docs/ROADMAP.md) | Phases and current focus |
| [`docs/FEATURE_MATRIX.md`](docs/FEATURE_MATRIX.md) | What exists |
| [`docs/COMPATIBILITY_MATRIX.md`](docs/COMPATIBILITY_MATRIX.md) | File/command/plugin honesty |
| [`docs/PERFORMANCE.md`](docs/PERFORMANCE.md) | Budgets and measured numbers |
| [`docs/legal/THIRD_PARTY.md`](docs/legal/THIRD_PARTY.md) | Dependency licenses |

## License

Original MyDesk code is licensed under the [Mozilla Public License 2.0](LICENSE). Third-party notices live in `docs/legal/`.

## Contributing

Read `AGENTS.md` first. Prefer small vertical slices with tests and matrix updates over broad unfinished surfaces.
