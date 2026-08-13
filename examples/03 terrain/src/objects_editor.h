#pragma once
#include "editor_client.h"

class EditorClient::ObjectsEditStrategy
	: public EditorClient::EditStrategyBase
{
public:
	using EditStrategyBase::EditStrategyBase;

	void setup() override
	{
		editor().m_inputs.define("select_edit_strategy_objects", 
			dk::io::modkey::alt + dk::io::key::_2);
	}

	bool showToolbarIcon() override
	{
		bool result = ImGui::Button("objects")
			|| editor().m_inputs.activated("select_edit_strategy_objects");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("alt + 2");
		}
		return result;
	}

	void showOptionsPanel() override
	{
		ImGui::Text("Edit objects");
	}
};
