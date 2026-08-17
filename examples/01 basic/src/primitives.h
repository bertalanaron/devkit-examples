#pragma once

#include "render_context.h"

#include <devkit/gfx/frame_buffer.h>
#include <devkit/gfx/scene.h>
#include <devkit/gfx/shader.h>

#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <rfl/Skip.hpp>

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

struct FrameBufferAttachmentBinding {
	enum class Type { RenderBuffer, Texture2D };

	Type                    type;
	dk::gfx::Format         format;
	dk::gfx::Channels       channels;
	std::optional<unsigned> samples;
	std::optional<int>      width;
	std::optional<int>      height;

	glm::ivec2 size(const glm::ivec2& default_size) const
	{
		const glm::ivec2 result(
			width.value_or(default_size.x),
			height.value_or(default_size.y));
		if (result.x <= 0 || result.y <= 0)
			throw std::runtime_error("frame buffer attachment dimensions must be positive");
		return result;
	}

	unsigned sample_count() const
	{
		const auto result = samples.value_or(1u);
		if (result == 0)
			throw std::runtime_error("frame buffer attachment samples must be positive");
		return result;
	}

	bool follows_window_size() const
	{
		return !width.has_value() || !height.has_value();
	}
};

struct FrameBufferAttachmentsBinding {
	std::optional<std::map<int, FrameBufferAttachmentBinding>> color;
	std::optional<FrameBufferAttachmentBinding>                depth;
	std::optional<FrameBufferAttachmentBinding>                stencil;
};

enum class FrameBufferAttachmentType { color, depth, stencil };

inline void apply_frame_buffer_config(
	dk::gfx::FrameBuffer& framebuffer,
	const std::optional<dk::gfx::FrameBuffer::Config>& config)
{
	if (!config.has_value())
		return;
	config->for_each([&](const auto& property) {
		framebuffer.config(property);
	});
}

struct BackBufferBinding {
	std::string                                 name = "back_buffer";
	std::optional<dk::gfx::FrameBuffer::Config> config;

	void apply() const
	{
		if (name != "back_buffer")
			throw std::runtime_error("back_buffer.name must be 'back_buffer'");
		apply_frame_buffer_config(dk::gfx::backBuffer(), config);
	}
};

struct FrameBufferBinding {
	std::string                                 name;
	std::optional<dk::gfx::FrameBuffer::Config> config;
	FrameBufferAttachmentsBinding               attachments;

	std::unique_ptr<dk::gfx::FrameBuffer> create(const glm::ivec2& default_size) const
	{
		if (name.empty())
			throw std::runtime_error("frame buffer name cannot be empty");
		if (name == "back_buffer")
			throw std::runtime_error("back_buffer must be defined using the top-level back_buffer field");

		auto framebuffer = std::make_unique<dk::gfx::FrameBuffer>();
		apply_frame_buffer_config(*framebuffer, config);

		const auto bind_attachment = [&](auto& output, const FrameBufferAttachmentBinding& binding) {
			const auto size = binding.size(default_size);
			const auto samples = binding.sample_count();
			switch (binding.type) {
			case FrameBufferAttachmentBinding::Type::RenderBuffer:
				output = dk::gfx::RenderBuffer(size, binding.channels, binding.format, samples);
				break;
			case FrameBufferAttachmentBinding::Type::Texture2D:
				if (samples == 1)
					output = dk::gfx::Texture2D(size, binding.channels, binding.format);
				else
					output = dk::gfx::MultisampledTexture2D(size, samples, binding.channels, binding.format);
				break;
			}
		};

		if (attachments.color.has_value()) {
			for (const auto& [index, binding] : *attachments.color) {
				if (index < 0 || static_cast<std::size_t>(index) >= framebuffer->color.size())
					throw std::runtime_error("color attachment index is outside the supported range in frame buffer '" + name + "'");
				bind_attachment(framebuffer->color[index], binding);
			}
		}
		if (attachments.depth.has_value())
			bind_attachment(framebuffer->depth, *attachments.depth);
		if (attachments.stencil.has_value())
			bind_attachment(framebuffer->stencil, *attachments.stencil);

		const bool has_color = attachments.color.has_value() && !attachments.color->empty();
		if (!has_color && !attachments.depth.has_value() && !attachments.stencil.has_value())
			throw std::runtime_error("frame buffer '" + name + "' must define at least one attachment");

		const auto is_multisampled = [](const auto& binding) {
			return binding.samples.value_or(1u) > 1;
		};
		bool has_multisampled_attachment = false;
		if (attachments.color.has_value()) {
			for (const auto& [index, binding] : *attachments.color) {
				(void)index;
				has_multisampled_attachment |= is_multisampled(binding);
			}
		}
		has_multisampled_attachment |= attachments.depth.has_value() && is_multisampled(*attachments.depth);
		has_multisampled_attachment |= attachments.stencil.has_value() && is_multisampled(*attachments.stencil);
		if (has_multisampled_attachment)
			framebuffer->config(dk::gfx::FrameBuffer::Multisample::Enabled);

		return framebuffer;
	}

