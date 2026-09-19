#include "runtime.h"
#include "csm.h"

#include <devkit/gfx/frame_buffer.h>

#include <stdexcept>
#include <type_traits>

#include <imgui.h>

namespace {

void apply_frame_buffer_config(
	dk::gfx::FrameBuffer& frame_buffer,
	const std::optional<dk::gfx::FrameBuffer::Config>& config)
{
	if (!config)
		return;
	config->for_each([&](const auto& property) { frame_buffer.config(property); });
}

glm::ivec2 to_size(const std::optional<glm::vec2>& configured, const glm::ivec2& fallback)
{
	const auto size = configured.value_or(glm::vec2(fallback));
	const glm::ivec2 result(size);
	if (result.x <= 0 || result.y <= 0)
		throw std::runtime_error("render target dimensions must be positive");
	return result;
}

unsigned sample_count(const std::optional<unsigned>& configured)
{
	const auto result = configured.value_or(1u);
	if (result == 0)
		throw std::runtime_error("render target samples must be positive");
	return result;
}

template <typename Output>
void bind_attachment(
	Output& output,
	const primitives::textures::RenderBufferDefinition& definition,
	const std::optional<glm::vec2>& frame_buffer_size,
	const glm::ivec2& default_size)
{
	const auto size = to_size(definition.size.or_else([&] { return frame_buffer_size; }), default_size);
	const auto samples = sample_count(definition.samples);
	output = dk::gfx::RenderBuffer(size, definition.channels, definition.format, samples);
}

template <typename Output>
void bind_attachment(
	Output& output,
	const primitives::textures::Texture2DDefinition& definition,
	const std::optional<glm::vec2>& frame_buffer_size,
	const glm::ivec2& default_size)
{
	const auto size = to_size(definition.size.or_else([&] { return frame_buffer_size; }), default_size);
	const auto samples = sample_count(definition.samples);
	if (samples == 1)
		output = dk::gfx::Texture2D(size, definition.channels, definition.format);
	else
		output = dk::gfx::MultisampledTexture2D(size, samples, definition.channels, definition.format);
}

} // namespace

void Runtime::reset()
{
	m_frame_buffer_views.clear();
	m_frame_buffers.clear();
	m_textures.clear();
	m_cameras.clear();
}

void Runtime::initialize(
	const primitives::TextureCollectionDefinition& textures,
	const primitives::FrameBufferCollectionDefinition& frame_buffers,
	const std::vector<primitives::CameraBinding>& cameras,
	const glm::ivec2& default_size)
{
	reset();
	for (const auto& [name, definition] : textures.textures)
		create_texture(name, definition);

	for (const auto& [name, definition] : frame_buffers.frame_buffers)
		create_frame_buffer(name, definition, default_size);
	initialize_cameras(cameras);
}

void Runtime::update(
	const primitives::FrameBufferCollectionDefinition& frame_buffers,
	const std::vector<primitives::CameraBinding>& cameras,
	const RenderContext& context,
	const glm::ivec2& window_size)
{
	apply_frame_buffer_config(dk::gfx::backBuffer(), frame_buffers.back_buffer.config);
	update_cameras(cameras, context);

	for (const auto& [name, definition] : frame_buffers.frame_buffers) {
		if (!definition.attachments)
			continue;
		auto& target = *m_frame_buffers.at(name);
		const auto resize = [&](auto& attachment, const primitives::textures::FrameBufferAttachment& configured) {
			configured.visit([&](const auto& descriptor) {
				using Descriptor = std::remove_cvref_t<decltype(descriptor)>;
				if constexpr (
					std::same_as<Descriptor, primitives::textures::RenderBufferDefinition> ||
					std::same_as<Descriptor, primitives::textures::Texture2DDefinition> ||
					std::same_as<Descriptor, primitives::textures::Texture2DArrayDefinition>) {
					const auto target_size = to_size(
						descriptor.size.or_else([&] { return definition.size; }), window_size);
					if (attachment.has_value() &&
						(attachment.size().x != target_size.x || attachment.size().y != target_size.y))
						attachment.get().resize(target_size);
				}
			});
		};
		if (definition.attachments->color)
			for (const auto& [index, attachment] : *definition.attachments->color)
				resize(target.color.at(index), attachment);
		if (definition.attachments->depth)
			resize(target.depth, *definition.attachments->depth);
		if (definition.attachments->stencil)
			resize(target.stencil, *definition.attachments->stencil);
	}
}

