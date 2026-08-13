#pragma once
#include "examples/common/src/example_application.h"

#include "game_state.h"
#include "view.h"

#include <imgui.h>
#include <devkit/io/window.h>
#include <devkit/io/input_combination.h>

#include <devkit/gfx/frame_buffer.h>
#include <devkit/gfx/vertex_sink.h>

class ClientBase 
	: public ExampleApplicationBase
{
public:
	ClientBase(
		std::unique_ptr<GameState>&& state = std::make_unique<GameState>())
		: m_state(std::move(state))
	{ }

	virtual void update(const dk::io::Frame& frame) { };
	virtual void render(const dk::io::Frame& frame) 
	{ 
		dk::gfx::backBuffer().clear(dk::gfx::Clear::Color, dk::colors::gray);
	};

	void stop()
	{ m_stopRequested = true; }

	bool stopRequested() const
	{
		return m_stopRequested || (m_initialized && !m_window.isOpen());
	}

	void run()
	{
		while (!stopRequested())
		{
			step();
		}
	}

	void step()
	{
		if (!m_initialized)
		{
			setup();
			m_window.open(std::stoi(ini_or("graphics", "msaa", "1")));
			dk::gfx::backBuffer().config(dk::gfx::FrameBuffer::DepthTest::Enabled);
			m_initialized = true;
		}
		const auto& frame = m_window.beginFrame();
		update(frame);
		render(frame);
		m_window.endFrame();
	}

	virtual ~ClientBase()
	{
		if (m_window.isOpen())
			m_window.close();
		teardown();
	}

protected:
	dk::io::AssetManager       m_assets;
	dk::io::InputManager       m_inputs;
	dk::gfx::ShaderCollection  m_shaders;
	View                       m_view;
	std::unique_ptr<GameState> m_state;
	dk::io::Window             m_window;

	virtual void setup()    { };
	virtual void teardown() { };

private:
	bool m_stopRequested = false;
	bool m_initialized = false;

	mINI::INIStructure m_ini;
};

// Immediate mode, RAII
class ImGuiMultiChoiceButtons
{
public:
	ImGuiMultiChoiceButtons(int& selected, const char* id)
		: m_selected(selected)
	{
		ImGui::BeginGroup();
		ImGui::PushID(id);

		// Style setup: selected vs unselected states
		m_style = &ImGui::GetStyle();
		m_colSelected      = ImGui::GetColorU32(ImGuiCol_ButtonActive) 
			? m_style->Colors[ImGuiCol_ButtonActive] 
			: ImVec4(0.26f,0.59f,0.98f,1.0f);
			m_colSelectedHover = m_style->Colors[ImGuiCol_ButtonHovered];
			m_colUnsel         = m_style->Colors[ImGuiCol_Button];
			m_colUnselHover    = m_style->Colors[ImGuiCol_ButtonHovered];
	}

	void item(auto itemFunctor, int itemCount)
	{
		ImGui::PushID(m_index);

		// Set colors based on whether item is selected
		const bool isSelected = (m_index == m_selected);
		ImGui::PushStyleColor(ImGuiCol_Button,        isSelected ? m_colSelected      : m_colUnsel);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, isSelected ? m_colSelectedHover : m_colUnselHover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive,  isSelected ? m_colSelected      : m_colUnsel);

		if (itemFunctor() && !isSelected)
			m_selected = m_index;

		ImGui::PopStyleColor(3);

		ImGui::PopID();
		++m_index;
		if (m_index != itemCount)
			ImGui::SameLine();
	}

	~ImGuiMultiChoiceButtons()
	{
		ImGui::PopID();
		ImGui::EndGroup();
	}

private:
	ImGuiStyle* m_style;
	ImVec4      m_colSelected; 
	ImVec4      m_colSelectedHover; 
	ImVec4      m_colUnsel;         
	ImVec4      m_colUnselHover;   

	int& m_selected;
	int  m_index = 0;
};

inline bool ImBeginClearInWindow(const char* windowName, const ImVec2& relativePos)
{
	const auto imguiMainWindowPos = ImGui::GetMainViewport()->WorkPos;
	ImGui::SetNextWindowPos(ImVec2(imguiMainWindowPos.x + relativePos.x, 
		imguiMainWindowPos.y + relativePos.y));
	const auto flags = ImGuiWindowFlags_NoTitleBar   |
		ImGuiWindowFlags_NoResize     |
		ImGuiWindowFlags_NoMove       |
		ImGuiWindowFlags_NoScrollbar  |
		ImGuiWindowFlags_NoCollapse   |
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoSavedSettings;
	return ImGui::Begin(windowName, nullptr, flags);
}
