#pragma once

#include "draw_calls.h"
#include "render_context.h"

#include <devkit/gfx/camera.h>
#include <devkit/gfx/frame_buffer.h>
#include <devkit/gfx/texture.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>

class Runtime {
public:
	using Texture = std::variant<dk::gfx::Texture2D, dk::gfx::Texture2DArray>;

	Runtime() = default;
	Runtime(const Runtime&) = delete;
	Runtime& operator=(const Runtime&) = delete;

	void initialize(
		const primitives::TextureCollectionDefinition& textures,
		const primitives::FrameBufferCollectionDefinition& frame_buffers,
		const std::vector<primitives::CameraBinding>& cameras,
		const glm::ivec2& default_size);

	void update(
		const primitives::FrameBufferCollectionDefinition& frame_buffers,
		const std::vector<primitives::CameraBinding>& cameras,
		const RenderContext& context,
		const glm::ivec2& window_size);

	dk::gfx::FrameBuffer& frame_buffer(const primitives::FrameBufferReference& reference);
	dk::gfx::FrameBuffer& frame_buffer(const std::string& name);
	dk::gfx::Texture& texture(const primitives::textures::FrameBufferTexture2DReference& reference);
	dk::gfx::Texture& texture(const primitives::textures::FrameBufferTexture2DArrayReference& reference);
	dk::gfx::Texture2D& texture2d(const std::string& name);
	dk::gfx::Texture2DArray& texture2d_array(const std::string& name);

	dk::gfx::Shader& shader(RenderContext& context, const primitives::ShaderReference& reference) const;

	const dk::gfx::Camera& camera(const std::string& name) const;
	glm::mat4 lightspace_matrix(const std::string& name, std::size_t index) const;

	void reset();

private:
	struct MainCameraRuntime {
		dk::gfx::Camera camera;
	};

	struct CsmSunCameraRuntime {
		dk::gfx::Camera camera;
		std::vector<glm::mat4> lightspace_matrices;
		std::vector<std::unique_ptr<dk::gfx::FrameBuffer>> shadow_buffers;
	};

	using CameraRuntime = std::variant<MainCameraRuntime, CsmSunCameraRuntime>;

	std::vector<std::unique_ptr<dk::gfx::Texture2DArray>> m_frame_buffer_texture_arrays;
	std::unordered_map<std::string, std::unique_ptr<dk::gfx::FrameBuffer>> m_frame_buffers;
	std::unordered_map<std::string, dk::gfx::FrameBuffer*> m_frame_buffer_views;
	std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;
	std::unordered_map<std::string, CameraRuntime> m_cameras;

	void create_texture(const std::string& name, const primitives::textures::TextureDefinition& definition);
	void create_frame_buffer(
		const std::string& name,
		const primitives::FrameBufferDefinition& definition,
		const glm::ivec2& default_size);
	void initialize_cameras(const std::vector<primitives::CameraBinding>& cameras);
	void update_cameras(const std::vector<primitives::CameraBinding>& cameras, const RenderContext& context);

	template <typename T>
	T& texture(const std::string& name)
	{
		const auto found = m_textures.find(name);
		if (found == m_textures.end())
			throw std::runtime_error("unknown texture '" + name + "'");
		return std::get<T>(*found->second);
	}
};

struct RenderPass {
	primitives::TextureCollectionDefinition textures;
	primitives::FrameBufferCollectionDefinition frame_buffers;
	std::vector<primitives::CameraBinding> cameras;
	std::vector<GuiUniform> gui_uniforms;
	std::vector<draw_calls::DrawCall> draw_calls;
	mutable rfl::Skip<std::shared_ptr<Runtime>> runtime;

	void initialize_gui_uniforms(RenderContext& context) const;
	void reset_runtime() const { runtime = nullptr; }
	void drawGui(RenderContext& context) const;
	void execute(RenderContext& context) const;
};
