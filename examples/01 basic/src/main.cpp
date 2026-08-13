#include "examples/common/src/example_application.h"

#include <devkit/io/window.h>
#include <devkit/gfx/frame_buffer.h>
#include <devkit/algo/geometry.h>
#include <devkit/algo/draw.h>
#include <devkit/gfx/vertex_buffer.h>
#include <devkit/gfx/element_buffer.h>
#include <devkit/io/asset_manager.h>
#include <devkit/io/input_combination.h>
#include <devkit/gfx/shader.h>
#include <devkit/gfx/mesh.h>
#include <devkit/gfx/scene.h>
#include <devkit/gfx/camera.h>
#include <devkit/gfx/texture.h>
//#include <devkit/gfx/font.h>
#include <devkit/gfx/vertex_sink.h>
#include <devkit/io/frame.h>

#include <random>

#include <imgui.h>

#include <magic_enum/magic_enum.hpp>
#include <yaml-cpp/yaml.h>

#include <assimp/scene.h>
#include <assimp/Importer.hpp>

#include <mini/ini.h>

#include <nfd.h>

#include <fstream>

template <typename T>
class AssetSelector {
public:
	AssetSelector(std::filesystem::path initial)
		: m_selected(initial.lexically_normal().string())
	{  }

	T& get(dk::io::AssetManager& assets)
	{
		const int MaxDropDownSize = 200;
		std::string label = std::format("###Asset{}_{:x}", typeid(T).name(), reinterpret_cast<intptr_t>(this));
		
		ImGui::Image(assets.get<T>(m_selected).handle(), ImVec2(18, 18));
		ImGui::SameLine();

		ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, FLT_MAX));
		if (ImGui::BeginCombo(label.c_str(), m_selected.c_str())) {
			//if (ImGui::IsWindowAppearing())
			//	ImGui::SetKeyboardFocusHere();
			ImGui::InputTextWithHint("##SearchBox", "Search...", m_searchString.data(), IM_ARRAYSIZE(m_searchString.data()));

			ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, MaxDropDownSize));
			if (ImGui::BeginChild("##ComboOptionsChild", ImVec2(-FLT_MIN, 0.0f), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize)) {
				for (auto [path, asset] : assets.each<T>()) {
					//if (!dk::common::fs::is_parent(path, assets.root() / "textures/planets"))
					//	continue;
					const auto relativePath = assets.relativeToRoot(path).lexically_normal().string();
					if (!relativePath.contains(m_searchString))
						continue;
					bool isSelected = (relativePath == m_selected);
					ImGui::Image(asset.handle(), ImVec2(18, 18));
					ImGui::SameLine();
					if (ImGui::Selectable(relativePath.c_str(), isSelected)) {
						m_selected = relativePath;
						ImGui::CloseCurrentPopup();
					}
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndChild();
			}
			ImGui::EndCombo();
		}

		return assets.get<T>(m_selected);
	}

private:
	std::string m_selected;
	std::string m_searchString;
};

class PostProcessLayer {
public:
	PostProcessLayer()                              = default;
	PostProcessLayer(PostProcessLayer&&)            = default;
	PostProcessLayer& operator=(PostProcessLayer&&) = default;

	PostProcessLayer(const std::string& textureUniform, dk::gfx::ShaderSource& fragmentSource)
		: m_textureUniform(textureUniform)
	{ 
		m_shader.source(dk::gfx::ShaderSource::postProcessVertexSource());
		m_shader.source(fragmentSource, dk::gfx::ShaderSource::Fragment);
		m_frameBuffer.color[0] = dk::gfx::Texture2D(glm::ivec2(100, 100), dk::gfx::Channels::RGB);
	}

	dk::gfx::Texture2D& operator()(dk::gfx::Texture2D& input)
	{
		auto& outTex = m_frameBuffer.color[0].get<dk::gfx::Texture2D>();
		outTex.resize(input.size());
		m_shader.uniformTexture(m_textureUniform, input);
		m_frameBuffer.render(m_shader);
		return outTex;
	}

	dk::gfx::Shader& shader() { return m_shader; }

	dk::gfx::Texture2D& get()
	{ return m_frameBuffer.color[0].get<dk::gfx::Texture2D>(); }

private:
	std::string          m_textureUniform;
	dk::gfx::Shader      m_shader;
	dk::gfx::FrameBuffer m_frameBuffer;
};

class ResolverLayer {
public:
	ResolverLayer()                           = default;
	ResolverLayer(ResolverLayer&&)            = default;
	ResolverLayer& operator=(ResolverLayer&&) = default;

