# MyDesk Draft — definition of done

Product version: see `products/draft/VERSION`. Parity detail: `parity/autocad-2025.yaml`.

A release is **not** done because the window opens. Minimum shipping gates for a professional Draft slice:

## Must work

- [x] Create/edit LINE, CIRCLE, PLINE/RECTANG; MOVE/COPY/ROTATE/SCALE; TRIM/EXTEND/OFFSET/FILLET/CHAMFER
- [x] Classic coordinate entry, including relative and polar forms
- [x] Layers: create/set/on/off/freeze/thaw/lock/unlock/plot/color via `LAYER`/`LA`
- [x] Window/crossing selection; F3/F7/F8/F9 modes
- [x] Save/reopen native `.mydesk` schema 7; undo/redo for entity edits and layer-state restores
- [x] BLOCK/INSERT and one-level EXPLODE; aligned DIMLINEAR; single-line TEXT; solid HATCH
- [x] Linetypes, lineweights, and block attributes for the documented subsets
- [x] Named layer-state save/restore/delete/list with native persistence and undo/redo
- [ ] External references
- [ ] Industrial-grade curve edge cases and grips
- [ ] Layouts / paper space / scale-accurate PDF plot
- [ ] DXF round-trip on a documented corpus (beyond LINE/CIRCLE/ARC/LWPOLYLINE/LAYER)
- [ ] Explicit DWG status remains unsupported until round-trip evidence exists
- [ ] Crash journal / autosave recovery
- [ ] Standalone macOS/Windows/Linux installers with GUI smoke in CI
- [ ] Benchmarks on 1k/10k/100k entity fixtures with recorded numbers

## Explicit limitations (current)

- No DWG. DXF is partial (LINE/CIRCLE/ARC/LWPOLYLINE/LAYER).
- No xrefs. Attributes, linetypes, lineweights, and paper-space plotting remain limited to the subsets documented in the capability manifest.
- Layer freeze/plot flags and named layer states persist. Layer-state `.las` interchange, selective masks, and viewport overrides are not implemented.
- Hatch is a solid fill from a closed polyline.
- EXPLODE handles inserts and polylines one level deep.
- Cross-OS GUI CI: see workflow; do not claim platforms not built.
