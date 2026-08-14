#pragma once

#include "render_context.h"

#include <devkit/gfx/frame_buffer.h>
#include <devkit/gfx/scene.h>
#include <devkit/gfx/shader.h>

#include <array>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

template <std::size_t N>
using FloatArray = std::array<float, N>;

inline glm::vec4 toVec4(const FloatArray<4>& value)
{
	return { value[0], value[1], value[2], value[3] };
}

struct Transform {
	struct Rotation {
		glm::vec3 axis;
		float     angle;
	};

	std::optional<glm::vec3> position;
	std::optional<glm::vec3> scale;
	std::optional<Rotation>  rotation;

	operator glm::mat4() const
	{
		const auto mScale = glm::scale(scale.value_or(glm::vec3(1)));
		const auto mRotation = rotation.has_value()
			? glm::rotate(glm::mat4(1), rotation.value().angle, rotation.value().axis)
			: glm::identity<glm::mat4>();
		const auto mTranslation = glm::translate(position.value_or(glm::vec3(0)));
		return mRotation * mScale * mTranslation;
	}
};

struct UniformTexture {
	rfl::Rename<"uniform", std::string>           m_uniform;
	rfl::Rename<"texture", std::filesystem::path> m_texture;

	void bind_to_shader(dk::gfx::Shader& shader, RenderContext& ctx) const
	{
		auto& texture = ctx.assets[m_texture.get().string()].as<dk::gfx::Texture2D>();
		shader.uniformTexture(m_uniform.get(), texture);
	}
};

struct ShaderBinding {
	rfl::Rename<"program", std::filesystem::path>                m_program;
	rfl::Rename<"uniforms", std::vector<std::string>>            m_uniforms;
	rfl::Rename<"uniform_textures", std::vector<UniformTexture>> m_uniform_textures;

	void set_uniforms(RenderContext& ctx) const
	{
		auto& program = ctx.assets[m_program.get()].as<dk::gfx::Shader>();

		for (const auto& uniform : m_uniforms.get())
			ctx.uniform(uniform).bind_to(uniform, program);

		for (const auto& uniform_texture : m_uniform_textures.get())
			uniform_texture.bind_to_shader(program, ctx);
	}
};

struct MeshBinding {
	std::filesystem::path source;
	dk::gfx::VertexFlags  vertices;
};

struct ClearDrawCall {
	using Tag = rfl::Literal<"clear">;

	std::string              output_buffer = "back_buffer";
	dk::gfx::Clear           mask = dk::gfx::Clear::Color | dk::gfx::Clear::Depth;
	std::optional<glm::vec4> color;

	void execute(RenderContext&) const
	{
		if (output_buffer != "back_buffer")
			throw std::runtime_error("only back_buffer is supported as clear output_buffer for now");
		dk::gfx::backBuffer().clear(mask, color.value_or(dk::colors::black));
	}
};

struct SingleMeshDrawCall {
	using Tag = rfl::Literal<"single_mesh">;

	std::string              output_buffer = "back_buffer";
	ShaderBinding            shader;
	MeshBinding              mesh;
	std::optional<Transform> transform;

	void execute(RenderContext& ctx) const
	{
		if (output_buffer != "back_buffer")
			throw std::runtime_error("only back_buffer is supported as single_mesh output_buffer for now");

		auto& program = ctx.assets[shader.m_program.get()].as<dk::gfx::Shader>();
		auto& mesh_factory = ctx.assets[mesh.source].as<dk::gfx::Scene::MeshFactory>();
		auto& mesh_mask = mesh_factory(mesh.vertices);

		program.layout(mesh_mask);
		const auto model = transform.transform([](const auto& value) -> glm::mat4 { return value; })
			.value_or(glm::identity<glm::mat4>());
		ctx.uniform_values["u_M"] = UniformValueBase::create_builtin(model);
		shader.set_uniforms(ctx);

		dk::gfx::backBuffer().render(program, mesh_mask.indices, dk::gfx::Primitive::Triangles);
	}
};

using DrawCall = rfl::TaggedUnion<"draw_kind", ClearDrawCall, SingleMeshDrawCall>;

struct RenderPass {
	std::vector<GuiUniform> gui_uniforms;
	std::vector<DrawCall>   draw_calls;

	void initialize_gui_uniforms(RenderContext& ctx) const
	{
		for (const auto& uniform : gui_uniforms) {
			rfl::visit([&](const auto& type) {
				if (!ctx.gui_uniform_values.contains(type.name()))
					ctx.gui_uniform_values[type.name()] = UniformValueBase::create_gui_editable(type);
			}, uniform);
		}
	}

	void drawGui(RenderContext& ctx) const
	{
		initialize_gui_uniforms(ctx);
		if (!ImGui::Begin("Shader Sandbox")) {
			ImGui::End();
			return;
		}

		for (const auto& uniform : gui_uniforms) {
			rfl::visit([&](const auto& type) {
				ctx.gui_uniform_values.at(type.name())->imgui_edit(type.name().c_str());
			}, uniform);
		}
		ImGui::End();
	}

	void execute(RenderContext& ctx) const
	{
		ctx.update_builtin_uniforms();
		initialize_gui_uniforms(ctx);
		for (const auto& draw_call : draw_calls) {
			rfl::visit([&](const auto& call) { call.execute(ctx); }, draw_call);
		}
	}
};
