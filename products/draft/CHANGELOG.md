# MyDesk Draft changelog

## 0.1.0 (unreleased)

- Standalone product layout: `products/draft/app`, binary **MyDeskDraft**, `MYDESK_PRODUCT=draft`.
- ADR-0001 / catalog / AGENTS updated for independent products (no suite shell).
- **LAYER** / **LA**: NEW, MAKE, SET, ON, OFF, LOCK, UNLOCK, COLOR, LIST.
- **CHPROP** / **CH**: selection COLOR (hex|BYLAYER) and LAYER.
- **DIMLINEAR** / **DIM**: aligned linear dimensions (native schema v3).
- **OSNAP** / **OS**: END/MID/CEN/INT/PER toggles; intersection + perpendicular snaps.
- **BLOCK** / **B** and **INSERT** / **I**: block definitions + scaled/rotated inserts (native round-trip).
- **TEXT** / **T**: single-line text.
- **HATCH** / **H**: solid fill from selected closed polyline.
- **EXPLODE** / **X**: one-level explode of INSERT (transformed primitives) and POLYLINE (lines). Undo restores the source object.
- Classic coordinate entry on point prompts: `x,y`, `@dx,dy`, `dist<angle`, `@dist<angle`, `#x,y`.
- **LAYER FREEZE/THAW** independent of on/off. **LAYER PLOT/NOPLOT** stores the plot flag. Native schema 4 round-trips those flags. Schema 3 files still load.
- **LAYER LTYPE / LWEIGHT**, **CHPROP LTYPE / LWEIGHT**, **LTSCALE**: Continuous/Dashed/Hidden/Center/Phantom/Dot patterns, mm lineweights, canvas dash display, native schema 5 + DXF layer/entity group 6 linetypes and group 370 lineweights round-trip, DXF `$LTSCALE` header persistence, and undo/redo coverage. Verified parity for `ACAD-LINETYPE` and `ACAD-LINEWEIGHT`.
- **ATTDEF**, **ATTEDIT**, **ATTDISP**: Block attributes definitions with tag, prompt, default value, position, height, rotation, invisible flag; automatic attribute instantiation upon INSERT with default values or per-tag overrides; in-place and ID-based attribute editing (`ATTEDIT`) with full undo/redo transaction history; visibility mode toggle (`ATTDISP` Normal, ON, OFF) respected during canvas rendering; one-level explode converts attribute instances into TextEntity primitives; native schema 6 and DXF BLOCKS/ATTDEF + INSERT ATTRIB/SEQEND round-trip. Verified parity for `ACAD-ATTRIB`.
- **LAYERSTATE** / **LAS**: named full-drawing layer snapshots with case-insensitive SAVE/RESTORE/DELETE/LIST. Restore covers current layer, visibility, freeze, lock, plot, color, linetype, lineweight, transparency, and thickness as one undoable transaction. Native schema 7 persists snapshots while schemas 3–6 remain readable.
- Preset `draft-macos-release` added.
