#include "examples/common/src/example_application.h"
#include "terrain.h"

#include <devkit/io/window.h>
#include <devkit/io/asset_manager.h>
#include <devkit/gfx/shader.h>
#include <devkit/gfx/texture.h>
#include <devkit/gfx/frame_buffer.h>
#include <devkit/gfx/shader.h>
#include <devkit/common/imgui_helpers.h>

using namespace dk::io;
using namespace dk::gfx;

class Editor
	: ExampleApplicationBase
{
public:
	Editor()
	{
		m_assets.root(ini("data", "path").value(), true);

		m_assets.type<Texture2D>("png", Texture2D::load, std::nullopt, std::nullopt, AssetManager::Async);
		m_assets.type<Shader>("shader", loadShaderYAML);
		m_assets.type<ShaderSource>("glsl", ShaderSource::load);
		m_assets.synchronize();

		m_terrain = Terrain(m_assets, "/heightmaps/pilis2.png");
	}

	void run()
	{
		m_window.open(std::stoi(ini_or("graphics", "msaa", "1")));
		
		while (m_window.isOpen())
		{
			const auto& frame = m_window.beginFrame();

			moveCamera(frame);

			if (ImGui::Begin("Terrain Config")) {
				dk::imgui_helpers::edit("##terrain", m_terrain.config);
			} ImGui::End();

			// Close window with esc
			if (key::esc) m_window.close();
			// Toggle fullscreen with the f key
			if (key::f(currentInputState()) && !key::f(previousInputState()))
				m_window.config(dk::common::toggle(m_window.config.get<Window::Mode>()));

			backBuffer().clear(Clear::Color | Clear::Depth, dk::colors::black);
			backBuffer().config(FrameBuffer::DepthTest::Enabled);
			backBuffer().config(FrameBuffer::Multisample::Enabled);
			backBuffer().config(FrameBuffer::SampleShading::Enabled);
			m_terrain.render(backBuffer(), m_camera, m_assets, frame);

			m_window.endFrame();
		}
	}

private:
	Window             m_window;
	AssetManager       m_assets;
	Terrain            m_terrain;

	ShaderCollection   m_shaders;

	Camera             m_camera;
	Camera::Orbit      m_orbit;

	void moveCamera(const dk::io::Frame& frame)
	{
		// Orbit around center with left mouse button
		if (button::left + modkey::none)  m_orbit.tilt(m_camera, (glm::vec2)frame.cursorDeltaP() * glm::vec2(.004, .004));
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

		// Shift by dragging
		if (button::left + modkey::shift) 
			[&] {
			auto cursor = frame.cursorP();
			if (frame.viewport().wrapPoint(cursor, 1))
			{
				frame.warpCursor(cursor);
				return;
			}
			const auto cursorProjection     = dk::geom::intersection(m_camera.castRay(frame.cursorN()), dk::geom::plane::Y());
			const auto prevCursorProjection = dk::geom::intersection(m_camera.castRay(frame.cursorN() - frame.cursorDeltaN()), dk::geom::plane::Y());
			const auto shift = prevCursorProjection - cursorProjection;
			m_orbit.shift(m_camera, shift);
		}();

		// Set aspect ratio
		m_camera.asp = dk::gfx::backBuffer().aspectRatio();
	}
};

int main()
{
	Editor editor;
	editor.run();
}
