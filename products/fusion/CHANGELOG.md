# MyDesk Fusion changelog

## Unreleased

- Replaced scalar-only rectangular extrusions with Open CASCADE B-rep solids.
- Added kernel validity checks, unique topology counts, and kernel-derived volume, surface area, and bounds.
- Regenerated supported push/pull results as valid B-rep bodies with persistent feature references.
- Replaced fixed 50 × 50 CAM coordinates with cutter-compensated raster paths derived from a selected body's actual bounds.
- Separated XY stepover from Z stepdown and reject invalid tool, depth, engagement, or boundary inputs.
- Added regression coverage for topology, mass properties, push/pull regeneration, selected-geometry G-code, independent depth levels, invalid CAM inputs, and native round-trip B-rep regeneration.
