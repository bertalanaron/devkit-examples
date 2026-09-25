#include "../src/runtime.h"

#include <devkit/gfx/compute.h>
#include <gtest/gtest.h>
#include <rfl/yaml.hpp>

#include <cmath>
#include <cstdlib>

namespace {

class GrassTest : public testing::Test {
protected:
    dk::io::Window window;
    RenderContext context;
    dk::io::Frame frame;

    void SetUp() override
    {
        window.config(dk::io::Window::Size(640, 480));
        window.open(1);
        frame.viewport() = dk::gfx::Viewport(glm::ivec2(640, 480));
        context.frame = &frame;
        context.camera.position = {55.f, 38.f, -65.f};
        context.camera.lookat = {0.f, 2.f, 0.f};
        context.camera.asp = 640.f / 480.f;
        dk::gfx::backBuffer().setViewport(frame.viewport());
        context.assets.scan_filesystem();
    }

    void TearDown() override { EXPECT_EQ(glGetError(), GL_NO_ERROR); }

    RenderPass scene()
    {
        return dk::io::assets::Meta(context.assets.root() / "scenes/asteroid_belt.asset.yaml",
            dk::io::assets::Yaml{}).parse<RenderPass>();
    }

    struct FloatUniform : UniformValueBase {
        float value;
        explicit FloatUniform(float v) : value(v) {}
        void bind_to(const std::string& name, dk::gfx::Shader& shader) const override
        { shader.uniforms().set(name, value); }
        void imgui_edit(const char*) override {}
    };

    void set(const std::string& name, float value)
    { context.gui_uniform_values[name] = std::make_unique<FloatUniform>(value); }

    dk::gfx::DrawArraysIndirectCommand command(RenderPass& pass)
    {
        dk::gfx::memoryBarrier(dk::gfx::Barrier::BufferUpdate);
        auto& buffer = pass.runtime.get()->buffer("grass_commands");
        glBindBuffer(GL_COPY_READ_BUFFER, buffer.handle());
        dk::gfx::DrawArraysIndirectCommand result{};
        glGetBufferSubData(GL_COPY_READ_BUFFER, 0, sizeof(result), &result);
        return result;
    }

    std::vector<glm::vec4> roots(RenderPass& pass)
    {
        const auto count = command(pass).instanceCount;
        auto& buffer = pass.runtime.get()->buffer("grass_instances");
        if (count > buffer.sizeBytes() / sizeof(glm::vec4))
            throw std::runtime_error("grass compute overflowed its instance buffer");
        std::vector<glm::vec4> result(count);
        glBindBuffer(GL_COPY_READ_BUFFER, buffer.handle());
        if (count) glGetBufferSubData(GL_COPY_READ_BUFFER, 0, count * sizeof(glm::vec4), result.data());
        return result;
    }

    static void compute_only(RenderPass& pass)
    {
        std::erase_if(pass.draw_calls, [](const auto& command) {
            return rfl::visit([](const auto& call) {
                using T = std::remove_cvref_t<decltype(call)>;
                return !std::same_as<T, draw_calls::ComputeDispatch> && !std::same_as<T, draw_calls::MemoryBarrier>;
            }, command);
        });
    }

    static std::vector<unsigned char> pixels(RenderPass& pass)
    {
        auto& texture = pass.runtime.get()->frame_buffer(std::string("resolve_buffer"))
            .color[0].get<dk::gfx::Texture2D>();
        std::vector<unsigned char> result(texture.size().x * texture.size().y * 4);
        glBindTexture(GL_TEXTURE_2D, texture.handle());
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
        return result;
    }
};

TEST_F(GrassTest, GeneratedRootsMatchHeightAndGrassMaterial)
{
    auto pass = scene();
    compute_only(pass);
    set("u_materialNoiseAmount", 0.f);
    pass.execute(context);
    const auto generated = roots(pass);
    ASSERT_GT(generated.size(), 0u);
    EXPECT_LE(generated.size(), 512u * 512u);
    const auto draw = command(pass);
    EXPECT_EQ(draw.count, 6u);
    EXPECT_EQ(draw.first, 0u);
    EXPECT_EQ(draw.baseInstance, 0u);

    auto& height = pass.runtime.get()->texture2d("height_map");
    std::vector<float> heights(height.size().x * height.size().y);
    glBindTexture(GL_TEXTURE_2D, height.handle());
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, heights.data());
    const auto sample = [&](glm::vec2 uv) {
        const auto p = glm::clamp(uv * glm::vec2(height.size()) - 0.5f,
            glm::vec2(0), glm::vec2(height.size() - 1));
        const auto lo = glm::ivec2(glm::floor(p));
        const auto hi = glm::min(lo + 1, height.size() - 1);
        const auto f = glm::fract(p);
        const auto at = [&](int x, int y) { return heights[y * height.size().x + x]; };
        return 80.f * glm::mix(glm::mix(at(lo.x, lo.y), at(hi.x, lo.y), f.x),
            glm::mix(at(lo.x, hi.y), at(hi.x, hi.y), f.x), f.y);
    };
    for (const auto& root : generated) {
        const glm::vec2 uv(root.x / 244.f + 0.5f, root.z / 244.f + 0.5f);
        ASSERT_NEAR(root.y, sample(uv) - 20.f, 0.003f);
        ASSERT_GT(root.y, 0.f);
        ASSERT_GE(root.w, 0.7f);
        ASSERT_LE(root.w, 1.3f);
        const auto texel = 1.f / glm::vec2(height.size());
        const float dx = sample(uv + glm::vec2(texel.x, 0)) - sample(uv - glm::vec2(texel.x, 0));
        const float dz = sample(uv + glm::vec2(0, texel.y)) - sample(uv - glm::vec2(0, texel.y));
        const auto normal = glm::normalize(glm::cross(
            glm::vec3(0, dz, 2 * texel.y * 244), glm::vec3(2 * texel.x * 244, dx, 0)));
        const auto grass = glm::smoothstep(1.75f, 2.25f, root.y);
        const auto notStone = glm::smoothstep(0.825f, 0.875f, normal.y);
        ASSERT_GT(grass * notStone, 0.499f);
    }
    // Height controls reclassify the existing map every frame, without CPU uploads.
    set("u_grassHeight", 80.f);
    pass.execute(context);
    EXPECT_EQ(command(pass).instanceCount, 0u);
}

