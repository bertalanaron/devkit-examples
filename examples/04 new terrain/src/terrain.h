#include <devkit/io/frame.h>
#include <devkit/gfx/frame_buffer.h>
#include <devkit/gfx/texture.h>
#include <devkit/common/imgui_helpers.h>

using namespace dk::io;
using namespace dk::gfx;
using namespace dk::common;

//class ResolverLayer {
//public:
//	ResolverLayer()                           = default;
//	ResolverLayer(ResolverLayer&&)            = default;
//	ResolverLayer& operator=(ResolverLayer&&) = default;
//
//	void operator()(dk::gfx::FrameBuffer& input, std::vector<int> colorIndices)
//	{
//		m_frameBuffer.setViewport(input.viewport());
//
//		for (int index : colorIndices)
//		{
//			auto& inputColor = input.color[index];
//			if (!m_frameBuffer.color[index].has_value())
//				m_frameBuffer.color[index] = Texture2D(dk::geom::xy(inputColor.size()), inputColor.channels(), inputColor.format());
//			else
//				m_frameBuffer.color[index].get().resize(dk::geom::xy(inputColor.size()));
//
//			m_frameBuffer.blit(input, Mask::Color, index, index);
//		}
//	}
//
//	Texture2D& get(int colorIndex)
//	{ return m_frameBuffer.color[colorIndex].get<dk::gfx::Texture2D>(); }
//
//private:
//	FrameBuffer m_frameBuffer;
//};

class PostProcessLayer {
public:
	PostProcessLayer()                              = default;
	PostProcessLayer(PostProcessLayer&&)            = default;
	PostProcessLayer& operator=(PostProcessLayer&&) = default;

	PostProcessLayer(const std::string& textureUniform, ShaderSource& fragmentSource)
		: m_textureUniform(textureUniform)
	{ 
		m_shader.source(ShaderSource::postProcessVertexSource());
		m_shader.source(fragmentSource, dk::gfx::ShaderSource::Fragment);
		m_frameBuffer.color[0] = Texture2D(glm::ivec2(100, 100), Channels::RGB);
	}

	Texture2D& operator()(Texture2D& input)
	{
		auto& outTex = m_frameBuffer.color[0].get<Texture2D>();
		outTex.resize(input.size());
		m_shader.uniformTexture(m_textureUniform, input);
		m_frameBuffer.render(m_shader);
		return outTex;
	}

	Shader& shader() { return m_shader; }

	Texture2D& get() { return m_frameBuffer.color[0].get<Texture2D>(); }

private:
	std::string m_textureUniform;
	Shader      m_shader;
	FrameBuffer m_frameBuffer;
};

class Terrain {
public:
	using MaxHeight         = UniqueProperty<float, "u_maxHeight">;
	using HeightOffset      = UniqueProperty<float, "u_heightOffset">;
	using MinTessLevel      = UniqueProperty<int  , "u_minTessLevel">;
	using MaxTessLevel      = UniqueProperty<int  , "u_maxTessLevel">;
	using TessMinDistance   = UniqueProperty<float, "u_tessMinDistance">;
	using TessMaxDistance   = UniqueProperty<float, "u_tessMaxDistance">;
	using ShadeFlat         = UniqueProperty<bool , "u_shadeFlat">;
	using VisualizeNormals  = UniqueProperty<bool , "u_visualizeNormals">;
	using DrawWireframe     = UniqueProperty<bool , "u_drawWireframe">;
	using TextureBombing    = UniqueProperty<bool , "u_textureBombing">;
	using TriplanarSampling = UniqueProperty<bool , "u_triplanarSampling">;
	using ColorAttachmentIndex = UniqueProperty<int  , "ColorAttachmentIndex">;

	class Config : DK_CONFIG_SPECIALIZATION(Terrain,
		MaxHeight, HeightOffset, MinTessLevel, MaxTessLevel, TessMinDistance, 
		TessMaxDistance, ShadeFlat, VisualizeNormals, DrawWireframe, 
		TextureBombing, TriplanarSampling, ColorAttachmentIndex);

	Config config;

public:
	struct Sun {
		std::string shader = "/shaders/terrain_depth_only.shader";
		Camera      camera;
		FrameBuffer framebuffer;

		Sun()
		{
			camera.projection = Camera::Projection::Orthographic;

			framebuffer.config(FrameBuffer::DepthTest::Enabled);
			framebuffer.config(FrameBuffer::Multisample::Enabled);
			framebuffer.color[0] = Texture2D({ 10, 10 }, Channels::RGB, Format::Unsigned8);
			framebuffer.depth    = RenderBuffer({ 10, 10 }, Channels::Depth, Format::Depth24);
		}
	};

public:
	Terrain()                     = default;
	Terrain(Terrain&&)            = default;
	Terrain& operator=(Terrain&&) = default;

	Terrain(AssetManager& assets, const glm::ivec2& size);
	Terrain(AssetManager& assets, const std::filesystem::path& heightmapPath);

	void render(FrameBuffer& out, const Camera& camera, AssetManager& assets, const Frame& frame);

	Texture2D& heightmap() { return *m_heightmap; }

private:
	using TerrainVertex = Vertex<glm::vec3, glm::vec2>;

	Texture2D*       m_heightmap;
	Shader*          m_shader;
	VertexBuffer     m_vertices = dk::common::id<TerrainVertex>;
	Sun              m_sun;
};

// namespace dk::imgui_helpers {
// DK_IMHELPER_SCALAR_CONFIG_MINMAXSTEP(Terrain::MaxHeight      , 0.1,  100, 0.1);
// DK_IMHELPER_SCALAR_CONFIG_MINMAXSTEP(Terrain::HeightOffset   , -64,   64, 0.1);
// DK_IMHELPER_SCALAR_CONFIG_MINMAXSTEP(Terrain::MinTessLevel   ,   0,   64, 1);
// DK_IMHELPER_SCALAR_CONFIG_MINMAXSTEP(Terrain::MaxTessLevel   ,   0,   64, 1);
// DK_IMHELPER_SCALAR_CONFIG_MINMAXSTEP(Terrain::TessMinDistance,   0, 1000, 0.1);
// DK_IMHELPER_SCALAR_CONFIG_MINMAXSTEP(Terrain::TessMaxDistance,   0, 1000, 0.1);
// DK_IMHELPER_SCALAR_CONFIG_MINMAXSTEP(Terrain::ColorAttachmentIndex, 0, 2, 1);
// }