void Runtime::create_texture(
	const std::string& name,
	const primitives::textures::TextureDefinition& definition)
{
	if (name.empty())
		throw std::runtime_error("texture name cannot be empty");
	if (m_textures.contains(name))
		throw std::runtime_error("duplicate texture name '" + name + "'");

	rfl::visit([&](const auto& typed_definition) {
		using Definition = std::remove_cvref_t<decltype(typed_definition)>;
		if constexpr (std::same_as<Definition, primitives::textures::Texture2DDefinition>) {
			const auto size = to_size(typed_definition.size, glm::ivec2(1));
			if (typed_definition.samples.value_or(1u) != 1)
				throw std::runtime_error("multisampled textures must be framebuffer attachments");
			auto texture = std::make_unique<Texture>(
				std::in_place_type<dk::gfx::Texture2D>, size,
				typed_definition.channels, typed_definition.format);
			m_textures.emplace(name, std::move(texture));
		}
		else {
			const auto size = to_size(typed_definition.size, glm::ivec2(1));
			if (typed_definition.layers <= 0 || typed_definition.samples.value_or(1u) != 1)
				throw std::runtime_error("invalid texture array definition");
			auto texture = std::make_unique<Texture>(
				std::in_place_type<dk::gfx::Texture2DArray>, size,
				typed_definition.layers, typed_definition.channels, typed_definition.format);
			m_textures.emplace(name, std::move(texture));
		}
	}, definition);
}

void Runtime::create_frame_buffer(
	const std::string& name,
	const primitives::FrameBufferDefinition& definition,
	const glm::ivec2& default_size)
{
	if (name.empty())
		throw std::runtime_error("frame buffer name cannot be empty");
	if (name == "back_buffer" || m_frame_buffer_views.contains(name))
		throw std::runtime_error("duplicate or reserved frame buffer name '" + name + "'");
	if (!definition.attachments)
		throw std::runtime_error("frame buffer '" + name + "' must define attachments");

	auto frame_buffer = std::make_unique<dk::gfx::FrameBuffer>();
	apply_frame_buffer_config(*frame_buffer, definition.config);
	const auto bind = [&](auto& output, const auto& attachment) {
		bool multisampled = false;
		attachment.visit([&](const auto& typed) {
			using Descriptor = std::remove_cvref_t<decltype(typed)>;
			if constexpr (
				std::same_as<Descriptor, primitives::textures::RenderBufferDefinition> ||
				std::same_as<Descriptor, primitives::textures::Texture2DDefinition> ||
				std::same_as<Descriptor, primitives::textures::Texture2DArrayDefinition>) {
				if constexpr (std::same_as<Descriptor, primitives::textures::Texture2DArrayDefinition>) {
					const auto size = to_size(typed.size.or_else([&] { return definition.size; }), default_size);
					if (typed.layers <= 0 || typed.samples.value_or(1u) != 1)
						throw std::runtime_error("invalid texture array framebuffer attachment");
					output = dk::gfx::Texture2DArray(size, typed.layers, typed.channels, typed.format);
				}
				else {
					bind_attachment(output, typed, definition.size, default_size);
				}
				multisampled = typed.samples.value_or(1u) > 1;
			}
			else if constexpr (std::same_as<Descriptor, primitives::textures::RenderBufferReference>) {
				throw std::runtime_error("named render buffer attachments are not supported");
			}
			else if constexpr (std::same_as<Descriptor, primitives::textures::Texture2DReference>) {
				output = texture2d(typed.name);
			}
			else if constexpr (std::same_as<Descriptor, primitives::textures::Texture2DArrayReference>) {
				output = texture2d_array(typed.name);
			}
			else if constexpr (std::same_as<Descriptor, primitives::textures::Texture2DArrayLayerReference>) {
				output = texture2d_array(typed.name)[typed.layer];
			}
		});
		if (multisampled)
			frame_buffer->config(dk::gfx::FrameBuffer::Multisample::Enabled);
	};

	if (definition.attachments->color)
		for (const auto& [index, attachment] : *definition.attachments->color) {
			if (index < 0 || static_cast<std::size_t>(index) >= frame_buffer->color.size())
				throw std::runtime_error("color attachment index is outside the supported range in frame buffer '" + name + "'");
			bind(frame_buffer->color.at(index), attachment);
		}
	if (definition.attachments->depth)
		bind(frame_buffer->depth, *definition.attachments->depth);
	if (definition.attachments->stencil)
		bind(frame_buffer->stencil, *definition.attachments->stencil);
	if ((!definition.attachments->color || definition.attachments->color->empty()) &&
		!definition.attachments->depth && !definition.attachments->stencil)
		throw std::runtime_error("frame buffer '" + name + "' must define at least one attachment");

	const auto [entry, inserted] = m_frame_buffers.emplace(name, std::move(frame_buffer));
	if (!inserted)
		throw std::runtime_error("duplicate frame buffer name '" + name + "'");
	m_frame_buffer_views.emplace(name, entry->second.get());
}

