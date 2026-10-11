# MyDesk Animate Architecture

MyDesk Animate is an independent Maya-class animation application. `AnimateDocument` owns the scene
hierarchy, stable node IDs, TRS values, per-channel Bezier curves, polygon meshes, optional four-weight
linear-blend skin data, cameras, lights, and timeline. Qt is confined to the product application.

## Desktop workspace

The Qt workspace reads and edits the same `AnimateDocument` used for evaluation and export. The Outliner
shows the node hierarchy; the Channel Box commits transforms; the timeline scrubs evaluated world matrices,
plays the active range, and authors keys; and the viewport projects evaluated mesh triangles with selection,
orbit, pan, zoom, and frame-all. New/Open/Save/Save As, USDA export, delete, undo, and redo are connected to
the document. Window and dock layout are product-local preferences.

The current viewport is a CPU painter projection, not a production GPU viewport. Manipulators, component
selection, polygon modeling tools, UV/material editors, graph-curve tangent editing, constraints, IK, dynamics,
rendering, audio, references, scripting, and plug-ins remain absent.

## Transactions and persistence

Desktop mutations checkpoint schema-v1 native JSON before committing. Undo/redo restores hierarchy, animation,
mesh, rig, camera, light, and timeline state. `.myanimate` persistence validates node IDs, hierarchy references
and cycles, transforms, animation values, mesh indices, skin cardinality, camera ranges, lights, and timeline.
Saves validate the generated payload, write a same-directory temporary file, and retain a `.bak` copy.

USDA export remains a limited clean-room text writer. It emits timeline metadata, root transforms, triangle
meshes, and cameras, but does not preserve full hierarchy, materials, animation samples, skeletons, or USD
composition. The existing `export_alembic_minimal` output is an experimental Ogawa-marked wrapper and is not
claimed as complete Alembic interoperability.
