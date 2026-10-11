# MyDesk Draft LT changelog

## 0.1.0 (unreleased)

- Added **LAYERSTATE** / **LAS** SAVE, RESTORE, DELETE, and LIST using the shared Draft document engine.
- Layer-state restore includes current layer, visibility, freeze, lock, plot, color, linetype, lineweight, transparency, and thickness, and is undoable/redoable as one transaction.
- Native `.mydesk` schema 7 persists named layer states; schemas 3–6 remain readable.
