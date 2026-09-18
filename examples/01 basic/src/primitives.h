#pragma once

#include <devkit/gfx/frame_buffer.h>
#include <devkit/gfx/shader.h>

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <rfl.hpp>

namespace primitives {

namespace transforms {

struct RotateAroundAxis {
	glm::vec3 axis;
	float     angle;
};

using Transform = rfl::Variant<
	rfl::Field<"translate", glm::vec3>,
	rfl::Field<"scale", glm::vec3>,
	rfl::Field<"rotate_around_axis", RotateAroundAxis>>;

} // namespace transforms

struct ColorAttachment {
	using Tag = rfl::Literal<"color">;
	std::optional<int> index;
};

struct DepthAttachment {
	using Tag = rfl::Literal<"depth">;
};

struct StencilAttachment {
	using Tag = rfl::Literal<"stencil">;
};

using FrameBufferAttachmentReference = rfl::TaggedUnion<
	"type", ColorAttachment, DepthAttachment, StencilAttachment>;

namespace textures {

struct RenderBufferDefinition {
	using Tag = rfl::Literal<"RenderBufferDefinition">;
	dk::gfx::Format          format;
	dk::gfx::Channels        channels;
	std::optional<glm::vec2> size;
	std::optional<unsigned>  samples;
};

struct RenderBufferReference {
	using Tag = rfl::Literal<"RenderBufferReference">;
	std::string                              name;
};

struct Texture2DDefinition {
	using Tag = rfl::Literal<"Texture2DDefinition">;
	dk::gfx::Format          format;
	dk::gfx::Channels        channels;
	std::optional<glm::vec2> size;
	std::optional<unsigned>  samples;
};

struct Texture2DReference {
	using Tag = rfl::Literal<"Texture2DReference">;
	std::string                              name;
	std::optional<dk::gfx::Texture::Config> config;
};

struct FrameBufferTexture2DReference {
	using Tag = rfl::Literal<"FrameBufferTexture2DReference">;
	std::string                              frame_buffer;
	FrameBufferAttachmentReference           attachment;
	std::optional<dk::gfx::Texture::Config> config;
};

struct Texture2DAsset {
	using Tag = rfl::Literal<"Texture2DAsset">;
	std::filesystem::path                   asset;
	std::optional<dk::gfx::Texture::Config> config;
};

struct Texture2DArrayDefinition {
	using Tag = rfl::Literal<"Texture2DArrayDefinition">;
	dk::gfx::Format          format;
	dk::gfx::Channels        channels;
	std::optional<glm::vec2> size;
	std::optional<unsigned>  samples;
	int layers;
};

struct Texture2DArrayReference {
	using Tag = rfl::Literal<"Texture2DArrayReference">;
	std::string                              name;
	std::optional<dk::gfx::Texture::Config> config;
};

struct Texture2DArrayLayerReference {
	using Tag = rfl::Literal<"Texture2DArrayLayerReference">;
	std::string                              name;
	std::optional<dk::gfx::Texture::Config> config;
	int layer;
};

struct FrameBufferTexture2DArrayReference {
	using Tag = rfl::Literal<"FrameBufferTexture2DArrayReference">;
	std::string                              frame_buffer;
	FrameBufferAttachmentReference           attachment;
	std::optional<dk::gfx::Texture::Config> config;
	int layer;
};

struct Texture2DArrayAsset {
	using Tag = rfl::Literal<"Texture2DArrayAsset">;
	std::filesystem::path                   asset;
	std::optional<dk::gfx::Texture::Config> config;
};

struct Texture2DArrayAssetLayerReference {
	using Tag = rfl::Literal<"Texture2DArrayAssetLayerReference">;
	std::filesystem::path                   asset;
	std::optional<dk::gfx::Texture::Config> config;
	int layer;
};

using FrameBufferAttachment = rfl::TaggedUnion<
"type",
	RenderBufferDefinition, RenderBufferReference,
	Texture2DDefinition, Texture2DReference,
	Texture2DArrayDefinition, Texture2DArrayReference, Texture2DArrayLayerReference>;
using TextureDefinition = rfl::TaggedUnion<
	"type", Texture2DDefinition, Texture2DArrayDefinition>;

} // namespace textures

struct TextureCollectionDefinition {
	rfl::ExtraFields<textures::TextureDefinition> textures;
};

struct FrameBufferDefinition {
	struct Attachments {
		std::optional<std::map<int, textures::FrameBufferAttachment>> color;
		std::optional<textures::FrameBufferAttachment> depth;
		std::optional<textures::FrameBufferAttachment> stencil;
	};