	void resize_window_sized_attachments(
		dk::gfx::FrameBuffer& framebuffer,
		const glm::ivec2& default_size) const
	{
		const auto resize_attachment = [&](auto& output, const FrameBufferAttachmentBinding& binding) {
			if (!binding.follows_window_size())
				return;
			const auto target_size = binding.size(default_size);
			const auto current_size = output.size();
			if (current_size.x != target_size.x || current_size.y != target_size.y)
				output.get().resize(target_size);
		};

		if (attachments.color.has_value()) {
			for (const auto& [index, binding] : *attachments.color)
				resize_attachment(framebuffer.color.at(index), binding);
		}
		if (attachments.depth.has_value())
			resize_attachment(framebuffer.depth, *attachments.depth);
		if (attachments.stencil.has_value())
			resize_attachment(framebuffer.stencil, *attachments.stencil);
	}
};

struct RenderPassRuntime {
	std::unordered_map<std::string, std::unique_ptr<dk::gfx::FrameBuffer>> frame_buffers;

	dk::gfx::FrameBuffer& frame_buffer(const std::string& name) const
	{
		if (name == "back_buffer")
			return dk::gfx::backBuffer();
		const auto found = frame_buffers.find(name);
		if (found == frame_buffers.end())
			throw std::runtime_error("unknown frame buffer '" + name + "'");
		return *found->second;
	}

	dk::gfx::Texture& texture(
		const std::string& frame_buffer_name,
		FrameBufferAttachmentType attachment_type,
		int color_index) const
	{
		if (frame_buffer_name == "back_buffer")
			throw std::runtime_error("back_buffer attachments cannot be used as uniform textures");

		auto& framebuffer = frame_buffer(frame_buffer_name);
		dk::gfx::RenderTarget* target = nullptr;
		if (attachment_type == FrameBufferAttachmentType::color) {
			if (color_index < 0 || static_cast<std::size_t>(color_index) >= framebuffer.color.size() ||
				!framebuffer.color[color_index].has_value())
				throw std::runtime_error("frame buffer '" + frame_buffer_name + "' has no color attachment " + std::to_string(color_index));
			target = &framebuffer.color[color_index].get();
		}
		else if (attachment_type == FrameBufferAttachmentType::depth) {
			if (!framebuffer.depth.has_value())
				throw std::runtime_error("frame buffer '" + frame_buffer_name + "' has no depth attachment");
			target = &framebuffer.depth.get();
		}
		else if (attachment_type == FrameBufferAttachmentType::stencil) {
			if (!framebuffer.stencil.has_value())
				throw std::runtime_error("frame buffer '" + frame_buffer_name + "' has no stencil attachment");
			target = &framebuffer.stencil.get();
		}
		auto* texture = dynamic_cast<dk::gfx::Texture*>(target);
		if (!texture)
			throw std::runtime_error("the selected attachment of frame buffer '" + frame_buffer_name + "' is not texture-backed");
		return *texture;
	}
};

inline void apply_texture_config(
	dk::gfx::Texture& texture,
	const std::optional<dk::gfx::Texture::Config>& config)
{
	if (!config.has_value())
		return;
	config->for_each([&](const auto& property) {
		texture.config(property);
	});
}

struct AssetUniformTexture {
	using Tag = rfl::Literal<"asset">;

