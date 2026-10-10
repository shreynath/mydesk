# Build instructions

## macOS

```bash
brew install cmake ninja qt@6
export CMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Run the app:

```bash
./build/default/apps/desktop/MyDesk.app/Contents/MacOS/MyDesk
# or
open build/default/apps/desktop/MyDesk.app
```

## Linux (Debian/Ubuntu)

```bash
sudo apt-get install -y cmake ninja-build qt6-base-dev libgl1-mesa-dev
cmake --preset ci-linux
cmake --build --preset ci-linux
ctest --test-dir build/ci --output-on-failure
```

## Windows

Install Visual Studio 2022 Build Tools, CMake, Ninja, and Qt 6. Set `CMAKE_PREFIX_PATH` to the Qt kit. Then:

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