	ResolverLayer(dk::gfx::Channels channels)
	{ 
		m_frameBuffer.color[0] = dk::gfx::Texture2D(glm::ivec2(100, 100), dk::gfx::Channels::RGB);
	}

	dk::gfx::Texture2D& operator()(dk::gfx::FrameBuffer& input, int colorIndex = 0)
	{
		auto&      outTex    = m_frameBuffer.color[0].get<dk::gfx::Texture2D>();
		const auto inputSize = dk::geom::xy(input.color[0].size());

		m_frameBuffer.setViewport(inputSize);
		outTex.resize(inputSize);
		
		m_frameBuffer.blit(input);
		return outTex;
	}

	dk::gfx::Texture2D& get()
	{ return m_frameBuffer.color[0].get<dk::gfx::Texture2D>(); }

private:
	dk::gfx::Channels    m_channels;
	dk::gfx::FrameBuffer m_frameBuffer;
};


using namespace dk::gfx;
using namespace dk::io;

class PlanetScene {
public:
	PlanetScene()                         = default;
	PlanetScene(PlanetScene&&)            = default;
	PlanetScene& operator=(PlanetScene&&) = default;

	PlanetScene(AssetManager& assets, int asteroidCount)
		: m_assets(&assets)
	{
		m_asteroidTransforms = dk::common::id<Vertex<glm::mat4>>;
		placeAsteroids(asteroidCount);
	}

	void setup()
	{
		// Setup shaders
		m_shaders.insert("planet"  , m_assets->getMultiple<ShaderSource>("/shaders/mesh_vs.glsl"          , "/shaders/textured_fs.glsl"));
		m_shaders.insert("asteroid", m_assets->getMultiple<ShaderSource>("/shaders/instanced_mesh_vs.glsl", "/shaders/asteroid_fs.glsl"));
		m_shaders.insert("skybox"  , m_assets->getMultiple<ShaderSource>("/shaders/skybox_vs.glsl"        , "/shaders/skybox_fs.glsl"));

		auto& scene = m_assets->get<Scene>("/models/planet_scene.fbx");

		// Set planet layout
		auto& planetMesh = scene["/planet"][0](m_vertexFlags);
		m_shaders["planet"].layout(planetMesh);

		// Set asteroid texture
		auto& asteroidTexture = m_assets->get<Texture2D>("/textures/Asteroid2b_Color.png");
		asteroidTexture.config(Texture::MinFilter::NearestMipmapLinear);
		m_shaders["asteroid"].uniformTexture("u_texture", asteroidTexture);
		// Set asteroid layout
		m_asteroidMesh = &*m_assets->get<Scene>("/models/asteroid_2b_lower.obj").meshes().begin();
		m_shaders["asteroid"].layout(m_asteroidMesh->vertices, perInstance(m_asteroidTransforms));

		// Set skybox texture
		m_skybox = Cubemap({
			(m_assets->root() / "textures/skybox/right.png").string(),
			(m_assets->root() / "textures/skybox/left.png").string(),
			(m_assets->root() / "textures/skybox/top.png").string(),
			(m_assets->root() / "textures/skybox/bottom.png").string(),
			(m_assets->root() / "textures/skybox/front.png").string(),
			(m_assets->root() / "textures/skybox/back.png").string()
			});
		m_skybox.config(Texture::MinFilter::Linear);
		m_shaders["skybox"].uniformTexture("u_skybox", m_skybox);
		// Set skybox layout
		auto& unitCubeMesh = scene["/skybox"][0]();
		m_shaders["skybox"].layout(unitCubeMesh.vertices);
	}

	void update(const Frame& frame, const Camera& camera, Texture2D& planetTexture)
	{
		// Set up camera uniforms
		m_cameraUniforms.set("u_camera.VP",        camera.P() * camera.V());
		m_cameraUniforms.set("u_camera.position",  camera.position);
		m_cameraUniforms.set("u_camera.direction", camera.lookat - camera.position);

		// Setup planet shader textures
		m_shaders["planet"].uniformTexture("u_texture", planetTexture);
		planetTexture.config(Texture::MagFilter::Linear);
		// Use specular and normal maps when earth is selected
		if (&planetTexture == &m_assets->get<Texture2D>("textures/planets/earth.png")) {
			m_shaders["planet"].uniforms().set("u_useSpecular", (int)true);
			m_shaders["planet"].uniformTexture("u_normal"  , m_assets->get<Texture2D>("textures/planets/earth_normal_map.png"));
			m_shaders["planet"].uniformTexture("u_specular", m_assets->get<Texture2D>("textures/planets/earth_specular_map.png"));
		}
		else {
			m_shaders["planet"].uniforms().set("u_useSpecular", (int)false);
		}

		auto& scene = m_assets->get<dk::gfx::Scene>("/models/planet_scene.fbx");

		// Setup planet shader
		m_shaders["planet"].uniforms() << m_cameraUniforms;

		// Setup asteroids
		m_shaders["asteroid"].uniforms() << m_cameraUniforms;
		// Rotate around y axis
		static float t = 0;
		t += frame.dt<std::chrono::seconds>() / 10.0;
		m_shaders["asteroid"].uniforms().set("u_t", t);

		// Setup skybox
		m_shaders["skybox"].uniforms() << m_cameraUniforms;
	}

