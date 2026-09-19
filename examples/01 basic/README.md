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
height-map gradients provide normals for procedural stone, sand, and grass. `u_terrainHeight`
controls elevation and `u_terrainTessLevel` controls maximum subdivision, with
the same distance falloff controls as water (`u_tessNear` / `u_tessFar`).
The terrain's model transform controls its extent and seabed level. It renders
before water so shoreline foam and depth use the displaced terrain surface.

Terrain materials update live through these GUI uniforms:

- `u_grassHeight`: world-space height where sand transitions to grass (water is Y = 0).
- `u_stoneNormalThreshold`: surfaces whose normal points less toward the sky become
  gray stone. Higher values cover more slopes; 0 corresponds to vertical and 1 to
  horizontal ground. Stone takes precedence over sand and grass.
- `u_materialNoiseAmount`: height variation of the sand/grass boundary, in world
  units. Set it to zero for a uniform height boundary.
- `u_materialNoiseScale`: frequency of that noise; higher values make smaller patches.

The materials use procedural grain, mottling, and stone variation in world space,
with softened transitions and fading of subpixel detail. No image textures or
height-map regeneration are needed to change the material controls.

Water refracts the opaque scene using a color resolve taken before the water
draw. Wave normals bend the view ray using water's refractive index, and the
shader samples the projected background with depth checks and a fade at screen
edges and contact points. Invalid samples fall back toward the original pixel;
offscreen or hidden geometry cannot be refracted by this screen-space effect.
Foam continues to use the original shoreline depth. Transmission is composited
before reflection and foam, with absorption increasing along the underwater path.

- `u_refractionStrength`: bending strength; zero keeps transmission undistorted.
- `u_waterAbsorption`: absorption strength; higher values hide the seabed sooner,
  while zero keeps the transmitted scene color clear.

The color resolve is overwritten after water for the final post-process. Water
outputs alpha 1 because its shader already includes the background color.

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
