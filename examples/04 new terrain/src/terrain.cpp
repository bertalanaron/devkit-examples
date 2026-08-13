#include "terrain.h"

//Terrain::Terrain(AssetManager& assets, const glm::ivec2& size)
//	: m_heightmap(size, Channels::R, Format::Unsigned8)
//{ }

Terrain::Terrain(AssetManager& assets, const std::filesystem::path& heightmapPath)
	: m_heightmap(&assets.get<Texture2D>(heightmapPath))
	, m_shader(&assets.get<Shader>("/shaders/terrain.shader"))
{
	config(MaxHeight(8));
	config(HeightOffset(0));
	config(MinTessLevel(4));
	config(MaxTessLevel(64));
	config(TessMaxDistance(150));
	config(TessMinDistance(0));

	m_heightmap->config(Texture::MagFilter::Linear);
	m_heightmap->config(Texture::MinFilter::LinearMipmapLinear);

	glm::dvec2 size(500, 200);
	unsigned rezX = 50;
	unsigned rezY = size.y / size.x * (double)rezX;
	for(unsigned i = 0; i <= rezX-1; i++)
	{
		for(unsigned j = 0; j <= rezY-1; j++)
		{
			m_vertices.modify().push_back(TerrainVertex{ 
				glm::vec3(-size.x/2.0f + size.x*i/(float)rezX, 0.0f, -size.y/2.0f + size.y*j/(float)rezY), 
				glm::vec2(i / (float)rezX, j / (float)rezY) });

			m_vertices.modify().push_back(TerrainVertex{ 
				glm::vec3(-size.x/2.0f + size.x*(i+1)/(float)rezX, 0.0f, -size.y/2.0f + size.y*j/(float)rezY), 
				glm::vec2((i+1) / (float)rezX, j / (float)rezY) });

			m_vertices.modify().push_back(TerrainVertex{ 
				glm::vec3(-size.x/2.0f + size.x*i/(float)rezX, 0.0f, -size.y/2.0f + size.y*(j+1)/(float)rezY), 
				glm::vec2(i / (float)rezX, (j+1) / (float)rezY) });

			m_vertices.modify().push_back(TerrainVertex{ 
				glm::vec3(-size.x/2.0f + size.x*(i+1)/(float)rezX, 0.0f, -size.y/2.0f + size.y*(j+1)/(float)rezY), 
				glm::vec2((i+1) / (float)rezX, (j+1) / (float)rezY) });
		}
	}
}

void Terrain::render(FrameBuffer& out, const Camera& camera, AssetManager& assets, const Frame& frame)
{
	const glm::vec3 sun_direction = glm::normalize(glm::vec3(1, -1, 1));
	m_sun.camera.lookat = camera.position;
	m_sun.camera.position = camera.position - sun_direction * 40.f;

	// Render scene from the sun's point of view
	{
		// Setup shader
		auto& sun_shader = assets.get<Shader>(m_sun.shader);
		// Setup uniforms
		sun_shader.uniforms().set("u_userView", camera.V());
		sun_shader.uniforms().set("u_sun.view", m_sun.camera.V());
		sun_shader.uniforms().set("u_sun.projection", m_sun.camera.P());
		sun_shader.uniforms().set("u_model", glm::identity<glm::mat4>());
		sun_shader.uniformTexture("u_heightMap", heightmap());
		sun_shader.uniforms().set("u_terrainSize", glm::vec2(500, 200));
		// Set uniforms from config
		config.for_each([&](const auto& prop) {
			if (!config.dirty(prop)) return;
			const std::string_view name = config.property_name(prop);
			if (name.find("u_") == name.npos) return;
			const std::string namestr(name.cbegin(), name.cend());
			sun_shader.uniforms().set(namestr, config.property_value(prop));
		});
		// Set shader layout
		sun_shader.layout(m_vertices);

		// Resize buffer and render terrain
		m_sun.framebuffer.resize(frame.viewport().size());
		m_sun.framebuffer.setViewport(frame.viewport());
		m_sun.framebuffer.clear(Clear::Color | Clear::Depth);
		m_sun.framebuffer.render(sun_shader, m_vertices, Primitive::Patches);
	}
	
	// Render scene from the players point of view
	{
		// Setup shader
		auto& shader = *m_shader;
		// Setup uniforms
		shader.uniforms().set("u_camera.view",       camera.V());
		shader.uniforms().set("u_camera.projection", camera.P());
		shader.uniforms().set("u_camera.position",   camera.position);
		shader.uniforms().set("u_camera.direction",  camera.lookat - camera.position);
		m_shader->uniforms().set("u_model", glm::identity<glm::mat4>());
		m_shader->uniformTexture("u_heightMap", heightmap());
		m_shader->uniforms().set("u_terrainSize", glm::vec2(500, 200));
		// Set uniforms from config
		config.for_each([this](const auto& prop) {
			if (!config.dirty(prop)) return;
			const std::string_view name = config.property_name(prop);
			if (name.find("u_") == name.npos) return;
			const std::string namestr(name.cbegin(), name.cend());
			m_shader->uniforms().set(namestr, config.property_value(prop));
		});
		config.reset_dirty();

		// Set vertex layout
		m_shader->layout(m_vertices);

		// Setup textures
		auto& terrainTex = assets.get<Texture2D>("/textures/grass.png");
		terrainTex.config(Texture::MagFilter::Linear);
		terrainTex.config(Texture::MinFilter::LinearMipmapLinear);
		auto& terrainTex2 = assets.get<Texture2D>("/textures/rock.png");
		terrainTex2.config(Texture::MagFilter::Linear);
		terrainTex2.config(Texture::MinFilter::LinearMipmapLinear);
		auto& terrainTex3 = assets.get<Texture2D>("/textures/snow.png");
		terrainTex3.config(Texture::MagFilter::Linear);
		terrainTex3.config(Texture::MinFilter::LinearMipmapLinear);
		m_shader->uniformTexture("u_terrainTexture", assets.get<Texture2D>("/textures/grass.png"));
		m_shader->uniformTexture("u_terrainTexture2", assets.get<Texture2D>("/textures/rock.png"));
		m_shader->uniformTexture("u_terrainTexture3", assets.get<Texture2D>("/textures/snow.png"));
		m_shader->uniformTexture("u_simplexNoise", assets.get<Texture2D>("/textures/simplex_noise.png"));

		// Resize buffer and render terrain
		out.clear(Clear::Color | Clear::Depth, dk::colors::black);
		out.render(*m_shader, m_vertices, Primitive::Patches);
	}

	if (ImGui::Begin("sun view")) {
		auto& tex = m_sun.framebuffer.color[0].get<Texture2D>();
		dk::imgui_helpers::draw(tex);
	} ImGui::End();
}
