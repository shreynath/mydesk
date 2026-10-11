# Build instructions — MyDesk standalone products

Default product is Draft (`MYDESK_PRODUCT=draft`). Each other preset selects
and builds one independent product; see `CMakePresets.json` for the complete
list. Mechanical and Fusion additionally require Open CASCADE.

## macOS

```bash
brew install cmake ninja qt@6
export CMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Run:

```bash
open build/default/products/draft/app/MyDeskDraft.app
# or
./build/default/products/draft/app/MyDeskDraft.app/Contents/MacOS/MyDeskDraft
```

Release:

```bash
cmake --preset draft-macos-release
cmake --build --preset draft-macos-release
```

## Linux (Debian/Ubuntu)

```bash
sudo apt-get install -y cmake ninja-build qt6-base-dev libgl1-mesa-dev
cmake --preset ci-linux
cmake --build --preset ci-linux
ctest --test-dir build/ci --output-on-failure
```

## Windows

Install Visual Studio 2022 Build Tools, CMake, Ninja, and Qt 6. Set `CMAKE_PREFIX_PATH` to the Qt kit:

```bat
cmake --preset ci-windows
cmake --build --preset ci-windows
ctest --test-dir build/ci --output-on-failure
```

## Headless tests only

```bash
cmake --preset headless
cmake --build --preset headless
ctest --preset headless
```

## Standalone development packages

After building and testing a product, use its generated CPack configuration:

```bash
cpack --config build/mill/CPackConfig.cmake -G TGZ -B build/mill/packages
```

The resulting artifact contains only that selected application and its package
metadata. These are unsigned development packages whose external runtimes are
not yet bundled. See [`docs/PACKAGING.md`](PACKAGING.md) for the verified
install contract and the remaining native release gates.
