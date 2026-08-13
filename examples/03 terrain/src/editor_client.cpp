#include "editor_client.h"

#include "terrain_editor.h"
#include "objects_editor.h"

void EditorClient::setup()
{
	setupAssetManager(m_assets, ini());
	m_assets.synchronize();

	// Setup hotkeys
	m_inputs.define("close", dk::io::key::esc);
	m_inputs.define("run"  , dk::io::key::f5);
	m_cameraController.inputs.define("tilt" , dk::io::modkey::alt);
	m_cameraController.inputs.define("shift", dk::io::modkey::none + dk::io::button::middle);

	// Setup window
	m_window.config(dk::io::Window::Theme::Dark);
	m_window.config(dk::io::Window::Size(1280, 720));

	// Create edit strategies
	m_editStrategies.emplace_back(std::make_unique<TerrainEditStrategy>(this));
	m_editStrategies.emplace_back(std::make_unique<ObjectsEditStrategy>(this));
	for (auto& strategy : m_editStrategies)
		strategy->setup();

	// Set up shaders
	// Debug
	m_shaders.insert("rgba", m_assets.getMultipleWeak<dk::gfx::ShaderSource>("/shaders/rgba_vs.glsl", "/shaders/rgba_fs.glsl"));
	// Terrain
	m_shaders.insert("terrain", m_assets.getMultipleWeak<dk::gfx::ShaderSource>("/shaders/rts/terrain_vs.glsl", "/shaders/rts/terrain_fs.glsl"));

	// Set texture filtering
	for (auto [path, texture] : m_assets.each<dk::gfx::Texture2D>("/textures/terrain"))
	{
		texture.config(dk::gfx::Texture::MinFilter::NearestMipmapLinear);
		texture.config(dk::gfx::Texture::MagFilter::Linear);
	}

	// Setup global debug output
	dk::dbg::store<dk::gfx::VertexSink*, "navmesh_poly_out">() = &m_navmeshGenerationDebugOut;

	m_state->terrain->regenerate();
}

void EditorClient::showUI(const dk::io::Frame& frame)
{
	ImGui::GetStyle().WindowMenuButtonPosition = ImGuiDir_None;
	ImGui::GetStyle().TabRounding = 0.f;
	ImGui::PushStyleColor(ImGuiCol_WindowBg, 0xff1f1f1f);

	if (ImBeginClearInWindow("editor_strategy_options_window", 
		ImVec2(frame.viewport().offset().x, frame.viewport().offsetFromTop())))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
		ImGuiMultiChoiceButtons multiChoice(m_selectedEditStrategyIndex, "editor_strategy_options");
		for (const auto& strategy : m_editStrategies)
			multiChoice.item([&] { return strategy->showToolbarIcon(); }, m_editStrategies.size());
		ImGui::PopStyleVar();
	}
	ImGui::End();

	// Show options panel of selected strategy
	if (ImGui::Begin("Editor Options"))
		m_editStrategies.at(m_selectedEditStrategyIndex)->showOptionsPanel();
	ImGui::End();

	ImGui::ShowDemoWindow();

	ImGui::PopStyleColor();
}

void EditorClient::updateWhileEditing(const dk::io::Frame& frame)
{
	// Update edit strategy
	m_editStrategies[m_selectedEditStrategyIndex]->update(frame);

	// Rebuild navmesh
	m_state->terrain->navmesh().build(*m_state->terrain);
}

void EditorClient::updateWhileTesting(const dk::io::Frame& frame)
{

}
