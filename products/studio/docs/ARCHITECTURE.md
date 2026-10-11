# MyDesk Studio Architecture

MyDesk Studio is an independently built, modifier-stack-oriented 3D application targeting selected
3ds Max workflows. `StudioDocument` owns procedural primitive parameters, stable object/modifier IDs,
ordered non-destructive modifier stacks, evaluated triangle meshes, and PBR material records.

The standalone Qt desktop application owns no duplicate scene model. Its scene explorer, command
panel, primitive editor, modifier stack, transform editor, and viewport all mutate or inspect the same
`StudioDocument`. The custom viewport draws evaluated meshes with orbit, pan, zoom, frame-all, and
object hit selection. It is intentionally a CPU projection today; it is not represented as a production
GPU renderer.

## Evaluation and transactions

Box, cylinder, and UV-sphere primitives regenerate from parameters. Bend, taper, twist, and midpoint
subdivision modifiers evaluate in stack order. Modifier parameters, enabled state, insertion order,
removal, primitive parameters, persistent object transforms, object creation/removal, materials, and
scene naming participate in the product-local undo/redo history. Evaluated meshes are derived data and
are regenerated after undo, redo, and project loading.

## Persistence and exchange

Native `.mystudio` schema version 3 uses structural JSON parsing. It preserves all supported objects,
primitive parameters, translation/XYZ rotation/per-axis scale, modifier order/parameters/enabled state,
materials, stable IDs, and monotonic ID allocators. Schema versions 1 and 2 are migrated on read. Unknown
future schema versions, duplicate IDs, malformed JSON, invalid enum values, non-finite values, and zero
scale components are rejected. File saves validate a same-directory temporary document before replacement
and retain a `.bak` copy of the prior file. POSIX replacement is atomic; the portable fallback may briefly
remove the destination after its backup exists.

Wavefront OBJ export writes the evaluated mesh for a selected object. OBJ import, material export,
textures, normals, smoothing groups, hierarchy, animation, rendering, USD, and proprietary 3ds Max formats
are not yet implemented.

## Application boundary

The application is now a persistent Qt workspace connected to the real document. It supports new/open/save,
primitive creation and parameter editing, numerical transforms, modifier-stack insertion, ordering,
enable/disable, parameter editing and removal, scene-tree and viewport selection, undo/redo, orbit/pan/zoom,
frame-all, and selected-object OBJ export. It retains window/dock preferences and protects unsaved work.

This is a substantive desktop editing slice, not complete professional parity. A GPU viewport, transform
gizmos and snapping, sub-object selection/editing, object hierarchy, material editor, UV tools, animation
timeline/controllers, renderer, plug-in system, and broad interchange remain required.

The capability manifest records only these tested subsets as `FUNCTIONAL`; it does not claim complete
3ds Max parity.
