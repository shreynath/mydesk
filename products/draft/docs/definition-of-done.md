# MyDesk Draft — definition of done

Product version: see `products/draft/VERSION`. Parity detail: `parity/autocad-2025.yaml`.

A release is **not** done because the window opens. Minimum shipping gates for a professional Draft slice:

## Must work

- [x] Create/edit LINE, CIRCLE, PLINE/RECTANG; MOVE/COPY/ROTATE/SCALE; TRIM/EXTEND/OFFSET/FILLET/CHAMFER
- [x] Layers: create/set/on/off/lock/unlock/color via `LAYER`/`LA`; panel lists layers; invisible layers not drawn/selected
- [x] Window/crossing selection; F3/F7/F8/F9 modes
- [x] Save/reopen native `.mydesk`; undo/redo for entity edits
- [ ] Industrial-grade curve edge cases and grips
- [ ] Dimensions, text, hatch, blocks/attributes, xrefs
- [ ] Layouts / paper space / scale-accurate PDF plot
- [ ] DXF round-trip on a documented corpus (beyond smoke)
- [ ] Explicit DWG status (unsupported until proven)
- [ ] Crash journal / autosave recovery
- [ ] Standalone macOS/Windows/Linux installers with GUI smoke in CI
- [ ] Benchmarks on 1k/10k/100k entity fixtures with recorded numbers

## Explicit limitations (current)

- No DWG; DXF partial (LINE/CIRCLE/ARC/LWPOLYLINE/LAYER).
- No blocks, dims, hatch, xrefs, plot.
- Layer UI is list + command-line; no full layer manager dialog.
- Entity properties panel is summary-only.
- Cross-OS GUI CI: see workflow; do not claim platforms not green.
