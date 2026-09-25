# devkit examples

The repository contains two examples, both enabled by default:

- [`shader_sandbox`](examples/shader_sandbox/README.md): the shader and render-pass sandbox.
- `rts`: a starting point for an RTS, with the `rts_common` static library shared
  by the `rts_client` and `rts_editor` executables. Both currently open an empty
  window; press Escape to close it.

Use `DEVKIT_EXAMPLES_BUILD_SHADER_SANDBOX` and `DEVKIT_EXAMPLES_BUILD_RTS` to
enable or disable individual examples, or `DEVKIT_EXAMPLES_BUILD_ALL` to build both.

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

To build specific targets:

```bash
cmake --build --preset debug --target shader_sandbox rts_client rts_editor
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
