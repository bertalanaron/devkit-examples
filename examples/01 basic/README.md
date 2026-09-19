# Render passes

Render-pass assets may define `preprocess_draw_calls` alongside `draw_calls`.
It accepts the same draw-call types and bindings, and executes in list order
once when the runtime is first initialized, before the first frame's `draw_calls`.
Textures, framebuffers, GUI uniforms, and cameras are ready before preprocessing.
Omitting the section or using `preprocess_draw_calls: []` keeps the usual behavior.

Preprocessing runs again after the render-pass asset is reloaded. It does not run
on subsequent frames, camera changes, window resizing, or shader-only reloads.
If preprocessing fails, the incomplete runtime is discarded and retried on the
next execution; the regular draw calls do not run until preprocessing succeeds.

The `asteroid_belt` scene generates a 1024 × 1024 single-channel floating-point
`height_map` using `shaders/procedural/height_map`. Values are in the range [0, 1].
The texture is attached to `height_map_buffer` for preprocessing and remains
available to subsequent draws:

```yaml
uniform_textures:
  - uniform: u_heightMap
    name: height_map
    config: { MinFilter: Linear, MagFilter: Linear }
```

The scene renders this map as terrain using `shaders/terrain` and a 16 × 16 grid
of triangle patches. Tessellation samples the map to displace the surface;
height-map gradients provide normals for a plain gray material. `u_terrainHeight`
controls elevation and `u_terrainTessLevel` controls maximum subdivision, with
the same distance falloff controls as water (`u_tessNear` / `u_tessFar`).
The terrain spans 144 × 144 world units, from -24 at the seabed to 13 at the summit
with the default height. It renders before water so shoreline foam and depth
use the displaced terrain surface.

Use a named texture with an explicit size, as in this example, or an explicitly
sized framebuffer attachment for persistent procedural output. Window-sized
attachments are reallocated when the window resizes and lose their contents.
Reload the render-pass asset after changing the generation shader to regenerate
the texture.

The render-pass regression tests require an OpenGL context. On Linux they can run
without a visible window using SDL's offscreen driver:

```sh
cmake --build --preset release --target 01_basic_render_pass_tests
SDL_VIDEODRIVER=offscreen ctest --test-dir build/release -R RenderPass --output-on-failure
```
