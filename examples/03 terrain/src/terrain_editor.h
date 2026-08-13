#pragma once
#include "editor_client.h"

class EditorClient::TerrainEditStrategy
	: public EditorClient::EditStrategyBase
{
public:
	using EditStrategyBase::EditStrategyBase;

	void setup() override
	{
		editor().m_inputs.define("select_edit_strategy_terrain", 
			dk::io::modkey::alt + dk::io::key::_1);
		editor().m_inputs.define("raise_terrain_cliff", dk::io::modkey::none + dk::io::button::left);
		editor().m_inputs.define("lower_terrain_cliff", dk::io::modkey::shift + dk::io::button::left);
		editor().m_inputs.define("modify_terrain"     , dk::io::button::left);
	}

	void update(const dk::io::Frame& frame) override
	{
		// Calculate cursors intersection with terrain
		const auto cursor = [&] { 
			const auto ray          = editor().m_view.camera().castRay(frame.cursorN());
			const auto intersection = dk::geom::intersection(ray, dk::geom::plane::Y());
			return dk::geom::xz(intersection); 
		}();
		editor().m_navmeshGenerationDebugOut << dk::gfx::draw(cursor, dk::colors::red, dk::geom::plane::Y(), -dk::geom::axis::X);

		changeTerrainHeight(*editor().m_state->terrain, cursor);
	}

	bool showToolbarIcon() override
	{
		// Show button
		bool result = ImGui::Button("terrain")
			|| editor().m_inputs.activated("select_edit_strategy_terrain");
		// Show hotkey in tooltip
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("alt + 1");
		return result;
	}

	void showOptionsPanel() override
	{
		ImGui::Text("Edit terrain");
	}

private:
	int m_hightAtCursor = 0;

	void changeTerrainHeight(Terrain& terrain, const glm::ivec2& cursor)
	{
		if (!terrain.accessor().isInbounds(cursor))
			return;
		if (editor().m_inputs.activated("modify_terrain")) 
			m_hightAtCursor = terrain[cursor].height;
		if (editor().m_inputs.active("raise_terrain_cliff"))
			terrain.setHeight(cursor, m_hightAtCursor + 1);
		if (editor().m_inputs.active("lower_terrain_cliff"))
			terrain.setHeight(cursor, m_hightAtCursor - 1);
		if (editor().m_inputs.active("modify_terrain"))
			editor().m_navmeshGenerationDebugOut.clear();
	}
};
