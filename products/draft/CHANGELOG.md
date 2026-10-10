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
- Preset `draft-macos-release` added.