void Runtime::initialize_cameras(const std::vector<primitives::CameraBinding>& cameras)
{
	for (const auto& binding : cameras) {
		binding.visit([&](const auto& definition) {
			if (definition.name.empty())
				throw std::runtime_error("camera name cannot be empty");
			if (!(definition.np > 0.f) || !(definition.fp > definition.np))
				throw std::runtime_error("camera '" + definition.name + "' must have 0 < np < fp");

			CameraRuntime runtime;
			using Definition = std::remove_cvref_t<decltype(definition)>;
			if constexpr (std::same_as<Definition, primitives::MainCamera>) {
				runtime = MainCameraRuntime{};
			}
			else {
				const auto cascades = definition.cascade_count.value_or(3u);
				const auto resolution = definition.shadow_resolution.value_or(1024u);
				if (cascades == 0 || cascades > csm::max_cascades)
					throw std::runtime_error("csm_sun.cascade_count must be between 1 and 4");
				if (resolution == 0)
					throw std::runtime_error("csm_sun.shadow_resolution must be positive");
				auto& csm = runtime.emplace<CsmSunCameraRuntime>();
				csm.lightspace_matrices.resize(cascades);
				csm.shadow_buffer = std::make_unique<dk::gfx::FrameBuffer>();
				csm.shadow_buffer->config(dk::gfx::FrameBuffer::DepthTest::Enabled);
				csm.shadow_buffer->config(dk::gfx::FrameBuffer::DepthFunc::Less);
				csm.shadow_buffer->config(dk::gfx::FrameBuffer::CullFace::Disabled);
				csm.shadow_buffer->config(dk::gfx::FrameBuffer::PointSize(1.f));
				csm.shadow_buffer->depth = dk::gfx::Texture2DArray(
					glm::ivec2(resolution), cascades, dk::gfx::Channels::Depth, dk::gfx::Format::Depth32F);
			}

			const auto [entry, inserted] = m_cameras.emplace(definition.name, std::move(runtime));
			if (!inserted)
				throw std::runtime_error("duplicate camera name '" + definition.name + "'");
			if constexpr (std::same_as<Definition, primitives::CsmSunCamera>) {
				auto& csm = std::get<CsmSunCameraRuntime>(entry->second);
				const auto shadow_name = definition.name + "_shadow";
				if (!m_frame_buffer_views.emplace(shadow_name, csm.shadow_buffer.get()).second)
					throw std::runtime_error("duplicate frame buffer name '" + shadow_name + "'");
			}
		});
	}
}

