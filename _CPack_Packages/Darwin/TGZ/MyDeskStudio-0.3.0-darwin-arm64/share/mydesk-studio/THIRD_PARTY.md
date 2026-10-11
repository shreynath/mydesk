# Third-party dependencies

Inventory for MyDesk v0.1. Update when adding packages.

| Dependency | Use | License | Notes |
|---|---|---|---|
| Qt 6 (Widgets, Gui) | Desktop shell | LGPL v3 / commercial | Dynamically linked via system/Homebrew/aqt |
| CMake | Build | BSD-3 | Dev only |
| Ninja | Build | Apache-2.0 | Dev only |
| Apple clang / MSVC / GCC | Toolchain | Vendor | Dev only |
| Open CASCADE Technology | Mechanical and Fusion B-rep modeling and STEP/IGES support | LGPL-2.1 with additional exception | Dynamically linked from the system package |
| JSON for Modern C++ 3.12.0 | Infra, Simulation, Studio, and Vault native-format structural parsing and validation | MIT | Pinned release archive, SHA-256 verified by CMake |

## Planned (not yet linked)

| Dependency | Candidate use | License to verify |
|---|---|---|
| SolveSpace solver pieces | Constraints | GPL — **compatibility review required before link** |

## Assets

No proprietary Autodesk/Adobe fonts, icons, or sample files are shipped. Sample `hello_line.mydesk` is original MyDesk content.