	std::optional<dk::gfx::FrameBuffer::Config> config;
	std::optional<glm::vec2>                    size;
	std::optional<Attachments>                  attachments;
};

struct FrameBufferReferenceWithConfig {
	std::string                                 name;
	std::optional<dk::gfx::FrameBuffer::Config> config;
};

using FrameBufferReference = rfl::Variant<
	std::string, FrameBufferReferenceWithConfig>;

struct FrameBufferCollectionDefinition {
	FrameBufferDefinition                   back_buffer;
	rfl::ExtraFields<FrameBufferDefinition> frame_buffers;
};

struct ShaderReferenceWithConfig {
	std::filesystem::path                  asset;
	std::optional<dk::gfx::Shader::Config> config;
};

using ShaderReference = rfl::Variant<
	std::filesystem::path, ShaderReferenceWithConfig>;

struct Texture2DUniform {
	std::string                               uniform;
	std::string                               name;
	std::optional<dk::gfx::Texture::Config>  config;
};

struct FrameBufferTexture2DUniform {
	std::string                               uniform;
	std::string                               frame_buffer;
	FrameBufferAttachmentReference            attachment;
	std::optional<dk::gfx::Texture::Config>  config;
};

struct Texture2DAssetUniform {
	std::string                               uniform;
	std::filesystem::path                     asset;
	std::optional<dk::gfx::Texture::Config>  config;
};

struct Texture2DArrayUniform {
	std::string                               uniform;
	std::string                               array;
	std::optional<dk::gfx::Texture::Config>  config;
};

struct Texture2DArrayLayerUniform {
	std::string                               uniform;
	std::string                               name;
	std::optional<dk::gfx::Texture::Config>  config;
	int                                       layer;
};

struct FrameBufferTexture2DArrayUniform {
	std::string                               uniform;
	std::string                               frame_buffer;
	FrameBufferAttachmentReference            attachment;
	std::optional<dk::gfx::Texture::Config>  config;
	int                                       layer;
};

struct Texture2DArrayAssetUniform {
	std::string                               uniform;
	std::filesystem::path                     array_asset;
	std::optional<dk::gfx::Texture::Config>  config;
};

struct Texture2DArrayAssetLayerUniform {
	std::string                               uniform;
	std::filesystem::path                     asset;
	std::optional<dk::gfx::Texture::Config>  config;
	int                                       layer;
};

using ShaderUniformTexture = rfl::Variant<
	Texture2DUniform, FrameBufferTexture2DUniform, Texture2DAssetUniform,
	Texture2DArrayUniform, Texture2DArrayLayerUniform,
	FrameBufferTexture2DArrayUniform, Texture2DArrayAssetUniform,
	Texture2DArrayAssetLayerUniform>;

struct MainCamera {
	using Tag = rfl::Literal<"main">;
	std::string          name;
	float                np = 0.1f;
	float                fp = 100.f;
	std::optional<float> fov;
};

struct CsmSunCamera {
	using Tag = rfl::Literal<"csm_sun">;
	std::string          name;
	float                np = 0.1f;
	float                fp = 100.f;
	std::optional<float> fov;
	std::optional<unsigned> cascade_count;
	std::optional<glm::vec3> direction;
	std::optional<unsigned> shadow_resolution;
};

using CameraBinding = rfl::TaggedUnion<"type", MainCamera, CsmSunCamera>;

} // namespace primitives
