# ADR-0001: Standalone MyDesk products (not a suite shell)

## Status

Accepted (2026-10-10). **Supersedes** earlier “one platform / modular domain apps in a shared desktop shell” guidance in legacy AGENTS/ARCHITECTURE/ROADMAP text.

## Context

Prior scaffolding organized MyDesk as a single desktop executable with future domain modules (Draft, Mechanical, Buildings, …). The product philosophy is now explicit: **each Autodesk-class workflow is a separate, independently installable application**. Users download only the product they need. Shared libraries may exist invisibly; a unified launcher/suite UI must not.

## Decision

1. **One product = one executable, installer, VERSION, changelog, docs, CI gate, and preferences store.**
2. **Monorepo allowed; monolith forbidden.** Layout uses `products/<id>/` for shippable apps and `libs/` (currently still named `core/` during migration) for versioned shared infrastructure.
3. **CMake builds a single selected product by default** (`MYDESK_PRODUCT=draft`). Building “everything” is opt-in and never required for release.
4. **No suite home app, account, telemetry, or mandatory cross-product install.**
5. **Catalog `products/catalog.yaml` is the inventory of targets**, not a claim of implementation. Only products with real builds have maturity beyond `catalog-stub`.
6. **Implementation status is evidence-driven.** Every catalog target now has a product-local executable, engine or shared Draft/LT domain stack, and tests, but most remain scaffolding or partial vertical slices. Catalog maturity and capability manifests—not this historical ADR—record current implementation evidence.

## Migration notes

| Legacy | New |
|---|---|
| `apps/desktop` → binary `MyDesk` | `products/draft/app` → binary `MyDeskDraft` |
| `modules/draft` | Product-local helpers under `products/draft/` and/or shared `libs` (see ARCHITECTURE) |
| “modules/mechanical…” umbrella UI | Separate `products/mechanical`, `products/buildings`, and other product-local applications — **not** Draft tabs |
| Shared command registry for all disciplines | Draft-owned commands; other products get their own interaction systems |

Working Draft geometry/commands/tests must keep compiling across the move. Cosmetic-only folder reshuffles that break CI are forbidden.

## Consequences

- Docs and CI must speak of **MyDesk Draft** releases (`draft/vX.Y.Z`), not “MyDesk suite vX”.
- Shared libs need semantic versioning and consumer-agnostic APIs.
- Parity matrices live per product under `products/<id>/docs/parity/`.
