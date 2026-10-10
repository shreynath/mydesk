# AGENTS.md — MyDesk

MyDesk is an ecosystem of **standalone desktop applications**. Each application is a dedicated, independently installable alternative to one commercial product. Functional parity with that product is the goal. A partial demo is not a finished replacement.

Authoritative decision: [`docs/DECISIONS/ADR-0001-standalone-products.md`](docs/DECISIONS/ADR-0001-standalone-products.md). Target inventory: [`products/catalog.yaml`](products/catalog.yaml). Draft capability evidence: [`compatibility/target-manifests/draft-autocad-2025.json`](compatibility/target-manifests/draft-autocad-2025.json).

The repository layout keeps `products/<id>/` for applications and `core/` for shared libraries. That is the product architecture. Do not rename it into empty `applications/*-alternative` trees.

## Rules

1. MyDesk is an ecosystem of standalone applications.
2. Each application targets one specific commercial product.
3. Functional parity is the goal. `FUNCTIONAL` and `PARTIAL_PARITY` are not complete parity. Only `VERIFIED_PARITY` means the cited behavior matches the reference for that capability.
4. Independent installation is mandatory. One executable, installer, preferences store, docs, tests, and release per product.
5. Shared code must not create shared application dependencies. Libraries may be reused. Domain models and interfaces stay separate.
6. Domain-specific interfaces are mandatory. Do not build a suite launcher, a shared workspace, or tabs that turn Draft into another product.
7. Features must be implemented, not mocked. A menu entry, command name, or dialog is not the feature.
8. Professional workflows take precedence over superficial demonstrations.
9. Every implementation must be tested.
10. Every iteration must advance a real application’s compatibility.
11. Existing functioning code must be preserved.
12. Incomplete functionality must be tracked without lowering the completion target.
13. Each application retains its own roadmap, manifest, and definition of done.
14. Do not replace a requested capability with an unrelated approximation.
15. Continue from the repository state. Do not restart the project plan each session.

Also:

- Do not create product directories that have no code and tests. Catalog rows are targets, not shipped apps.
- Do not copy proprietary source, artwork, trademarks, or DRM bypasses. Clean-room behavioral parity only.
- Do not push, publish, or change remotes without explicit user authorization.
- Original code is MPL-2.0. Third-party notices: `docs/legal/THIRD_PARTY.md`.

## Current reality

Inspect the tree, tests, and manifest before claiming status.

| Item | Status |
|---|---|
| Implemented products | **Draft** (`MyDeskDraft`), **Mechanical** (`MyDeskMechanical`, OCCT), **Buildings** (`MyDeskBuildings`, semantic BIM/IFC) |
| Other catalog products | `catalog-stub` until their engines land |
| Shared libraries | Draft: `core/*`. Mechanical/Buildings own product engines under `products/<id>/engine` |
| Default build | `MYDESK_PRODUCT=draft`; also `mechanical`, `buildings` |

Draft is the AutoCAD-class application. Mechanical, Fusion, Buildings, Civil, Coordinate, Infra, Animate, Studio, Surface, Mill, Simulation, and Vault remain separate future products. Do not merge them into Draft.

## Session protocol

1. Read the Draft capability manifest, failing tests, and the product the user named.
2. Pick the highest-impact unimplemented Draft capability unless the user named another product that already has code.
3. Implement it in the document model, command interaction, persistence, and undo path.
4. Add or extend automated tests. Build Draft. Run `ctest`.
5. Update the capability manifest. `tools/compatibility-runner/check_manifest.py` must pass.
6. Update that product’s changelog. Do not mark a capability `VERIFIED_PARITY` without a test that checks the reference outcome.
7. Report files changed, tests run, build result, remaining differences, and the next capability.

## Draft priority

Continue from the manifest statuses. Next after classic coordinates, one-level explode, and layer freeze/plot: **linetypes and lineweights**, then attributes, layouts/plot, and deeper DXF. DWG stays unsupported until a lawful reader has round-trip evidence.

## Build

```bash
cmake --preset headless
cmake --build --preset headless
ctest --test-dir build/headless --output-on-failure
```

`cmake --preset default` also builds the MyDesk Draft Qt app when Qt 6 is available. Use the preset that matches the host. A green headless test run is not evidence for a platform whose desktop build was not compiled.
