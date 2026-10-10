# Compatibility Matrix

Last updated: 2026-10-10

## Support levels

| Level | Meaning |
|---|---|
| **native-editable** | Full semantic edit in MyDesk |
| **lossless/round-trip** | Save/reload preserves tested properties |
| **import-only / read-only** | View or reference; edit may lose data |
| **partial** | Subset of objects/versions only |
| **unsupported** | Not available; do not advertise |

## File formats

| Format | Level | Notes |
|---|---|---|
| `.mydesk` | native-editable + round-trip | schemaVersion `2`; LINE/CIRCLE/ARC/PLINE/layers/units |
| `.dxf` import | partial | LINE, CIRCLE, ARC, LWPOLYLINE, LAYER |
| `.dxf` export | partial | same entity set; conversion report warns |
| `.dwg` | unsupported | see `docs/DECISIONS/0004-dwg-strategy.md` |
| `.step` / `.ifc` / `.rvt` | unsupported | later phases |

## Command aliases (`autocad-inspired-v1`)

| Alias | Command | Status |
|---|---|---|
| L | LINE | implemented-tested |
| C | CIRCLE | implemented-tested |
| PL | PLINE | implemented-tested |
| REC | RECTANGLE | implemented-tested |
| M | MOVE | implemented-tested |
| CO / CP | COPY | implemented-tested |
| RO | ROTATE | implemented-tested |
| SC | SCALE | implemented-tested |
| TR | TRIM | implemented-tested |
| EX | EXTEND | implemented-tested |
| O | OFFSET | implemented-tested |
| F | FILLET | implemented-tested |
| CHA | CHAMFER | implemented-tested |
| E | ERASE | partial |
| U | UNDO | implemented-tested |
| LA | LAYER | implemented-tested |
| CH | CHPROP | implemented-tested |
| OS | OSNAP | implemented-tested |
| DIM | DIMLINEAR | implemented-tested |
| H / BH | HATCH | not-implemented |
| B | BLOCK | implemented-tested |
| I | INSERT | implemented-tested |

## Drawing aids

| Key | Behavior | Status |
|---|---|---|
| F3 | Object snap toggle | implemented |
| F7 | Grid display | implemented |
| F8 | Ortho | implemented-tested |
| F9 | Grid snap | implemented |
| Esc | Cancel command / clear selection | implemented |
