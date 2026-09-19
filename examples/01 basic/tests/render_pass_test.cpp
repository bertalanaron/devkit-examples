#include "../src/runtime.h"

#include <glad/glad.h>
#include <gtest/gtest.h>
#include <rfl/yaml.hpp>

#include <algorithm>
#include <cmath>

namespace {

const std::string pass_yaml = R"(
textures:
  generated:
    type: Texture2DDefinition
    size: [8, 8]
    format: Float32
    channels: R
frame_buffers:
  back_buffer: {}
  source:
    attachments:
      color:
        0: { type: Texture2DReference, name: generated }
  destination:
    size: [8, 8]
    attachments:
      color:
        0: { type: Texture2DDefinition, format: Float32, channels: R }
cameras: []
gui_uniforms: []
draw_calls:
  - type: blit
    input_buffer: source
    output_buffer: destination
    filter: Nearest
)";

const std::string preprocess_yaml = R"(
preprocess_draw_calls:
  - type: clear
    output_buffer: source
    mask: Color
    color: [0.25, 0, 0, 1]
  - type: clear
    output_buffer: source
    mask: Color
    color: [0.75, 0, 0, 1]
)";

std::vector<float> read_texture(dk::gfx::Texture2D& texture)
{
    std::vector<float> pixels(texture.size().x * texture.size().y);
    glBindTexture(GL_TEXTURE_2D, texture.handle());
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, pixels.data());
    return pixels;
}

class RenderPassPreprocess : public testing::Test {
protected:
    // Keep the GL context alive until all assets and runtime resources are destroyed.
    dk::io::Window window;
    RenderContext context;
    dk::io::Frame frame;

    void SetUp() override
    {
        window.config(dk::io::Window::Size(32, 32));
        window.open(1);
        frame.viewport() = dk::gfx::Viewport(glm::ivec2(32));
        context.frame = &frame;
    }

    float output(const RenderPass& pass)
    {
        auto& buffer = pass.runtime.get()->frame_buffer(std::string("destination"));
        return read_texture(buffer.color[0].get<dk::gfx::Texture2D>()).front();
    }
};

TEST(RenderPassParsing, PreprocessingIsOptionalOrEmpty)
{
    const auto omitted = rfl::yaml::read<RenderPass>(pass_yaml);
    ASSERT_TRUE(omitted) << omitted.error().what();
    EXPECT_FALSE(omitted.value().preprocess_draw_calls);
    const auto empty = rfl::yaml::read<RenderPass>(pass_yaml + "\npreprocess_draw_calls: []\n");
    ASSERT_TRUE(empty) << empty.error().what();
    ASSERT_TRUE(empty.value().preprocess_draw_calls);
    EXPECT_TRUE(empty.value().preprocess_draw_calls->empty());
}

