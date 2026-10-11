# MyDesk Fusion — Architecture

MyDesk Fusion is an integrated parametric design, direct modeling, CAM, and electronics integration system targeting the commercial capabilities of Autodesk Fusion.

## Scope & Target Functionality
- **Commercial Target**: Autodesk Fusion (current releases).
- **Core Engine Subsystems**:
  - **Parametric History Timeline**:
    - Linear chronological timeline tracking sketch creation, solid extrusions, revolves, and direct-modeling modifications.
    - Timeline rollback and replay / regeneration engine.
    - Feature suppression and modification propagation.
  - **Direct Modeling**:
    - Push/Pull face displacement operations without breaking parametric history.
  - **CAM & Manufacturing**:
    - Integrated toolpath generation, spindle feeds/speeds calculation, and standard RS-274 G-code post-processing.
  - **ECAD/MCAD Interoperability**:
    - Printed circuit board outline modeling, keep-out zones, mounting hole integration, and 3D component packaging.
  - **Persistence & Exchange**:
    - Native editable `.myfusion` JSON format.
    - Neutral 3D export (STL, STEP).
- **Binary**: `MyDeskFusion`.
- **Identity**: Bundle `app.mydesk.fusion`.
