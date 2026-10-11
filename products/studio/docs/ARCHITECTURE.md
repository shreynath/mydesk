# MyDesk Studio Architecture

MyDesk Studio is an independently built, modifier-stack-oriented 3D application targeting selected
3ds Max workflows. `StudioDocument` owns procedural primitive parameters, stable object/modifier IDs,
ordered non-destructive modifier stacks, evaluated triangle meshes, and PBR material records.

## Evaluation and transactions

Box, cylinder, and UV-sphere primitives regenerate from parameters. Bend, taper, twist, and midpoint
subdivision modifiers evaluate in stack order. Modifier parameters, enabled state, insertion order,
removal, primitive parameters, object creation/removal, materials, and scene naming participate in the
product-local undo/redo history. Evaluated meshes are derived data and are regenerated after undo,
redo, and project loading.

## Persistence and exchange

Native `.mystudio` schema version 2 uses structural JSON parsing. It preserves all supported objects,
primitive parameters, modifier order/parameters/enabled state, materials, stable IDs, and monotonic ID
allocators. Schema version 1 documents are migrated on read. Unknown future schema versions, duplicate
IDs, malformed JSON, invalid enum values, and non-finite parameters are rejected. File saves validate a
same-directory temporary document before replacement and retain a `.bak` copy of the prior file. POSIX
replacement is atomic; the portable fallback may briefly remove the destination after its backup exists.

Wavefront OBJ export writes the evaluated mesh for a selected object. OBJ import, material export,
textures, normals, smoothing groups, transforms, hierarchy, animation, rendering, USD, and proprietary
3ds Max formats are not yet implemented.

## Application boundary

The current application is an interactive command interface connected to the real document and now
supports new/open/save, modeling edits, modifier-stack edits, undo/redo, material creation, inspection,
and OBJ export. A professional Qt viewport, modifier command panel, scene explorer, selection/manipulator
system, material editor, animation tools, and renderer remain required before desktop acceptance.

The capability manifest records only these tested subsets as `FUNCTIONAL`; it does not claim complete
3ds Max parity.
