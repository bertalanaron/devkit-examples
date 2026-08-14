#include "render_context.h"

#include "primitives.h"

#include <devkit/gfx/frame_buffer.h>
#include <devkit/io/asset_factories/scene.h>
#include <devkit/io/asset_factories/shader.h>
#include <devkit/io/asset_factories/texture.h>

#include <chrono>
#include <format>
#include <stdexcept>

RenderContext::RenderContext()
	: assets(dk::io::assets::Yaml{}, ".asset.yaml")
{
	const auto ini_path = dk::common::program_location() /
		std::format("{}.ini", dk::common::program_name());
	assets.root_via_ini(ini_path, "data", "path");
	assets.register_factory("texture", dk::io::assets::factories::Texture2DFactory());
	assets.register_factory("cubemap", dk::io::assets::factories::CubemapFactory());
	assets.register_factory("shader", dk::io::assets::factories::ShaderFactory());
	assets.register_factory("scene", dk::io::assets::factories::SceneFactory());
	assets.register_factory("render_pass", dk::io::assets::PODFactory<RenderPass>());
}

void RenderContext::begin_frame(const dk::io::Frame& current_frame)
{
	frame = &current_frame;
	time += current_frame.dt<std::chrono::seconds>();
}

void RenderContext::update_builtin_uniforms()
{
	if (!frame)
		throw std::logic_error("RenderContext::begin_frame must be called before updating uniforms");

	uniform_values["u_camera.VP"] = UniformValueBase::create_builtin(camera.P() * camera.V());
	uniform_values["u_camera.position"] = UniformValueBase::create_builtin(camera.position);
	uniform_values["u_camera.direction"] = UniformValueBase::create_builtin(camera.lookat - camera.position);
	uniform_values["u_camera.fov"] = UniformValueBase::create_builtin(camera.fov);
	uniform_values["u_t"] = UniformValueBase::create_builtin(time);
	uniform_values["u_dt"] = UniformValueBase::create_builtin(frame->dt<std::chrono::seconds>());
	uniform_values["u_window.size"] = UniformValueBase::create_builtin(
		(glm::vec2)frame->viewport().size());
}
