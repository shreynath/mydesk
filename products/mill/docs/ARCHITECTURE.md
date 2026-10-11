# MyDesk Mill Architecture

MyDesk Mill is an independently built PowerMill-class CAM application. Its product-local engine owns
tool definitions, stock, selected machining geometry, operation parameters, calculated toolpaths, and
generic RS-274 output.

## Desktop application and interaction

The Qt 6 desktop target is an independent `MyDeskMill` application rather than a suite launcher. Its
project browser exposes the CAM setup as stock, cutting tools, selected boundaries, and calculated
operations. A target-specific property panel edits stock extents, tool geometry and cutting data, and
operation strategy/depth/step parameters. Applying a tool, stock, or boundary change invalidates the
calculated paths that depend on it instead of leaving stale NC output in the document.

The CPU machining viewport draws the stock box and every generated move. Cutting motion is visually
separated from dashed rapid/retract motion. It supports top and isometric projections, toolpath hit
selection, orbit, pan, zoom, and fit. This viewport is a path inspection view; it does not claim material
removal simulation, holder collision, gouge analysis, or GPU solid rendering.

## Document lifecycle

The engine owns product-local snapshot history for tool, stock, boundary, and operation mutations.
Calculated operations can be edited and regenerated in place while preserving their user-facing names.
The schema-v1 JSON `.mymill` format stores stock, tools, boundaries, and operation parameters; calculated
moves are regenerated and revalidated during open rather than trusted as opaque serialized output.
Saving uses a temporary file and backup/replace sequence. The desktop application supplies New, Open,
Save, Save As, undo/redo, persistent window layout, and an unsaved-change prompt.

## Geometry and strategies

The current clean-room slice is deliberately 2.5D. A machining boundary is a finite, strictly convex
planar polygon inside the stock. The engine computes analytical inward line offsets for cutter-centre
clearance. Raster operations intersect scanlines with that offset polygon; parallel operations generate
successive inward offsets; profile and pencil operations follow the offset perimeter. Drilling consumes
explicit user-selected hole centres instead of inventing a stock-centre hole.

Depth passes begin one configured stepdown below the top and always terminate exactly at the requested
bottom. A calculation is rejected before mutation when the tool, stock, boundary, clearance, depth,
stepover, stepdown, tolerance, or drill centres are invalid.

## Verification and post

Calculated moves are checked for finite coordinates and stock containment. G-code export refuses paths
that fail validation. The generic Fanuc-style post emits metric or unit-converted inch coordinates,
absolute positioning, G0/G1 motion, spindle/tool changes, dwell, coolant-off, and program termination.
It does not present itself as a machine-certified controller post.

## Current boundary

This is a substantive convex 2.5D CAM kernel, not full PowerMill parity. Concave regions, islands,
surface projection, automatic rest-material detection, holder/fixture collision, gouge checking,
multi-axis kinematics, stock-removal simulation, machine-specific posts, direct boundary vertex editing,
and controller certification remain future work. These gaps are recorded as limitations in the
capability manifest.