	void render(FrameBuffer& output)
	{
		// Configure output framebuffer
		output.config(FrameBuffer::DepthTest::Enabled);
		output.config(FrameBuffer::DepthFunc::Lequal);
		output.config(FrameBuffer::Multisample::Enabled);
		output.config(FrameBuffer::SampleShading::Enabled);
		output.config(FrameBuffer::CullFace::Enabled);

		// Clear canvas
		output.clear(Clear::Color | Clear::Depth, dk::colors::gray);

		auto& scene = m_assets->get<dk::gfx::Scene>("/models/planet_scene.fbx");

		// Render planet
		auto& indices = scene["/planet"][0](m_vertexFlags).indices;
		output.render(m_shaders["planet"], indices, dk::gfx::Primitive::Triangles);

		// Render asteroids
		output.render(m_shaders["asteroid"], m_asteroidMesh->indices, dk::gfx::Primitive::Triangles, m_asteroidTransforms.size());

		// Render skybox
		auto& unitCubeMesh = scene["/skybox"][0]();
		output.render(m_shaders["skybox"], unitCubeMesh.indices, dk::gfx::Primitive::Triangles);
	}

private:
	AssetManager*     m_assets;
	ShaderCollection  m_shaders;
	VertexFlags       m_vertexFlags = VertexFlags::Position | VertexFlags::Normal | VertexFlags::TexCoords;

	UniformCollection m_cameraUniforms;
	Cubemap           m_skybox;
	
	VertexBuffer      m_asteroidTransforms;
	MeshMask*         m_asteroidMesh;

	void placeAsteroids(int count) 
	{
		srand(0);
		static float t = 0;
		t += 0.0001;

		float radius = 40.0;
		float offset = 4.5f;
		for(unsigned int i = 0; i < count; i++)
		{
			glm::mat4 model = glm::mat4(1.0f);
			// 1. translation: displace along circle with 'radius' in range [-offset, offset]
			float angle = (float)i / (float)count * 280.0f + t;
			float displacement = (rand() % (int)(2 * offset * 100)) / 100.0f - offset;
			float x = sin(angle) * radius + displacement;
			displacement = (rand() % (int)(2 * offset * 100)) / 100.0f - offset;
			float y = displacement * 0.2f; // keep height of field smaller compared to width of x and z
			displacement = (rand() % (int)(2 * offset * 100)) / 100.0f - offset;
			float z = cos(angle) * radius + displacement;
			model = glm::translate(model, glm::vec3(x, y, z));

			// 2. scale: scale between 0.05 and 0.25f
			float scale = (rand() % 20) / 400.0f + 0.05;
			model = glm::scale(model, glm::vec3(scale));

			// 3. rotation: add random rotation around a (semi)randomly picked rotation axis vector
			float rotAngle = (rand() % 360);
			model = glm::rotate(model, rotAngle, glm::vec3(0.4f, 0.6f, 0.8f));

			// 4. now add to list of matrices
			m_asteroidTransforms.modify().push_back(dk::gfx::Vertex(model));
		}  
	}
};

