# MyDesk Mill Architecture

MyDesk Mill is an independently built PowerMill-class CAM application. Its product-local engine owns
tool definitions, stock, selected machining geometry, operation parameters, calculated toolpaths, and
generic RS-274 output.

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
that fail validation. The generic post emits metric or imperial setup selection, absolute positioning,
G0/G1 motion, spindle/tool changes, dwell, coolant-off, and program termination.

## Current boundary

This is a substantive convex 2.5D CAM kernel, not full PowerMill parity. Concave regions, islands,
surface projection, automatic rest-material detection, holder/fixture collision, gouge checking,
multi-axis kinematics, simulation, machine-specific posts, operation persistence, and undo/redo remain
future work. These gaps are recorded as limitations in the capability manifest.