	rfl::Rename<"uniform", std::string>                            m_uniform;
	rfl::Rename<"texture", std::filesystem::path>                  m_texture;
	rfl::Rename<"config", std::optional<dk::gfx::Texture::Config>> m_config;

	void bind_to_shader(dk::gfx::Shader& shader, RenderContext& ctx, RenderPassRuntime&) const
	{
		auto& texture = ctx.assets[m_texture.get().string()].as<dk::gfx::Texture2D>();
		apply_texture_config(texture, m_config.get());
		shader.uniformTexture(m_uniform.get(), texture);
	}
};

struct FrameBufferAttachmentUniformTexture {
	using Tag = rfl::Literal<"frame_buffer_attachment">;

	struct Attachment {
		FrameBufferAttachmentType type;
		std::optional<int>        index;
	};

	rfl::Rename<"uniform", std::string>                            m_uniform;
	rfl::Rename<"frame_buffer", std::string>                       m_frame_buffer;
	rfl::Rename<"attachment", Attachment>                          m_attachment;
	rfl::Rename<"config", std::optional<dk::gfx::Texture::Config>> m_config;

	void bind_to_shader(dk::gfx::Shader& shader, RenderContext&, RenderPassRuntime& runtime) const
	{
		auto& texture = runtime.texture(
			m_frame_buffer.get(),
			m_attachment.get().type,
			m_attachment.get().index.value_or(0));
		apply_texture_config(texture, m_config.get());
		shader.uniformTexture(m_uniform.get(), texture);
	}
};

using UniformTexture = rfl::TaggedUnion<
	"source",
	AssetUniformTexture,
	FrameBufferAttachmentUniformTexture>;

struct ShaderBinding {
	rfl::Rename<"program", std::filesystem::path>                              m_program;
	rfl::Rename<"uniforms", std::optional<std::vector<std::string>>>           m_uniforms;
	rfl::Rename<"uniform_textures", std::optional<std::vector<UniformTexture>>> m_uniform_textures;

	void set_uniforms(RenderContext& ctx, RenderPassRuntime& runtime) const
	{
		auto& program = ctx.assets[m_program.get()].as<dk::gfx::Shader>();

		if (m_uniforms.get().has_value()) {
			for (const auto& uniform : *m_uniforms.get())
				ctx.uniform(uniform).bind_to(uniform, program);
		}

		if (m_uniform_textures.get().has_value()) {
			for (const auto& uniform_texture : *m_uniform_textures.get()) {
				rfl::visit([&](const auto& binding) {
					binding.bind_to_shader(program, ctx, runtime);
				}, uniform_texture);
			}
		}
	}
};

struct MeshBinding {
	std::filesystem::path source;
	dk::gfx::VertexFlags  vertices;
};

struct ClearDrawCall {
	using Tag = rfl::Literal<"clear">;

	std::optional<std::string> output_buffer;
	std::optional<dk::gfx::Clear> mask;
	std::optional<glm::vec4>      color;

	void execute(RenderContext&, RenderPassRuntime& runtime) const
	{
		runtime.frame_buffer(output_buffer.value_or("back_buffer")).clear(
			mask.value_or(dk::gfx::Clear::Color | dk::gfx::Clear::Depth),
			color.value_or(dk::colors::black));
	}
};

struct SingleMeshDrawCall {
	using Tag = rfl::Literal<"single_mesh">;

	std::optional<std::string> output_buffer;
	ShaderBinding              shader;
	MeshBinding                mesh;
	std::optional<Transform>   transform;
	dk::gfx::Primitive         gl_primitive;

	void execute(RenderContext& ctx, RenderPassRuntime& runtime) const
	{
		auto& program = ctx.assets[shader.m_program.get()].as<dk::gfx::Shader>();
		auto& mesh_factory = ctx.assets[mesh.source].as<dk::gfx::Scene::MeshFactory>();
		auto& mesh_mask = mesh_factory(mesh.vertices);

		program.layout(mesh_mask);
		const auto model = transform.transform([](const auto& value) -> glm::mat4 { return value; })
			.value_or(glm::identity<glm::mat4>());
		ctx.uniform_values["u_M"] = UniformValueBase::create_builtin(model);
		shader.set_uniforms(ctx, runtime);

		runtime.frame_buffer(output_buffer.value_or("back_buffer")).render(
			program, mesh_mask.indices, gl_primitive);
	}
};

