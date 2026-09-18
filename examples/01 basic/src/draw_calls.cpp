#include "draw_calls.h"

#include "render_context.h"
#include "runtime.h"

#include <devkit/gfx/scene.h>

#include <charconv>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

namespace {

void apply_texture_config(
	dk::gfx::Texture& texture,
	const std::optional<dk::gfx::Texture::Config>& config)
{
	if (!config)
		return;
	config->for_each([&](const auto& property) { texture.config(property); });
}

glm::vec4 parse_color(const std::string& value)
{
	if (value.empty() || value.front() != '#' || (value.size() != 7 && value.size() != 9))
		throw std::runtime_error("color must use #RRGGBB or #RRGGBBAA notation");

	const auto component = [&](std::size_t offset) {
		unsigned result = 0;
		const auto [end, error] = std::from_chars(value.data() + offset, value.data() + offset + 2, result, 16);
		if (error != std::errc{} || end != value.data() + offset + 2)
			throw std::runtime_error("invalid color '" + value + "'");
		return static_cast<float>(result) / 255.f;
	};
	return { component(1), component(3), component(5), value.size() == 9 ? component(7) : 1.f };
}

void bind_uniform_texture(
	dk::gfx::Shader& shader, RenderContext&, Runtime& runtime,
	const primitives::Texture2DUniform& binding)
{
	auto& texture = runtime.texture2d(binding.name);
	apply_texture_config(texture, binding.config);
	shader.uniformTexture(binding.uniform, texture);
}

void bind_uniform_texture(
	dk::gfx::Shader& shader, RenderContext&, Runtime& runtime,
	const primitives::FrameBufferTexture2DUniform& binding)
{
	auto& texture = runtime.texture(primitives::textures::FrameBufferTexture2DReference{
		binding.frame_buffer, binding.attachment, binding.config});
	apply_texture_config(texture, binding.config);
	shader.uniformTexture(binding.uniform, texture);
}

void bind_uniform_texture(
	dk::gfx::Shader& shader, RenderContext& context, Runtime&,
	const primitives::Texture2DAssetUniform& binding)
{
	auto& texture = context.assets[binding.asset].as<dk::gfx::Texture2D>();
	apply_texture_config(texture, binding.config);
	shader.uniformTexture(binding.uniform, texture);
}

void bind_uniform_texture(
	dk::gfx::Shader& shader, RenderContext&, Runtime& runtime,
	const primitives::Texture2DArrayUniform& binding)
{
	auto& texture = runtime.texture2d_array(binding.array);
	apply_texture_config(texture, binding.config);
	shader.uniformTexture(binding.uniform, texture);
}

void bind_uniform_texture(
	dk::gfx::Shader& shader, RenderContext& context, Runtime&,
	const primitives::Texture2DArrayAssetUniform& binding)
{
	auto& texture = context.assets[binding.array_asset].as<dk::gfx::Texture2DArray>();
	apply_texture_config(texture, binding.config);
	shader.uniformTexture(binding.uniform, texture);
}

void bind_uniform_texture(
	dk::gfx::Shader& shader, RenderContext& context, Runtime&,
	const primitives::Texture2DArrayAssetLayerUniform& binding)
{
	auto& texture = context.assets[binding.asset].as<dk::gfx::Texture2DArray>();
	apply_texture_config(texture, binding.config);
	shader.uniformTexture(binding.uniform, texture);
}

void bind_uniform_texture(
	dk::gfx::Shader&, RenderContext&, Runtime&,
	const primitives::Texture2DArrayLayerUniform& binding)
{
	throw std::runtime_error("texture array layers cannot be bound as standalone samplers: " + binding.name);
}

void bind_uniform_texture(
	dk::gfx::Shader&, RenderContext&, Runtime&,
	const primitives::FrameBufferTexture2DArrayUniform&)
{
	throw std::runtime_error("texture array framebuffer layers cannot be bound as standalone samplers");
}

void bind_uniform_textures(
	dk::gfx::Shader& shader, RenderContext& context, Runtime& runtime,
	const std::optional<std::vector<primitives::ShaderUniformTexture>>& textures)
{
	if (!textures)
		return;
	for (const auto& uniform : *textures)
		uniform.visit([&](const auto& binding) {
			bind_uniform_texture(shader, context, runtime, binding);
		});
}

void bind_uniform_cameras(
	dk::gfx::Shader& shader, Runtime& runtime,
	const std::optional<std::vector<UniformCameraBinding>>& cameras)
{
	if (!cameras)
		return;
	for (const auto& binding : *cameras) {
		const auto& camera = runtime.camera(binding.name);
		for (auto member : binding.uniform_members) {
			if (member.starts_with("u_"))
				member.erase(0, 2);
			const auto target = binding.uniform + "." + member;
			if (member == "VP")
				shader.uniforms().set(target, camera.P() * camera.V());
			else if (member == "position")
				shader.uniforms().set(target, camera.position);
			else if (member == "direction")
				shader.uniforms().set(target, camera.lookat - camera.position);
			else if (member == "asp")
				shader.uniforms().set(target, camera.asp);
			else if (member == "fov")
				shader.uniforms().set(target, camera.fov);
			else if (member == "np" || member == "nearPlane")
				shader.uniforms().set(target, camera.np);
			else if (member == "fp" || member == "farPlane")
				shader.uniforms().set(target, camera.fp);
			else if (member.starts_with("lightspaceM[") && member.back() == ']') {
				const auto index_text = member.substr(12, member.size() - 13);
				std::size_t index = 0;
				const auto [end, error] = std::from_chars(
					index_text.data(), index_text.data() + index_text.size(), index);
				if (error != std::errc{} || end != index_text.data() + index_text.size())
					throw std::runtime_error("invalid lightspace matrix member '" + member + "'");
				shader.uniforms().set(target, runtime.lightspace_matrix(binding.name, index));
			}
			else
				throw std::runtime_error("unknown camera member '" + member + "'");
		}
	}
}

template <typename DrawCall>
void set_shader_uniforms(
	dk::gfx::Shader& shader, RenderContext& context, Runtime& runtime,
	const DrawCall& draw_call)
{
	if (draw_call.gui_uniforms)
		for (const auto& uniform : *draw_call.gui_uniforms)
			context.gui_uniform(uniform).bind_to(uniform, shader);
	bind_uniform_textures(shader, context, runtime, draw_call.uniform_textures);
	bind_uniform_cameras(shader, runtime, draw_call.uniform_cameras);
}

glm::mat4 transform_matrix(
	const std::vector<primitives::transforms::Transform>& transforms)
{
	glm::mat4 result = glm::identity<glm::mat4>();
	for (const auto& transform : transforms) {
		rfl::visit([&](const auto& operation) {
		using Operation = std::remove_cvref_t<decltype(operation)>;
		if constexpr (std::same_as<Operation, rfl::Field<"translate", glm::vec3>>)
			result *= glm::translate(glm::identity<glm::mat4>(), operation.get());
		else if constexpr (std::same_as<Operation, rfl::Field<"scale", glm::vec3>>)
			result *= glm::scale(glm::identity<glm::mat4>(), operation.get());
		else
			result *= glm::rotate(glm::identity<glm::mat4>(), operation.get().angle, operation.get().axis);
	}, transform);
	}
	return result;
}

} // namespace