void Runtime::update_cameras(
	const std::vector<primitives::CameraBinding>& cameras, const RenderContext& context)
{
	dk::gfx::Camera* main_camera = nullptr;
	for (const auto& binding : cameras) {
		binding.visit([&](const auto& definition) {
			using Definition = std::remove_cvref_t<decltype(definition)>;
			if constexpr (std::same_as<Definition, primitives::MainCamera>) {
				auto& camera = std::get<MainCameraRuntime>(m_cameras.at(definition.name)).camera;
				camera = context.camera;
				camera.np = definition.np;
				camera.fp = definition.fp;
				camera.fov = definition.fov.value_or(1.f);
				main_camera = &camera;
			}
		});
	}

	const auto& source = main_camera ? *main_camera : context.camera;
	for (const auto& binding : cameras) {
		binding.visit([&](const auto& definition) {
			using Definition = std::remove_cvref_t<decltype(definition)>;
			if constexpr (std::same_as<Definition, primitives::CsmSunCamera>) {
				auto& camera_runtime = std::get<CsmSunCameraRuntime>(m_cameras.at(definition.name));
				camera_runtime.camera = source;
				camera_runtime.camera.np = definition.np;
				camera_runtime.camera.fp = definition.fp;
				camera_runtime.camera.fov = definition.fov.value_or(1.f);

				const auto cascades = definition.cascade_count.value_or(3u);
				const auto direction = definition.direction.value_or(glm::vec3(-0.5f, -1.f, -0.25f));
				camera_runtime.lightspace_matrices.clear();
				camera_runtime.lightspace_matrices.reserve(cascades);
				camera_runtime.direction = glm::normalize(direction);
				camera_runtime.split_depths = csm::split_depths(definition.np, definition.fp, cascades);
				float previous_split = definition.np;
				for (const auto split : camera_runtime.split_depths) {
					camera_runtime.lightspace_matrices.push_back(
						csm::lightspace_matrix(source, direction, previous_split, split));
					previous_split = split;
				}
			}
		});
	}
}

dk::gfx::FrameBuffer& Runtime::frame_buffer(const std::string& name)
{
	const auto resolved_name = name.empty() ? std::string("back_buffer") : name;
	if (resolved_name == "back_buffer")
		return dk::gfx::backBuffer();
	const auto found = m_frame_buffer_views.find(resolved_name);
	if (found == m_frame_buffer_views.end())
		throw std::runtime_error("unknown frame buffer '" + resolved_name + "'");
	return *found->second;
}

dk::gfx::FrameBuffer& Runtime::frame_buffer(const primitives::FrameBufferReference& reference)
{
	return rfl::visit([&](const auto& value) -> dk::gfx::FrameBuffer& {
		using Value = std::remove_cvref_t<decltype(value)>;
		if constexpr (std::same_as<Value, std::string>) {
			return frame_buffer(value);
		}
		else {
			auto& result = frame_buffer(value.name);
			apply_frame_buffer_config(result, value.config);
			return result;
		}
	}, reference);
}

dk::gfx::Texture2D& Runtime::texture2d(const std::string& name)
{
	return std::get<dk::gfx::Texture2D>(*m_textures.at(name));
}

dk::gfx::Texture2DArray& Runtime::texture2d_array(const std::string& name)
{
	return std::get<dk::gfx::Texture2DArray>(*m_textures.at(name));
}

dk::gfx::Texture& texture_from_attachment(
	dk::gfx::FrameBuffer& frame_buffer,
	const primitives::FrameBufferAttachmentReference& reference)
{
	return reference.visit([&](const auto& attachment) -> dk::gfx::Texture& {
		using Attachment = std::remove_cvref_t<decltype(attachment)>;
		dk::gfx::RenderTarget* target = nullptr;
		if constexpr (std::same_as<Attachment, primitives::ColorAttachment>)
			target = &frame_buffer.color.at(attachment.index.value_or(0)).get();
		else if constexpr (std::same_as<Attachment, primitives::DepthAttachment>)
			target = &frame_buffer.depth.get();
		else
			target = &frame_buffer.stencil.get();
		if (auto* texture = dynamic_cast<dk::gfx::Texture*>(target))
			return *texture;
		throw std::runtime_error("selected framebuffer attachment is not texture-backed");
	});
}

dk::gfx::Texture& Runtime::texture(
	const primitives::textures::FrameBufferTexture2DReference& reference)
{
	return texture_from_attachment(frame_buffer(reference.frame_buffer), reference.attachment);
}

dk::gfx::Texture& Runtime::texture(
	const primitives::textures::FrameBufferTexture2DArrayReference& reference)
{
	(void)reference;
	throw std::runtime_error("texture array framebuffer layers cannot be bound as a sampler");
}

