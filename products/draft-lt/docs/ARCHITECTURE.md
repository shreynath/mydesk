# MyDesk Draft LT — Architecture

MyDesk Draft LT is an independently installable, 2D-only drafting application. It targets AutoCAD LT workflows without launching or embedding MyDesk Draft and has its own executable, bundle identity, settings namespace, tests, and release version.

## Desktop workspace

`MyDeskDraftLT` is a persistent Qt 6 application with:

- a model-space viewport with layer-resolved color, linetype, and lineweight rendering;
- mouse selection, crossing/window selection, wheel zoom, middle-button pan, and fit-to-drawing;
- interactive LINE, CIRCLE, PLINE, RECTANGLE, and DIMLINEAR point entry;
- a layer manager for current-layer selection plus visibility and lock editing;
- selection properties for layer, linetype, and lineweight edits;
- command history and direct access to the drafting command engine;
- native New/Open/Save/Save As and partial DXF R12 export; and
- document and viewport state that remain live until the user closes the window.

The UI is implemented under `products/draft-lt/app`; it is not a generic suite shell. `QSettings` stores Draft LT window geometry and dock layout separately from other products.

## Document and interaction model

The product uses the shared clean-room 2D document libraries in `core/` and the drafting command implementation in `modules/draft`. `commands::Session` owns the active document and interactive command state. Creation, erase, move, copy, rotation, scale, and entity property setters write document transactions, so the exposed Undo and Redo actions operate on actual editable geometry rather than UI snapshots.

Native `.mydesk` files preserve editable layers, entities, layouts, blocks, units, and properties. The desktop workflow test creates and edits geometry through `MainWindow`, saves it, reopens it in a fresh window, and exports DXF.

Named `LAYERSTATE` / `LAS` operations capture graphical and plot properties plus the current layer,
restore them through one undoable transaction, and persist them in native schema 7. This subset
does not import/export AutoCAD `.las`, apply selective property masks, store viewport-specific
overrides, or force layers created after capture off during restore.

## Current limitations

This is a functional professional workflow slice, not parity with AutoCAD LT. DXF R12 export supports only the subset implemented by `core/exchange`; DWG is unavailable. Plot UI, complete annotation/style management, external references, constraints, tables, sheet sets, geolocation, dynamic blocks, large-drawing performance qualification, accessibility audit, signed installers, and cross-platform clean-machine verification remain incomplete. 3D solid modeling is intentionally outside the Draft LT product scope, but that scope choice does not make the application complete.

## Verification

- `mydesk_draft_lt_tests`: command engine, layers, native round trip, vector plot, and 3D-command rejection.
- `mydesk_draft_lt_ui_tests`: real Qt workspace construction, command-driven creation, undo/redo, property editing, native save/reopen, and DXF export.
- `MyDeskDraftLT --packaging-smoke`: constructs the production main window and exits without entering the event loop.
