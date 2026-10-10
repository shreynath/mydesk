# MyDesk Architecture

Status: v0.1 (2026-10-10)

## Goals

1. **Engine-first, headless-first.** Every feature reachable from tests/CLI without GUI.
2. **Everything is a command.** UI, shortcuts, future automation all dispatch `CommandRegistry` ids.
3. **Typed domain objects.** Do not collapse BIM/assemblies into anonymous meshes.
4. **Clean-room.** Recreate workflows from public specs and observation; never copy proprietary code/assets.
5. **Honest interoperability.** Support levels are explicit; unknown objects preserved or reported — never silently dropped without a conversion report.

## Layering

```text
apps/desktop          Qt shell — thin; no document mutation except via Session/commands
modules/draft         Draft domain helpers (keymap, snaps); future: dims, hatches
core/commands         Registry + Session interaction state
core/exchange         Native .mydesk + DXF export (more formats later)
core/document         Entities, layers, history/transactions
core/geometry         Vec2, BBox, predicates
core/units            Unit system
```

Dependency rule: lower layers must not include Qt or module UI. `core/*` must remain testable headlessly.

## Document model (Draft MVP)

- Stable `EntityId` / `LayerId`
- Entities: `LINE`, `CIRCLE` (more kinds planned)
- `HistoryStack` stores transactions of add/remove/replace ops
- Selection is ephemeral (not persisted)
- Native format schemaVersion `1` JSON (`.mydesk`)

## Command interaction

`Session` owns `Document` + `CommandRegistry` + interactive prompt state. Incomplete `LINE`/`CIRCLE` remain active until points/numbers complete or `ESC` cancels.

## Future kernels

| Concern | Planned approach | ADR |
|---|---|---|
| B-rep / STEP | OCCT behind `core/geometry` adapter | `docs/DECISIONS/0002-occt.md` |
| Sketch constraints | Evaluate SolveSpace / native | TBD |
| BIM | IFC-first semantic model | TBD |
| GPU | Qt RHI / Vulkan-Metal-D3D with software fallback | TBD |

## Avoiding late rewrites

Bake in from day one:

- Units and tolerances as data
- Schema versioning + migration hooks
- Command ids stable and documented
- Conversion reports for every import/export
- Plugin ABI separated from private headers (scaffold later)