TEST_F(RenderPassPreprocess, RunsInOrderBeforeDrawsAndOnlyOnceUntilReset)
{
    auto pass = rfl::yaml::read<RenderPass>(pass_yaml + preprocess_yaml).value();
    pass.execute(context);
    EXPECT_FLOAT_EQ(output(pass), 0.75f);

    // Overwrite preprocessing output: another frame must preserve it while
    // still running the regular blit, including after a window-size change.
    pass.runtime.get()->frame_buffer(std::string("source")).clear(
        dk::gfx::Clear::Color, glm::vec4(0.125f));
    frame.viewport() = dk::gfx::Viewport(glm::ivec2(64));
    pass.execute(context);
    EXPECT_FLOAT_EQ(output(pass), 0.125f);
    EXPECT_EQ(pass.runtime.get()->texture2d("generated").size(), glm::ivec2(8));

    pass.reset_runtime();
    pass.execute(context);
    EXPECT_FLOAT_EQ(output(pass), 0.75f);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(RenderPassPreprocess, FailedPreprocessingCanBeRetried)
{
    auto pass = rfl::yaml::read<RenderPass>(pass_yaml + preprocess_yaml + R"(
  - type: clear
    output_buffer: missing
    mask: Color
)").value();
    EXPECT_THROW(pass.execute(context), std::runtime_error);
    EXPECT_FALSE(pass.runtime.get());
    EXPECT_TRUE(context.updated);

    pass.preprocess_draw_calls->pop_back();
    pass.execute(context);
    EXPECT_FLOAT_EQ(output(pass), 0.75f);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(RenderPassPreprocess, AsteroidBeltGeneratesPersistentHeightMap)
{
    context.assets.scan_filesystem();
    auto pass = dk::io::assets::Meta(
        context.assets.root() / "scenes/asteroid_belt.asset.yaml",
        dk::io::assets::Yaml{}).parse<RenderPass>();
    // Exercise the actual procedural shader without drawing the rest of the scene.
    pass.draw_calls.clear();
    pass.execute(context);
    auto& texture = pass.runtime.get()->texture2d("height_map");
    EXPECT_EQ(texture.size(), glm::ivec2(1024));
    const auto pixels = read_texture(texture);
    EXPECT_TRUE(std::ranges::all_of(pixels, [](float value) {
        return std::isfinite(value) && value >= 0.f && value <= 1.f;
    }));
    const auto [low, high] = std::ranges::minmax_element(pixels);
    EXPECT_GT(*high - *low, 0.1f);
    frame.viewport() = dk::gfx::Viewport(glm::ivec2(64));
    pass.execute(context);
    EXPECT_EQ(read_texture(texture), pixels);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(RenderPassPreprocess, TerrainDisplacesGeometryAndRendersWithWater)
{
    context.assets.scan_filesystem();
    // Loading scenes publishes their mesh sub-assets, as in the main loop.
    for (auto&& [path, asset] : context.assets.all())
        asset.execute_pending_task();
    auto pass = dk::io::assets::Meta(
        context.assets.root() / "scenes/asteroid_belt.asset.yaml",
        dk::io::assets::Yaml{}).parse<RenderPass>();
    context.camera.position = {0.f, 60.f, 0.f};
    context.camera.lookat = {0.f, 0.f, 0.f};
    context.camera.vup = {0.f, 0.f, -1.f};
    context.camera.asp = 1.f;
    frame.viewport() = dk::gfx::Viewport(glm::ivec2(64));
    dk::gfx::backBuffer().setViewport(frame.viewport());

    const auto center_height = [&] {
        auto& depth = pass.runtime.get()->frame_buffer(std::string("scene_depth"))
            .depth.get<dk::gfx::Texture2D>();
        std::vector<float> pixels(depth.size().x * depth.size().y);
        glBindTexture(GL_TEXTURE_2D, depth.handle());
        glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, pixels.data());
        const float value = pixels[(depth.size().y / 2) * depth.size().x + depth.size().x / 2];
        const auto& camera = pass.runtime.get()->camera("default_camera");
        const float distance = 2.f * camera.np * camera.fp /
            (camera.fp + camera.np - (value * 2.f - 1.f) * (camera.fp - camera.np));
        return camera.position.y - distance;
    };

    pass.execute(context);
    // The generated summit is about 37 units high, translated down by 24.
    EXPECT_NEAR(center_height(), 13.f, 0.5f);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);

    // Flatten the map without rebuilding the runtime. Terrain must submerge,
    // leaving the water near sea level as the closest visible surface.
    pass.runtime.get()->frame_buffer(std::string("height_map_buffer"))
        .clear(dk::gfx::Clear::Color, glm::vec4(0.f));
    pass.execute(context);
    EXPECT_NEAR(center_height(), 0.f, 1.f);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(RenderPassPreprocess, WaterRefractionChangesTransmissionAndResizes)
{
    context.assets.scan_filesystem();
    for (auto&& [path, asset] : context.assets.all())
        asset.execute_pending_task();
    auto pass = dk::io::assets::Meta(
        context.assets.root() / "scenes/asteroid_belt.asset.yaml",
        dk::io::assets::Yaml{}).parse<RenderPass>();

    // Use the real pipeline and shaders with a fixed, shallow seabed. This
    // keeps the check independent of edits to the procedural island shape.
    pass.preprocess_draw_calls = std::vector<draw_calls::DrawCall>{draw_calls::Clear{
        .output_buffer = std::string("height_map_buffer"), .mask = dk::gfx::Clear::Color}};
    for (auto& draw_call : pass.draw_calls) {
        rfl::visit([&](auto& call) {
            if constexpr (std::same_as<std::remove_cvref_t<decltype(call)>, draw_calls::SingleMeshDrawCall>) {
                if (call.mesh.source == "models/terrain_grid/meshes/0")
                    call.transforms = rfl::yaml::read<std::vector<primitives::transforms::Transform>>(
                        "- translate: [0, -2, 0]\n- scale: [20, 1, 20]").value();
                else if (call.mesh.source == "models/plane/meshes/0") {
                    call.mesh.source = "models/terrain_grid/meshes/0";
                    call.transforms = rfl::yaml::read<std::vector<primitives::transforms::Transform>>(
                        "- scale: [20, 1, 20]").value();
                }
            }
        }, draw_call);
    }

    struct FloatUniform : UniformValueBase {
        float value;
        explicit FloatUniform(float v) : value(v) {}
        void bind_to(const std::string& name, dk::gfx::Shader& shader) const override
        { shader.uniforms().set(name, value); }
        void imgui_edit(const char*) override {}
    };
    const auto set_float = [&](const std::string& name, float value) {
        context.gui_uniform_values[name] = std::make_unique<FloatUniform>(value);
    };
    set_float("u_terrainTessLevel", 8.f);
    set_float("u_waterAbsorption", 0.3f);
    set_float("u_refractionStrength", 0.f);
    context.camera.position = {0.f, 8.f, -10.f};
    context.camera.lookat = {0.f, 0.f, 0.f};
    context.camera.asp = 1.f;
    frame.viewport() = dk::gfx::Viewport(glm::ivec2(96));
    dk::gfx::backBuffer().setViewport(frame.viewport());

    const auto render = [&] {
        pass.execute(context);
        auto& color = pass.runtime.get()->frame_buffer(std::string("resolve_buffer"))
            .color[0].get<dk::gfx::Texture2D>();
        std::vector<unsigned char> pixels(color.size().x * color.size().y * 4);
        glBindTexture(GL_TEXTURE_2D, color.handle());
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        EXPECT_EQ(glGetError(), GL_NO_ERROR);
        return pixels;
    };

    const auto straight = render();
    set_float("u_refractionStrength", 1.f);
    const auto refracted = render();
    ASSERT_EQ(straight.size(), refracted.size());
    std::size_t changed = 0;
    for (std::size_t i = 0; i < straight.size(); i += 4)
        if (std::abs(int(straight[i]) - int(refracted[i])) > 2 ||
            std::abs(int(straight[i + 1]) - int(refracted[i + 1])) > 2 ||
            std::abs(int(straight[i + 2]) - int(refracted[i + 2])) > 2)
            ++changed;
    EXPECT_GT(changed, 100u);

    // The opaque snapshot must be refreshed every frame, without feedback
    // from the previous water draw or a second blend with the background.
    set_float("u_refractionStrength", 0.f);
    EXPECT_EQ(render(), straight);
    frame.viewport() = dk::gfx::Viewport(glm::ivec2(128));
    dk::gfx::backBuffer().setViewport(frame.viewport());
    set_float("u_refractionStrength", 1.f);
    EXPECT_EQ(render().size(), 128u * 128u * 4u);
}

} // namespace
