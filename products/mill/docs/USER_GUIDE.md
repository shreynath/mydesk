# MyDesk Mill 0.3 User Guide

MyDesk Mill is a standalone 2.5D CAM application. Start a project with the default stock and D10 tool,
or edit them by selecting their entries in **CAM Project** and using **Machining Properties**.

1. Create a rectangular selected region with **Create → Rectangular Boundary**. The current release
   creates an inset convex boundary; direct vertex editing is not yet implemented.
2. Choose **Create → Toolpath Operation**. A validated raster operation is calculated from the selected
   boundary, stock, and first tool.
3. Select the operation to change its strategy, tool, boundary, stepover, stepdown, Z range, tolerance,
   or cut direction. **Apply / Regenerate** replaces its calculated moves using the engine.
4. Inspect paths in top or isometric view. Click a path to select it, drag to orbit, middle-drag to pan,
   and use the wheel to zoom. Cyan lines cut; dashed amber lines are rapid or retract moves.
5. Save the editable project as `.mymill`. Use **File → Export Generic G-code** only after reviewing the
   operation and verifying the output for the intended machine and controller.

The exported program is a clean-room generic Fanuc-style absolute post, not a certified machine post.
Always perform controller-specific validation, workholding review, dry run, and safe prove-out before
running generated NC code on physical equipment.
