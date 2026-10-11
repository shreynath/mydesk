# Standalone packaging

Every CMake build directory selects exactly one product with
`MYDESK_PRODUCT`. Its `package` target therefore creates an isolated artifact;
MyDesk does not produce a suite installer or shared launcher.

## Development packages

Configure and test a product, then run CPack. For example:

```bash
cmake --preset mill
cmake --build --preset mill
ctest --preset mill
cpack --config build/mill/CPackConfig.cmake -G TGZ \
  -B build/mill/packages
```

The archive contains the selected executable, that product's docs and
changelog when present, its `VERSION`, and the project license/notices. The
`packaging.install-contract` CTest installs to a temporary staging prefix and
checks that the expected executable and metadata exist and that no other
MyDesk product executable leaked into the install. Runtime-deployed desktop
products additionally construct their real main window from the installed
location in deterministic smoke mode; on macOS the contract also rejects
Mach-O load paths into Homebrew, MacPorts, `/usr/local`, or a user directory.

Default generators are DMG and TGZ on macOS, ZIP on Windows, and TGZ on other
systems. Override them at configure time with
`-DMYDESK_CPACK_GENERATORS=...`. Package filenames include the product binary,
product version, operating system, and processor.

## Current release boundary

These artifacts are unsigned development packages. Draft, Mechanical,
Buildings, Coordinate, Animate, Studio, Surface, Mill, and Vault now run Qt's
deployment step during install;
on the local macOS configuration this places their linked Qt frameworks and
platform plugins inside each independent application bundle. Mechanical's
local bundle also contains the linked Open CASCADE libraries with relocated
loader paths. This is runtime-bundling evidence for that configuration, not a
clean-machine, signing, or cross-platform certification. Products without a
deployment step can still require system runtimes. CI evidence for one
operating system is not evidence for another.

A platform package may be called a redistributable installer only after all of
these product-specific gates pass on that platform:

1. Runtime dependencies are bundled or declared and resolved by the package
   manager.
2. Installation and first launch pass on a clean machine without a source
   checkout or build toolchain.
3. Upgrade and uninstall preserve or remove the product's independent
   preferences as documented.
4. The package is signed; macOS artifacts are notarized where distribution
   policy requires it.
5. The product's desktop UI and file-open smoke tests pass from the installed
   location.

Until then, publish CPack output only as development artifacts and keep the
limitations above visible in release notes.

## CI coverage

The workflow defines independent configure, build, test, install-contract,
package, checksum, and artifact-upload jobs for every catalog product on
Ubuntu, macOS, and Windows. Qt is installed for Draft, Mechanical, Buildings,
Coordinate, Animate, Studio, Surface, Mill, and Vault. Open CASCADE comes from
distribution packages on Ubuntu, Homebrew on macOS, and vcpkg on Windows for
Mechanical and Fusion.
Each real Qt product also has a non-Windows executable-location smoke step;
its product-specific Qt UI regression remains the substantive interaction
test.

Defining a job is not evidence that a platform passes. Record platform support
only from a completed workflow run, and retain failures rather than describing
an unexecuted matrix as verified. Local macOS results likewise make no claim
about Linux or Windows.
