# Plugin Compatibility

Status: scaffold / planning (2026-10-10)

## A. Native MyDesk plugins (planned)

- Versioned C ABI + higher-level C++/Python bindings
- Manifest schema, permissions, sample plugin under `plugins/`
- No claim of binary portability across OS/arch

## B. Vendor ecosystems

| Ecosystem | Claim |
|---|---|
| ObjectARX | **No** binary compatibility |
| Revit add-ins | **No** binary compatibility |
| AutoCAD .NET | **No** drop-in |
| AutoLISP | Possible future **subset** interpreter with explicit function matrix |
| Fusion Python | Possible future **facade** for documented ops only |

Any future claim requires fixtures, OS matrix rows, and status labels — never marketing language alone.
