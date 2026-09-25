#pragma once

#include "primitives.h"
#include "uniforms.h"

#include <devkit/gfx/common.h>
#include <devkit/gfx/frame_buffer.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class RenderContext;
class Runtime;

struct UniformCameraBinding {
	std::string              name;
	std::string              uniform;
	std::vector<std::string> uniform_members;
};

namespace draw_calls {

struct Clear {
	using Tag = rfl::Literal<"clear">;
	primitives::FrameBufferReference output_buffer = std::string("back_buffer");
	std::optional<rfl::Variant<glm::vec3, glm::vec4, std::string>> color;
	dk::gfx::Clear mask = dk::gfx::Clear::Color | dk::gfx::Clear::Depth;

	void execute(RenderContext&, Runtime&) const;
};

struct BlitDrawCall {
	using Tag = rfl::Literal<"blit">;
	primitives::FrameBufferReference output_buffer = std::string("back_buffer");
	primitives::FrameBufferReference input_buffer;
	std::optional<dk::gfx::Mask> mask;
	std::optional<int> input_color_index;
	std::optional<int> output_color_index;
	std::optional<dk::gfx::Texture::MagFilter> filter;
	std::optional<dk::gfx::Rect> src_rect;
	std::optional<dk::gfx::Rect> dst_rect;

	void execute(RenderContext&, Runtime&) const;
};

struct PostProcessDrawCall {
	using Tag = rfl::Literal<"post_process">;
	primitives::FrameBufferReference output_buffer = std::string("back_buffer");
	primitives::ShaderReference shader;
	std::optional<std::vector<primitives::ShaderUniformTexture>> uniform_textures;
	std::optional<std::vector<std::string>> gui_uniforms;
	std::optional<std::vector<UniformCameraBinding>> uniform_cameras;

	void execute(RenderContext&, Runtime&) const;
};

struct MeshBinding {
	std::filesystem::path source;
	dk::gfx::VertexFlags vertices;
};

struct SingleMeshDrawCall {
	using Tag = rfl::Literal<"single_mesh">;
	primitives::FrameBufferReference output_buffer = std::string("back_buffer");
	primitives::ShaderReference shader;
	std::optional<std::vector<primitives::ShaderUniformTexture>> uniform_textures;
	std::optional<std::vector<std::string>> gui_uniforms;
	std::optional<std::vector<UniformCameraBinding>> uniform_cameras;
	std::vector<primitives::transforms::Transform> transforms;
	std::optional<std::string> model_uniform;
	std::optional<std::string> time_uniform;
	std::optional<int> cascade;
	MeshBinding mesh;
	dk::gfx::Primitive gl_primitive = dk::gfx::Primitive::Triangles;

	void execute(RenderContext&, Runtime&) const;
};

struct ComputeDispatch {
	using Tag = rfl::Literal<"dispatch">;
	primitives::ShaderReference shader;
	std::array<std::uint32_t, 3> groups;
	std::optional<std::vector<primitives::ShaderStorageBinding>> storage_buffers;
	std::optional<std::vector<primitives::ShaderUniformTexture>> uniform_textures;
	std::optional<std::vector<std::string>> gui_uniforms;
	std::optional<std::vector<UniformCameraBinding>> uniform_cameras;
	std::optional<std::vector<primitives::transforms::Transform>> transforms;
	std::optional<std::string> model_uniform;
	void execute(RenderContext&, Runtime&) const;
};

struct MemoryBarrier {
	using Tag = rfl::Literal<"memory_barrier">;
	std::vector<dk::gfx::Barrier> barriers;
	void execute(RenderContext&, Runtime&) const;
};

struct IndirectDrawCall {
	using Tag = rfl::Literal<"draw_indirect">;
	primitives::FrameBufferReference output_buffer;
	primitives::ShaderReference shader;
	primitives::BufferReference commands;
	std::optional<std::uint32_t> draw_count;
	std::optional<std::uint32_t> stride_bytes;
	std::optional<MeshBinding> mesh; // Omit for procedural vertices using gl_VertexID.
	std::optional<std::vector<primitives::ShaderStorageBinding>> storage_buffers;
	std::optional<std::vector<primitives::ShaderUniformTexture>> uniform_textures;
	std::optional<std::vector<std::string>> gui_uniforms;
	std::optional<std::vector<UniformCameraBinding>> uniform_cameras;
	std::optional<std::string> time_uniform;
	dk::gfx::Primitive gl_primitive = dk::gfx::Primitive::Triangles;
	void execute(RenderContext&, Runtime&) const;
};

using DrawCall = rfl::TaggedUnion<
	"type", Clear, BlitDrawCall, PostProcessDrawCall, SingleMeshDrawCall,
	ComputeDispatch, MemoryBarrier, IndirectDrawCall>;

} // namespace draw_calls
