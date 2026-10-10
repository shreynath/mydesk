# Gaps (ranked work list)

## P0 — Draft professionalism

1. Linetypes and lineweights on layers and entities, including native and DXF persistence
2. Block attributes
3. Layouts, paper space, viewports, and PDF plot that honors the layer plot flag
4. DXF coverage for dimensions, blocks, text, and hatches
5. Entity grips and multi-segment polyline trim

## P1 — Interop

1. Expand the DXF fixture corpus
2. Lawful DWG library evaluation (ADR 0004). Do not claim DWG until round-trip tests exist.
3. Autosave and drawing recovery

## P2 — Other standalone products (when scheduled)

1. MyDesk Mechanical OCCT sketch/extrude (own app)
2. MyDesk Buildings IFC pilot (own app)

## Closed

| Date | Gap | Evidence |
|---|---|---|
| 2026-10-10 | Phase 0 scaffold | ctest |
| 2026-10-10 | MOVE/COPY/ROTATE/SCALE + aids | ctest |
| 2026-10-10 | PLINE + TRIM/EXTEND/OFFSET/FILLET/CHAMFER | ctest 16/16 |
| 2026-10-10 | Window/crossing selection | ctest + UI |
| 2026-10-10 | DXF import/export partial | ctest |
| 2026-10-10 | Standalone products pivot + LAYER/LA | ctest + ADR-0001 |
| 2026-10-10 | Classic coordinates, EXPLODE, layer freeze/plot | ctest |
