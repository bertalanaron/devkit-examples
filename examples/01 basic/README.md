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

Broad wind patches vary ripple strength, surface gloss, blue/green tint, and
whitecap coverage. They drift through world space and retain broad foam coverage
as the fine foam pattern fades with distance. Shoreline foam still follows depth.

- `u_windStrength`: variation strength, from 0 (uniform sea state) to 1.
- `u_windPatchSize`: approximate patch size in world units; larger values give
  broader areas of calm and rough water.
- `u_windDriftSpeed`: patch movement in world units per second, independent of
  `u_waveSpeed`. Zero freezes the patches while waves can continue moving.

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

The asteroid-belt scene also grows billboard grass using compute and an indirect
draw. Its named GPU buffers hold up to 512 × 512 grass roots and one 16-byte
`DrawArraysIndirectCommand`. Each frame executes these scene commands after the
terrain and before the water's depth/color snapshots:

1. A storage barrier orders writes after the preceding frame's readers.
2. `grass_reset` resets the indirect command in one compute invocation.
3. A storage barrier makes the reset visible to `grass_place`.
4. `grass_place` samples the persistent height texture, computes world-space
   normals, and appends roots where the grass material has more than 50% weight.
5. Storage and indirect-command barriers publish roots and the instance count.
6. `grass` draws six procedural vertices per instance, with upright billboards,
   cutout blade silhouettes, deterministic variation, and gentle wind movement.

There is no CPU grass readback or per-frame buffer upload. The reset and placement
are separate dispatches, and the shader bounds the number of candidates by the
output capacity. The terrain and compute passes share their model transform via
a YAML anchor. Material classification duplicates the same height/slope/noise
formula in `terrain.asset.yaml` and `grass_place.asset.yaml`; keep those functions
in sync. Classification uses stable world-space noise, while terrain color detail
retains its derivative filtering. Roots below water level are rejected.

`u_grassDensity` controls candidate acceptance (zero removes all grass), and
`u_grassBladeHeight` controls billboard height. The existing terrain height and
material sliders also update placement on the next frame. The grass participates
in opaque depth/color, water refraction, and final depth-based fog.

Render-pass definitions now accept optional named `buffers`, `dispatch`,
`memory_barrier`, and `draw_indirect` commands. Shader assets accept `compute`
sources. The library API and synchronization rules are described in
[compute and indirect drawing](../../devkit/docs/compute_indirect.md).

The grass rendering test uses terrain tessellation level 8. In the offscreen
llvmpipe renderer, the scene's existing level 60 produces missing terrain patches;
the interactive scene retains that setting and still needs hardware validation.

Shore rocks use the same reset/compute/indirect sequence, with a much coarser
96 × 96 candidate grid. Placement accepts seabed between 0.12 and 3.5 units below
sea level and requires dry terrain within eight world units. Broad density noise
leaves whole patches empty; `u_rockDensity` adjusts acceptance within those patches.
`u_rockSize` scales the boulders. Each instance has deterministic shape deformation,
unequal axis scales, yaw, tilt, and stone color variation. The draw shader builds
an 80-triangle solid from a subdivided icosahedron and buries its base in the seabed.
Some tops emerge while smaller or deeper rocks stay submerged. Rocks render before
the opaque depth/color snapshots, so the existing water refraction and shoreline
foam include them. Placement and draw counts stay on the GPU.

```sh
cmake --build build/debug --target devkit_gfx_tests 01_basic_render_pass_tests
SDL_VIDEODRIVER=offscreen ctest --test-dir build/debug -R 'ComputeTest|GrassTest' --output-on-failure
# Optional screenshot from the grass render test:
SDL_VIDEODRIVER=offscreen DEVKIT_GRASS_CAPTURE=/tmp/devkit-grass.png \
  build/debug/examples/01\ basic/01_basic_render_pass_tests \
  --gtest_filter=GrassTest.RendersBillboardsInTheOpaquePass
```