dk::gfx::Shader& Runtime::shader(
	RenderContext& context, const primitives::ShaderReference& reference) const
{
	return rfl::visit([&](const auto& value) -> dk::gfx::Shader& {
		using Value = std::remove_cvref_t<decltype(value)>;
		if constexpr (std::same_as<Value, std::filesystem::path>) {
			return context.assets[value].template as<dk::gfx::Shader>();
		}
		else {
			auto& shader = context.assets[value.asset].template as<dk::gfx::Shader>();
			if (value.config)
				value.config->for_each([&](const auto& property) { shader.config(property); });
			return shader;
		}
	}, reference);
}

const dk::gfx::Camera& Runtime::camera(const std::string& name) const
{
	const auto found = m_cameras.find(name);
	if (found == m_cameras.end())
		throw std::runtime_error("unknown camera '" + name + "'");
	return std::visit([](const auto& runtime) -> const dk::gfx::Camera& { return runtime.camera; }, found->second);
}

glm::vec3 Runtime::camera_direction(const std::string& name) const
{
    const auto found = m_cameras.find(name);
    if (found == m_cameras.end())
        throw std::runtime_error("unknown camera '" + name + "'");
    return std::visit([](const auto& runtime) -> glm::vec3 {
        using Camera = std::remove_cvref_t<decltype(runtime)>;
        if constexpr (std::same_as<Camera, CsmSunCameraRuntime>)
            return runtime.direction;
        else
            return runtime.camera.lookat - runtime.camera.position;
    }, found->second);
}

const Runtime::CsmSunCameraRuntime& Runtime::csm_camera(const std::string& name) const
{
    const auto found = m_cameras.find(name);
    if (found == m_cameras.end())
        throw std::runtime_error("unknown camera '" + name + "'");
    const auto* result = std::get_if<CsmSunCameraRuntime>(&found->second);
    if (!result)
        throw std::runtime_error("camera '" + name + "' is not a csm_sun camera");
    return *result;
}

glm::mat4 Runtime::lightspace_matrix(const std::string& name, std::size_t index) const
{
    return csm_camera(name).lightspace_matrices.at(index);
}

const std::vector<float>& Runtime::cascade_splits(const std::string& name) const
{
    return csm_camera(name).split_depths;
}

void RenderPass::initialize_gui_uniforms(RenderContext& context) const
{
	for (const auto& uniform : gui_uniforms) {
		rfl::visit([&](const auto& type) {
			if (!context.gui_uniform_values.contains(type.name()))
				context.gui_uniform_values[type.name()] = UniformValueBase::create_gui_editable(type);
		}, uniform);
	}
}

void RenderPass::drawGui(RenderContext& context) const
{
	initialize_gui_uniforms(context);

	if (!ImGui::Begin("Shader Sandbox")) {
		ImGui::End();
		return;
	}
	for (const auto& uniform : gui_uniforms) {
		rfl::visit([&](const auto& type) {
			context.gui_uniform_values.at(type.name())->imgui_edit(type.name().c_str());
		}, uniform);
	}
	ImGui::End();
}

void RenderPass::execute(RenderContext& context) const
{
	if (context.updated) {
		for (auto&& [_, shader] : context.assets.of_type("shader"))
			shader.as<dk::gfx::Shader>().clearTextureUnit();
		context.updated = false;
	}
	if (!context.frame)
		throw std::logic_error("RenderContext::begin_frame must be called before executing a render pass");

	initialize_gui_uniforms(context);
	const auto window_size = context.frame->viewport().size();
	if (!runtime.get()) {
		auto initialized = std::make_shared<Runtime>();
		initialized->initialize(textures, frame_buffers, cameras, window_size);
		initialized->update(frame_buffers, cameras, context, window_size);
		try {
			if (preprocess_draw_calls)
				for (const auto& draw_call : *preprocess_draw_calls)
					rfl::visit([&](const auto& call) { call.execute(context, *initialized); }, draw_call);
		}
		catch (...) {
			// Shader texture bindings may refer to the discarded runtime.
			context.updated = true;
			throw;
		}
		// Publish only after preprocessing succeeds; a failed load can be retried.
		runtime = std::move(initialized);
	}
	else {
		runtime.get()->update(frame_buffers, cameras, context, window_size);
	}
	for (const auto& draw_call : draw_calls)
		rfl::visit([&](const auto& call) { call.execute(context, *runtime.get()); }, draw_call);
}