void draw_calls::Clear::execute(RenderContext&, Runtime& runtime) const
{
	const auto clear_color = color.transform([](const auto& value) {
		return rfl::visit([](const auto& color_value) -> glm::vec4 {
			using Color = std::remove_cvref_t<decltype(color_value)>;
			if constexpr (std::same_as<Color, glm::vec3>)
				return glm::vec4(color_value, 1.f);
			else if constexpr (std::same_as<Color, glm::vec4>)
				return color_value;
			else
				return parse_color(color_value);
		}, value);
	}).value_or(dk::colors::black);
	runtime.frame_buffer(output_buffer).clear(mask, clear_color);
}

void draw_calls::BlitDrawCall::execute(RenderContext&, Runtime& runtime) const
{
	auto& output = runtime.frame_buffer(output_buffer);
	auto& input = runtime.frame_buffer(input_buffer);
	const auto selected_mask = mask.value_or(dk::gfx::Mask::Color);
	const auto input_index = input_color_index.value_or(0);
	const auto output_index = output_color_index.value_or(0);
	const auto selected_filter = filter.value_or(dk::gfx::Texture::MagFilter::Linear);
	if (src_rect || dst_rect) {
		const auto source = src_rect.value_or(dk::gfx::Rect{ input.viewport().offset(), input.viewport().size() });
		const auto destination = dst_rect.value_or(dk::gfx::Rect{ output.viewport().offset(), output.viewport().size() });
		output.blit(input, source, destination, selected_mask, input_index, output_index, selected_filter);
	}
	else {
		output.blit(input, selected_mask, input_index, output_index, selected_filter);
	}
}

void draw_calls::PostProcessDrawCall::execute(RenderContext& context, Runtime& runtime) const
{
	auto& program = runtime.shader(context, shader);
	if (!program.source(dk::gfx::ShaderSource::Vertex).has_value())
		program.source(dk::gfx::ShaderSource::postProcessVertexSource());
	set_shader_uniforms(program, context, runtime, *this);
	runtime.frame_buffer(output_buffer).render(program);
}

void draw_calls::SingleMeshDrawCall::execute(RenderContext& context, Runtime& runtime) const
{
	auto& program = runtime.shader(context, shader);
	auto& mesh_factory = context.assets[mesh.source].as<dk::gfx::Scene::MeshFactory>();
	auto& mesh_mask = mesh_factory(mesh.vertices);
	program.layout(mesh_mask);

	const auto model = transform_matrix(transforms);
	if (model_uniform)
		program.uniforms().set(*model_uniform, model);
	if (time_uniform)
		program.uniforms().set(*time_uniform, context.time);
	if (cascade)
		program.uniforms().set("u_cascade", *cascade);
	set_shader_uniforms(program, context, runtime, *this);
	runtime.frame_buffer(output_buffer).render(program, mesh_mask.indices, gl_primitive);
}
