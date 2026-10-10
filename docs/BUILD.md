# Build instructions — MyDesk Draft

Default product is Draft (`MYDESK_PRODUCT=draft`). Other catalog products are not buildable yet.

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
