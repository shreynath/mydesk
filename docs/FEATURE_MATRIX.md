# Feature Matrix

Statuses: `implemented-tested` · `partial` · `experimental` · `planned` · `unsupported`

Last updated: 2026-10-10

## MyDesk Draft (standalone product)

| Feature | Status | Evidence |
|---|---|---|
| LINE (`L`) | implemented-tested | `test_commands.cpp` |
| CIRCLE (`C`) | implemented-tested | `test_commands.cpp` |
| PLINE (`PL`) true polyline | implemented-tested | `test_modify.cpp` |
| RECTANGLE (`REC`) closed PLINE | implemented-tested | `test_transforms.cpp` |
| ARC entity (from FILLET) | implemented-tested | `test_modify.cpp` |
| MOVE/COPY/ROTATE/SCALE | implemented-tested | `test_transforms.cpp` |
| TRIM (`TR`) | implemented-tested | `test_modify.cpp` (LINE; simple PLINE) |
| EXTEND (`EX`) | implemented-tested | `test_modify.cpp` |
| OFFSET (`O`) | implemented-tested | `test_modify.cpp` |
| FILLET (`F`) | implemented-tested | `test_modify.cpp` |
| CHAMFER (`CHA`) | implemented-tested | `test_modify.cpp` |
| UNDO / REDO | implemented-tested | `test_document.cpp` |
| ERASE selection (`E`) | partial | selection-only |
| Window / crossing selection | implemented-tested | `test_modify.cpp` + canvas |
| Shift toggle selection | implemented-tested | canvas |
| Hover highlight | partial | UI |
| Object snap end/mid/center | partial | F3; + pline/arc endpoints |
| Grid / ortho / snap toggles | partial | F7/F8/F9 |
| Native `.mydesk` v2 | implemented-tested | roundtrip tests |
| DXF import/export | partial | LINE/CIRCLE/ARC/LWPOLYLINE/LAYER |
| DWG | unsupported | ADR 0004 |
| LAYER (`LA`) NEW/MAKE/SET/ON/OFF/LOCK/UNLOCK/COLOR | implemented-tested | `test_layers.cpp` |
| CHPROP (`CH`) color / layer | implemented-tested | `test_dims_props_snaps.cpp` |
| DIMLINEAR (`DIM`) | implemented-tested | `test_dims_props_snaps.cpp` |
| OSNAP INT/PER (+ END/MID/CEN) | implemented-tested | `test_dims_props_snaps.cpp` |
| Layer panel (list / activate) | partial | UI |
| BLOCK (`B`) / INSERT (`I`) | implemented-tested | `test_blocks.cpp` |
| Hatch / linetypes / attributes | planned | |

## Other catalog products

| Product | Status |
|---|---|
| Mechanical, Fusion, Buildings, Civil, Coordinate, Infra, Animate, Studio, Surface, Mill, Simulation, Vault | catalog-stub — see `products/catalog.yaml` |
