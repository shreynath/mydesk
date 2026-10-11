# MyDesk Studio — Architecture

MyDesk Studio is a modifier-stack-oriented 3D modeling, material authoring, and rendering application targeting the commercial capabilities of Autodesk 3ds Max.

## Scope & Target Functionality
- **Commercial Target**: Autodesk 3ds Max (2025/2026).
- **Core Engine Subsystems**:
  - **Parametric Primitives**:
    - Procedural Box, Cylinder, Sphere, Plane with parameter controls (dimensions and segment divisions).
  - **Modifier Stack Architecture**:
    - Sequential modifier pipeline evaluating from base geometry to top of stack.
    - Standard spatial deformers: `Bend`, `Taper`, `Twist`.
    - Mesh refinement: `TurboSmooth` / subdivision surfaces.
    - Non-destructive stack manipulation: toggle modifier visibility, reorder stack stages, collapse stack to editable mesh.
  - **Materials & Shading**:
    - Physical PBR material representations (BaseColor, Metallic, Roughness, Specular, Emissive).
  - **Lighting & Scene Hierarchy**:
    - Directional, Omni, and Spot illumination sources with decay and photometric lumens.
  - **Interoperability & Persistence**:
    - Native `.mystudio` JSON format.
    - Neutral Wavefront OBJ mesh export and USD interchange.
- **Binary**: `MyDeskStudio`.
- **Identity**: Bundle `app.mydesk.studio`.