struct BlitDrawCall {
	using Tag = rfl::Literal<"blit">;

	std::string                           output_buffer;
	std::string                           input_buffer;
	std::optional<dk::gfx::Mask>          mask;
	std::optional<int>                    input_color_index;
	std::optional<int>                    output_color_index;
	std::optional<dk::gfx::Texture::MagFilter> filter;
	std::optional<dk::gfx::Rect>          src_rect;
	std::optional<dk::gfx::Rect>          dst_rect;

	void execute(RenderContext&, RenderPassRuntime& runtime) const
	{
		auto& output = runtime.frame_buffer(output_buffer);
		auto& input = runtime.frame_buffer(input_buffer);
		const auto selected_mask = mask.value_or(dk::gfx::Mask::Color);
		const auto input_index = input_color_index.value_or(0);
		const auto output_index = output_color_index.value_or(0);
		const auto selected_filter = filter.value_or(dk::gfx::Texture::MagFilter::Linear);

		if (src_rect.has_value() || dst_rect.has_value()) {
			const auto source = src_rect.value_or(dk::gfx::Rect{ input.viewport().offset(), input.viewport().size() });
			const auto destination = dst_rect.value_or(dk::gfx::Rect{ output.viewport().offset(), output.viewport().size() });
			output.blit(input, source, destination, selected_mask, input_index, output_index, selected_filter);
		}
		else {
			output.blit(input, selected_mask, input_index, output_index, selected_filter);
		}
	}
};

struct PostProcessDrawCall {
	using Tag = rfl::Literal<"post_process">;

	std::optional<std::string> output_buffer;
	ShaderBinding              shader;

	void execute(RenderContext& ctx, RenderPassRuntime& runtime) const
	{
		auto& program = ctx.assets[shader.m_program.get()].as<dk::gfx::Shader>();
		if (!program.source(dk::gfx::ShaderSource::Vertex).has_value())
			program.source(dk::gfx::ShaderSource::postProcessVertexSource());
		shader.set_uniforms(ctx, runtime);
		runtime.frame_buffer(output_buffer.value_or("back_buffer")).render(program);
	}
};

using DrawCall = rfl::TaggedUnion<
	"draw_kind",
	ClearDrawCall,
	SingleMeshDrawCall,
	BlitDrawCall,
	PostProcessDrawCall>;

struct RenderPass {
	BackBufferBinding               back_buffer;
	std::vector<GuiUniform>         gui_uniforms;
	std::vector<FrameBufferBinding> frame_buffers;
	std::vector<DrawCall>           draw_calls;
	mutable rfl::Skip<std::shared_ptr<RenderPassRuntime>> runtime;

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

	RenderPassRuntime& initialize_runtime(const glm::ivec2& default_size) const
	{
		if (!runtime.get()) {
			auto initialized = std::make_shared<RenderPassRuntime>();
			for (const auto& binding : frame_buffers) {
				auto [entry, inserted] = initialized->frame_buffers.emplace(
					binding.name, binding.create(default_size));
				if (!inserted)
					throw std::runtime_error("duplicate frame buffer name '" + binding.name + "'");
			}
			runtime = std::move(initialized);
		}
		return *runtime.get();
	}

	void execute(RenderContext& ctx) const
	{
		if (ctx.updated) {
			for (auto&& [_, shader] : ctx.assets.of_type("shader"))
				shader.as<dk::gfx::Shader>().clearTextureUnit();
			ctx.updated = false;
		}

		ctx.update_builtin_uniforms();
		initialize_gui_uniforms(ctx);
		back_buffer.apply();

		const auto window_size = ctx.frame->viewport().size();
		auto& current_runtime = initialize_runtime(window_size);
		for (const auto& binding : frame_buffers)
			binding.resize_window_sized_attachments(current_runtime.frame_buffer(binding.name), window_size);

		for (const auto& draw_call : draw_calls) {
			rfl::visit([&](const auto& call) { call.execute(ctx, current_runtime); }, draw_call);
		}
	}
};
