#include "primitives.h"

#include <devkit/gfx/frame_buffer.h>
#include <devkit/io/window.h>

#include <exception>
#include <optional>
#include <string>

#include <imgui.h>
#include <spdlog/spdlog.h>

namespace {

void moveCamera(dk::gfx::Camera& camera, const dk::io::Frame& frame)
{
	using namespace dk::io;

	static dk::gfx::Camera::Orbit orbit;

	if (button::left) orbit.tilt(camera, (glm::vec2)frame.cursorDeltaP() * glm::vec2(.004f, .004f));
	if (wheel::up)   orbit.zoom(camera, 0.9f);
	if (wheel::down) orbit.zoom(camera, 1.1f);

	if (key::d) orbit.tilt(camera, { -0.005f, 0.0f });
	if (key::a) orbit.tilt(camera, {  0.005f, 0.0f });
	if (key::w) orbit.tilt(camera, { 0.0f,  0.005f });
	if (key::s) orbit.tilt(camera, { 0.0f, -0.005f });
	if (key::j) orbit.zoom(camera, 0.999f);
	if (key::k) orbit.zoom(camera, 1.001f);

	camera.asp = dk::gfx::backBuffer().aspectRatio();
}

void drawAssetErrorPopup(const std::optional<std::string>& error)
{
	static bool popup_was_open = false;
	if (error.has_value() && !popup_was_open)
		ImGui::OpenPopup("Asset Error");

	bool popup_open = error.has_value();
	if (error.has_value())
		ImGui::SetNextWindowSize(ImVec2(520.0f, 160.0f), ImGuiCond_Appearing);
	if (error.has_value() && ImGui::BeginPopupModal("Asset Error", &popup_open)) {
		ImGui::TextWrapped("%s", error->c_str());
		ImGui::EndPopup();
	}
	popup_was_open = error.has_value() && popup_open;
}

} // namespace

int main()
{
	using namespace dk;

	spdlog::set_level(spdlog::level::trace);
	RenderContext context;

	io::Window window;
	window.open(1);

	while (window.isOpen()) {
		auto& frame = window.beginFrame();
		context.begin_frame(frame);

		std::optional<std::string> asset_error;
		try {
			context.assets.scan_filesystem();
		}
		catch (const std::exception& error) {
			asset_error = error.what();
		}

		for (auto&& [path, asset] : context.assets.all()) {
			try {
				asset.execute_pending_task();
			}
			catch (const std::exception& error) {
				if (!asset_error.has_value())
					asset_error = path.string() + ": " + error.what();
			}
		}

		if (!asset_error.has_value()) {
			moveCamera(context.camera, frame);

			auto& render_pass = context.assets["scenes/asteroid_belt"].as<RenderPass>();
			render_pass.drawGui(context);
			try {
				render_pass.execute(context);
			}
			catch (const std::exception& error) {
				asset_error = "scenes/asteroid_belt: " + std::string(error.what());
			}
		}

		drawAssetErrorPopup(asset_error);

		static bool screenshot_taken = false;
		if (!screenshot_taken) {
			gfx::backBuffer().saveAsPNG(common::program_location() / "screenshot.png");
			screenshot_taken = true;
		}

		window.endFrame();
	}
}
