#pragma once
#include "game_state.h"
#include "game_client.h"
#include "ui_utils.h"
#include "rts_camera_controller.h"

#include <devkit/algo/draw.h>

class EditorClient 
	: public ClientBase
{
private:
	template <typename T>
	using uptr_t = std::unique_ptr<T>;

public:
	enum class Execution {
		Editing, Testing
	};

	class EditStrategyBase {
	public:
		EditStrategyBase(EditorClient* editor)
			: m_editor(editor)
		{ }

		virtual void setup() { }
		virtual void update(const dk::io::Frame& frame) { }
		virtual bool showToolbarIcon() = 0;
		virtual void showOptionsPanel() { }

	protected:
		EditorClient& editor()
		{ return *m_editor; }
	private:
		EditorClient* m_editor;
	};

	class TerrainEditStrategy;
	class ObjectsEditStrategy;

public:
	EditorClient(
		std::unique_ptr<GameState>&& state = std::make_unique<GameState>())
		: ClientBase(std::move(state))
		, m_navmeshGenerationDebugOut(dk::common::id<dk::gfx::RGBAVertex>)
	{ }

	void update(const dk::io::Frame& frame) override
	{
		// Handle close
		if (m_inputs.activated("close"))
			stop();

		m_assets.synchronize();

		// Execution in editing or testing state
		if (m_execution == Execution::Editing)
			updateWhileEditing(frame);
		if (m_execution == Execution::Testing)
			updateWhileTesting(frame);

		// Handle game instance creation and update
		if (m_inputs.activated("run"))
		{
			m_gameClient.emplace(m_state->clone());
			m_execution = Execution::Testing;
		}
		if (m_gameClient)
		{
			if (m_gameClient->stopRequested())
			{
				m_gameClient.reset();
				m_execution = Execution::Editing;
			}
			else
				m_gameClient->step();
			frame.makeCurrent();
		}
	}

	void render(const dk::io::Frame& frame) override
	{
		dk::gfx::backBuffer().clear(dk::gfx::Clear::Color | dk::gfx::Clear::Depth, dk::colors::gray);
		//dk::gfx::backBuffer().config(dk::gfx::FrameBuffer::DepthTest::Enabled);

		// Update camera
		m_view.update(frame, m_cameraController);

		// Set uniforms
		m_shaders["rgba"].uniforms()    << m_view.uniforms();
		m_shaders["terrain"].uniforms() << m_view.uniforms();

		m_navmeshGenerationDebugOut << dk::gfx::draw(dk::geom::edge3{dk::geom::Origin3, dk::geom::axis::X}, dk::colors::red)
			                        << dk::gfx::draw(dk::geom::edge3{dk::geom::Origin3, dk::geom::axis::Y}, dk::colors::lime)
			                        << dk::gfx::draw(dk::geom::edge3{dk::geom::Origin3, dk::geom::axis::Z}, dk::colors::blue);
		m_navmeshGenerationDebugOut.draw(m_shaders["rgba"], dk::gfx::backBuffer());
		
		// Render terrain
		m_shaders["terrain"].uniformTexture("u_grassTexture1", m_assets.get<dk::gfx::Texture2D>("/textures/terrain/grass1.png"));
		m_shaders["terrain"].uniformTexture("u_grassTexture2", m_assets.get<dk::gfx::Texture2D>("/textures/terrain/grass2.png"));
		m_shaders["terrain"].uniformTexture("u_rockTexture" , m_assets.get<dk::gfx::Texture2D>("/textures/terrain/rock.png"));
		m_state->terrain->render(dk::gfx::backBuffer(), m_shaders["terrain"]);

		// Disable ui when testing
		dk::common::ScopeGuard disableUiGuard(
			[&]{
				ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 0.2);
				ImGui::BeginDisabled(m_execution == Execution::Testing);
			}, [&] {
				ImGui::EndDisabled();
				ImGui::PopStyleVar();
				});
		showUI(frame);
	}

private:
	std::optional<GameClient> m_gameClient;
	Execution                 m_execution = Execution::Editing;

	int                                   m_selectedEditStrategyIndex = 0;
	std::vector<uptr_t<EditStrategyBase>> m_editStrategies;

	RTSCameraController m_cameraController;
	dk::gfx::VertexSink m_navmeshGenerationDebugOut;

	void updateWhileEditing(const dk::io::Frame& frame);

	void updateWhileTesting(const dk::io::Frame& frame);

	void showUI(const dk::io::Frame& frame);

	void setup() override;
};
