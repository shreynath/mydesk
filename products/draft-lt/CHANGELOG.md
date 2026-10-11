# MyDesk Draft LT changelog

## 0.2.0 — 2026-10-11

- Replaced the demonstration console entry point with an independent Qt 6 drafting application.
- Added a persistent 2D model-space viewport with selection, crossing/window selection, pan, zoom, fit, layer-resolved display, and interactive command point input.
- Added Draft LT-specific layer management, editable selection properties, command history, and drafting/modify toolbars.
- Connected New/Open/Save/Save As and partial DXF export to the existing editable document and exchange engines.
- Exposed command-backed creation, editing, Undo, and Redo from the desktop application.
- Preserved named `LAYERSTATE` / `LAS` save, restore, delete, and list operations, including
  transactional restore and native schema-7 persistence.
- Added UI workflow and production-binary packaging smoke tests.

This release remains partial. It does not provide DWG support, complete DXF coverage, or AutoCAD LT parity.