TEST_F(GrassTest, BoundsCompactionAndReactsToDensityHeightAndSlope)
{
    auto pass = scene();
    compute_only(pass);
    pass.buffers->at("grass_instances").size_bytes = 16 * sizeof(glm::vec4);
    pass.preprocess_draw_calls = std::vector<draw_calls::DrawCall>{draw_calls::Clear{
        .output_buffer = std::string("height_map_buffer"),
        .color = glm::vec4(0.75f), .mask = dk::gfx::Clear::Color}};
    set("u_grassDensity", 1.f);
    set("u_materialNoiseAmount", 0.f);
    pass.execute(context);
    EXPECT_EQ(command(pass).instanceCount, 16u);
    for (const auto& root : roots(pass)) EXPECT_FLOAT_EQ(root.y, 40.f);
    pass.execute(context);
    EXPECT_EQ(command(pass).instanceCount, 16u); // Reset, never accumulate across frames.
    set("u_grassDensity", 0.f);
    pass.execute(context);
    EXPECT_EQ(command(pass).instanceCount, 0u);
    set("u_grassDensity", 1.f);
    set("u_grassHeight", 50.f);
    pass.execute(context);
    EXPECT_EQ(command(pass).instanceCount, 0u);
    set("u_grassHeight", 2.f);

    auto& height = pass.runtime.get()->texture2d("height_map");
    std::vector<float> ramp(height.size().x * height.size().y);
    for (int y = 0; y < height.size().y; ++y)
        for (int x = 0; x < height.size().x; ++x)
            ramp[y * height.size().x + x] = 0.5f + 0.5f * (x + 0.5f) / height.size().x;
    glBindTexture(GL_TEXTURE_2D, height.handle());
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, height.size().x, height.size().y, GL_RED, GL_FLOAT, ramp.data());
    set("u_terrainHeight", 800.f);
    pass.execute(context);
    EXPECT_EQ(command(pass).instanceCount, 0u); // Steep rock, despite positive height.
    set("u_stoneNormalThreshold", 0.1f);
    pass.execute(context);
    EXPECT_EQ(command(pass).instanceCount, 16u);

    // Simulate a render-pass reload: all shader views must bind the new buffers.
    pass.reset_runtime();
    context.updated = true;
    pass.execute(context);
    EXPECT_EQ(command(pass).instanceCount, 16u);
}

TEST_F(GrassTest, RendersBillboardsInTheOpaquePass)
{
    for (auto&& [path, asset] : context.assets.all()) asset.execute_pending_task();
    auto pass = scene();
    // Keep software tessellation inexpensive; llvmpipe drops terrain patches
    // at the scene's current level 60. The interactive scene retains its setting.
    set("u_terrainTessLevel", 8.f);
    set("u_grassDensity", 0.f);
    pass.execute(context);
    const auto bare = pixels(pass);
    EXPECT_EQ(command(pass).instanceCount, 0u);
    set("u_grassDensity", 1.f);
    pass.execute(context);
    const auto grassy = pixels(pass);
    ASSERT_GT(command(pass).instanceCount, 0u);
    std::size_t changed = 0;
    for (std::size_t i = 0; i < bare.size(); i += 4)
        if (std::abs(int(bare[i]) - int(grassy[i])) > 2 ||
            std::abs(int(bare[i + 1]) - int(grassy[i + 1])) > 2 ||
            std::abs(int(bare[i + 2]) - int(grassy[i + 2])) > 2) ++changed;
    EXPECT_GT(changed, 100u);
    // Optional artifact for manual review; tests do not write images by default.
    if (const auto* capture = std::getenv("DEVKIT_GRASS_CAPTURE"))
        dk::gfx::backBuffer().saveAsPNG(capture);
}

} // namespace
