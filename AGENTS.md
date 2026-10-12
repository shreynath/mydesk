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
| Implemented products | All fourteen catalog products have independent engines, persistence, tests, and Qt desktop entry points. Every product remains an alpha functional subset rather than parity. |
| Desktop applications | Draft, Draft LT, Mechanical, Fusion, Buildings, Civil, Infra, Coordinate, Animate, Studio, Surface, Mill, Simulation, Vault |
| Shared libraries | Draft: `core/*`. Domain-specific engines under `products/<id>/engine` |
| Presets | `headless`/`default` (Draft) plus one named preset for each of the other thirteen products |

Draft is the AutoCAD-class application. Mechanical, Fusion, Buildings, Civil, Coordinate, Infra, Animate, Studio, Surface, Mill, Simulation, Vault, and Draft LT are independent applications. Advance all products across the ecosystem. Do not restrict development to Draft.

## Ecosystem development protocol

1. Maintain independent installation, document models, computational engines, and interfaces across all products.
2. For any product being advanced: implement features in its domain model, command/interaction layer, persistence, and undo path.
3. Every capability must have substantive computational code, not empty stubs or mocks.
4. Add or extend automated regression tests. Verify builds and ctest suite runs cleanly.
5. Update capability manifests accurately. Mark `VERIFIED_PARITY` only when tests verify complete reference behavior; record `PARTIAL_PARITY` or `FUNCTIONAL` with exact tested subsets otherwise.
6. Maintain each product's changelog, architecture documentation, and manifest.

## Build

```bash
cmake --preset headless
cmake --build --preset headless
ctest --test-dir build/headless --output-on-failure
```

`cmake --preset default` also builds the MyDesk Draft Qt app when Qt 6 is available. Use the preset that matches the host. A green headless test run is not evidence for a platform whose desktop build was not compiled.
