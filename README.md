# devkit examples

## Prerequisites

- CMake 3.21 or newer
- Ninja
- vcpkg

Set `VCPKG_ROOT` to the vcpkg checkout you want CMake to use:

```bash
export VCPKG_ROOT="$HOME/.vcpkg-clion/vcpkg"
```

To make that persistent for new terminals, add the same line to your shell startup file, such as `~/.bashrc`.

## Build From A Shell

Configure and build one of the presets:

```bash
cmake --preset debug
cmake --build --preset debug
```

Other available presets:

```bash
cmake --preset release
cmake --build --preset release

cmake --preset relwithdebinfo
cmake --build --preset relwithdebinfo
```

Each preset uses its own build directory:

- `build/debug`
- `build/release`
- `build/relwithdebinfo`

## Build In CLion

1. Make sure `VCPKG_ROOT` is set in the environment CLion inherits.
2. Reload the CMake project.
3. Select one of the presets: `Debug`, `Release`, or `RelWithDebInfo`.

If CLion was already open before setting `VCPKG_ROOT`, restart CLion so it sees the updated environment.