class AsteroidBeltApplication
	: public ExampleApplicationBase
{
public:
	AsteroidBeltApplication()
	{
		// Setup window
		m_window.config(Window::Resize::Enabled);
		m_window.config(Window::Theme::Dark);
		m_window.config(Window::VSync::Disabled);

		// Setup scene framebuffer
		m_sceneBuffer.color[0] = RenderBuffer(m_window.config.get<Window::Size>(), Channels::RGB, Format::Unsigned16, 8);
		m_sceneBuffer.depth    = RenderBuffer(m_window.config.get<Window::Size>(), Channels::Depth, Format::Depth24, 8);

		// Watch for assets in directories
		m_assets.root(ini("data", "path").value(), false);
		m_assets.watch("/textures/"       , false);
		m_assets.watch("/textures/planets", true);
		m_assets.watch("/models"          , false);
		m_assets.watch("/shaders"         , true);

		// Register asset types
		m_assets.type<ShaderSource>("glsl", ShaderSource::load, &ShaderSource::update);
		m_assets.type<Scene>({ "obj", "fbx" }, Scene::load, &Scene::update, std::nullopt, AssetManager::Deferred);
		m_assets.type<Texture2D>("png", Texture2D::load, std::nullopt, std::nullopt, AssetManager::Async);
		m_assets.type<YAML::Node>("yaml", YAML::LoadFile, [](YAML::Node& node, const std::string& path) { 
			node = YAML::LoadFile(path);
			spdlog::info("{}", YAML::Dump(node));
		});
		using JsonFStream = dk::io::FileStream<nlohmann::json>;
		m_assets.type<nlohmann::json>("json", JsonFStream::load, JsonFStream::update, JsonFStream::save);
		m_assets.synchronize();

		// Setup layers
		m_resolveLayer     = ResolverLayer(Channels::RGB);
		m_postProcessLayer = PostProcessLayer("u_texture", m_assets.get<dk::gfx::ShaderSource>("/shaders/chromatic_aberration_fs.glsl"));

		// Create camera asset if doesn't exist
		if (!m_assets.contains("camera.json"))
			m_assets.create("camera.json", nlohmann::json(dk::gfx::Camera(m_camera)));
		m_camera = m_assets.get<nlohmann::json>("camera.json");

		// Setup planet scene
		m_scene = PlanetScene(m_assets, yaml<int>("asteroid_count"));
		m_scene.setup();
	}

	void run()
	{
		// Open window
		m_window.open(std::stoi(ini_or("graphics", "msaa", "1")));

		while (m_window.isOpen())
		{
			const auto& frame = m_window.beginFrame();

			static float t = 0;
			t += frame.dt<std::chrono::seconds>();
			m_postProcessLayer.shader().uniforms().set("u_t", 0);

			// Resize scene framebuffer and set viewport
			m_sceneBuffer.resize(frame.viewport().size());

			// Close window with esc
			if (key::esc) m_window.close();
			// Toggle fullscreen with the f key
			if (key::f(currentInputState()) && !key::f(previousInputState()))
				m_window.config(dk::common::toggle(m_window.config.get<Window::Mode>()));

			moveCamera(frame);
			
			// Update assets
			m_assets.synchronize();

			// Select planet texture in an imgui window
			static AssetSelector<Texture2D> planetTextureSelector("textures/planets/mars.png");
			auto& planetTexture = planetTextureSelector.get(m_assets);

			// Update and render scene
			m_scene.update(frame, m_camera, planetTexture);
			m_scene.render(m_sceneBuffer);

			// Resolve scene and apply postprocessing step
			auto& resolvedScene  = m_resolveLayer(m_sceneBuffer);
			auto& processedScene = m_postProcessLayer(resolvedScene);

			backBuffer().render(processedScene);

			m_window.endFrame();
		}
	}

private:
	Window             m_window;
	AssetManager       m_assets;

	PlanetScene        m_scene;

	FrameBuffer        m_sceneBuffer;
	ResolverLayer      m_resolveLayer;
	PostProcessLayer   m_postProcessLayer;

	Camera             m_camera;
	Camera::Orbit      m_orbit;

	void moveCamera(const dk::io::Frame& frame)
	{
		// Orbit around center with left mouse button
		if (button::left) m_orbit.tilt(m_camera, (glm::vec2)frame.cursorDeltaP() * glm::vec2(.004, .004));
		// Zoom in/out with mouse wheel
		if (wheel::up)   m_orbit.zoom(m_camera, 0.9);
		if (wheel::down) m_orbit.zoom(m_camera, 1.1);

		// Tilt around center with w a s d keys
		if (key::d) m_orbit.tilt(m_camera, { -0.005,      0 });
		if (key::a) m_orbit.tilt(m_camera, {  0.005,      0 });
		if (key::w) m_orbit.tilt(m_camera, {      0,  0.005 });
		if (key::s) m_orbit.tilt(m_camera, {      0, -0.005 });
		// Zoom in/out with the j k keys
		if (key::j) m_orbit.zoom(m_camera, 0.999);
		if (key::k) m_orbit.zoom(m_camera, 1.001);

		// Set aspect ratio
		m_camera.asp = dk::gfx::backBuffer().aspectRatio();
	}

	template <typename T>
	T yaml(const std::string& path) const
	{
		auto& yaml = m_assets.get<YAML::Node>("assets.yaml");
		return yaml[path].as<T>();
	}
};

int main()
{
	spdlog::set_level(spdlog::level::trace);

	AsteroidBeltApplication app;
	app.run();
}
