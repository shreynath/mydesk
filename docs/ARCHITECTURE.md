# MyDesk Architecture

Status: v0.2 (2026-10-10) — aligned with ADR-0001 standalone products.

## Goals

1. **Standalone products.** One executable and release stream per catalog product. No suite shell.
2. **Engine-first, headless-first.** Features reachable from tests without GUI.
3. **Product-owned interaction.** Draft uses a CAD command line; other products get their own UX when built — never force Draft commands onto BIM/animation.
4. **Invisible shared libs.** `core/*` (future `libs/`) stay consumer-agnostic.
5. **Honest interoperability.** Explicit support levels; conversion reports; no silent drops.

## Layout

```text
products/draft/app    Qt shell → binary MyDeskDraft
products/<id>/…       Future standalone apps (catalog-stub until real)
core/commands         Draft command registry + Session (Draft-owned today)
core/exchange         Native .mydesk + partial DXF
core/document         Entities, layers, history
core/modify           TRIM/EXTEND/OFFSET/FILLET/CHAMFER
core/geometry         Vec2, BBox, predicates
core/units            Unit system
modules/draft         Keymap, snaps (migrate into products/draft or libs later)
```

CMake: `MYDESK_PRODUCT=draft` (default). Other product ids fail configure until implemented.

Dependency rule: `core/*` must not include Qt. Desktop UI mutates documents only via `Session` / commands.

## Document model (Draft)

- Stable `EntityId` / `LayerId`
- Entities: LINE, CIRCLE, ARC, POLYLINE
- Layers: name, visible, locked, color; command `LAYER`/`LA`
- `HistoryStack` transactions (add/remove/replace)
- Native schema versioned JSON (`.mydesk`); selection ephemeral

## Future kernels

| Concern | Approach | ADR |
|---|---|---|
| B-rep / STEP | OCCT behind adapter | `0002-occt.md` |
| Sketch constraints | Evaluate SolveSpace / native | TBD |
| BIM | Separate **MyDesk Buildings** product + IFC | ADR-0001 |
| GPU | Qt RHI with software fallback | TBD |

## Avoiding late rewrites

Units/tolerances as data; schema migrations; stable command ids; conversion reports; product-specific plugin SDKs when needed.
