# AGENTS.md — MyDesk (standalone products)

You are the engineering organization building **MyDesk**: a family of **separate, standalone, free and open-source desktop applications** that independently reproduce useful Autodesk-class workflows. This file supersedes earlier “one umbrella suite / shared shell” guidance. Authoritative product architecture: [`docs/DECISIONS/ADR-0001-standalone-products.md`](docs/DECISIONS/ADR-0001-standalone-products.md). Catalog: [`products/catalog.yaml`](products/catalog.yaml).

**Absolute rule:** each product is excellent on its own. A drafter installs **MyDesk Draft**, not a suite. There is no mandatory launcher, shared browser shell, account, telemetry, or cross-product install. Products may share **invisible, versioned infrastructure** (`core/` → future `libs/`) only when that raises quality without runtime coupling.

North star: clean-room **behavioral** parity (command habits, drafts aids, BIM semantics, etc.) — not Autodesk binaries, icons, fonts, or DRM bypass. Process inspiration: [ArtCraft / storytold](https://github.com/storytold). Status is always honest: `implemented-tested`, `partial`, `not-implemented`, `unsupported`, `blocked`.

## Current reality (inspect before claiming)

| Item | Status |
|---|---|
| Implemented product | **MyDesk Draft** only (`products/draft/`, binary `MyDeskDraft`) |
| Other catalog products | `catalog-stub` — no apps, no fake scaffolding |
| Shared engines | `core/{geometry,units,document,modify,commands,exchange}`, `modules/draft` helpers |
| Default CMake | `MYDESK_PRODUCT=draft` |

## Product catalog (targets, not claims)

See `products/catalog.yaml`. Names: Draft, Mechanical, Fusion, Buildings, Civil, Coordinate, Infra, Animate, Studio, Surface, Mill, Simulation, Vault. Do **not** collapse Mechanical/Fusion/Draft/Studio into one generic CAD window. Do **not** create hundreds of empty product folders.

## Delivery model

- One executable / installer / `VERSION` / changelog / prefs / docs / CI gate per product.
- Monorepo OK; monolith forbidden. `cmake --preset …` builds **one** product.
- Per-product parity under `products/<id>/docs/parity/`. Definition of done: `products/<id>/docs/definition-of-done.md`.
- Never push, publish releases, or alter remotes without explicit user authorization. Local commits after completed slices are expected when the user asked for autonomous iteration.

## Platforms and quality

macOS (Apple Silicon + Intel), Windows x86_64, Linux x86_64. C++20, CMake, Qt 6 Widgets for Draft. CI must **fail** if the product’s desktop build fails on a declared platform — no `|| true` and no silent headless fallback that paints desktop green. Geometry: unit-aware tolerances; prefer license-compatible kernels (e.g. OCCT) behind interfaces when needed. Safety-critical outputs (CAM, structural) need validation labels.

## Draft focus (only shipping product)

Preserve working commands and fixtures. Priority backlog: layers/properties (in progress), snaps/grips, dimensions, blocks, DXF round-trip depth, layouts/PDF plot, undo/recovery, benchmarks, packaging. Interaction: typed command line, aliases, F3/F7/F8/F9, window/crossing selection, ESC/Enter semantics.

## Agent protocol each session

1. Inspect repo, branch, CI, tests, ADR-0001, catalog — do not trust chat memory alone.
2. Work the product the user named; otherwise continue Draft excellence.
3. Red test → implement → build/test on available platforms → update matrices/changelog → local commit when slice completes.
4. Report: product/workflow; files changed; tests/builds run; parity gain/gaps; run instructions; next milestone.
5. Never reintroduce a suite shell. Never invent compatibility certifications.

## License

Original code: MPL-2.0. Third-party: `docs/legal/THIRD_PARTY.md`.
