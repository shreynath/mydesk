# MyDesk Roadmap

## Current focus

**Phase 1 Draft modify commands + Phase 2 selection + Phase 3 DXF partial — landed (2026-10-10).**  
Next: layers commands, dimensions, linetypes, blocks; deepen DXF; evaluate lawful DWG.

## Phases

### Phase 0 — Foundation ✅

- [x] Monorepo + CMake presets
- [x] Core libraries + headless tests
- [x] Qt desktop shell named MyDesk
- [x] CI workflow for macOS/Linux/Windows
- [x] Docs: architecture, matrices, ADRs, legal inventory

### Phase 1 — Core drafting commands ✅ (initial)

- [x] LINE, CIRCLE, RECTANGLE (closed PLINE), PLINE
- [x] MOVE/COPY/ROTATE/SCALE/ERASE
- [x] TRIM, EXTEND, OFFSET, FILLET, CHAMFER
- [x] Undo/redo + tests
- [ ] Multi-segment PLINE trim
- [ ] Fillet/chamfer for polylines

### Phase 2 — Selection system ✅ (initial)

- [x] Window (L→R) / crossing (R→L)
- [x] Click + Shift toggle
- [x] Hover highlight
- [x] ESC clears selection when idle
- [ ] Implied window during TRIM fence (future)

### Phase 3 — File compatibility ✅ (partial DXF)

- [x] `.mydesk` schema v2 (LINE/CIRCLE/ARC/PLINE)
- [x] DXF export/import partial (LINE/CIRCLE/ARC/LWPOLYLINE/LAYER)
- [x] ADR 0004 DWG strategy (no false claims)
- [ ] DXF dimensions/blocks/xrefs
- [ ] Lawful DWG evaluation

### Phase 4 — Professional drafting experience (active next)

- [ ] LAYER command + properties
- [ ] Linetypes, colors per entity
- [ ] Blocks / INSERT
- [ ] Dimensions
- [ ] Advanced snaps (intersection, perp, tangent)

### Phase 5 — Cross-platform engineering (ongoing)

- [x] Portable C++20 core; Qt shell isolated
- [x] CI matrix configured
- [ ] Signed packaging
- [ ] Performance baselines under `benchmarks/`

## Definition of done

Code runs; tests cover success+failure; UI and commands share transactions; undo correct; persistence/round-trip where claimed; matrices updated; CI green on claimed platforms.
