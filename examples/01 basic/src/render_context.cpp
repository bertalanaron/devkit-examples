#include "render_context.h"

#include "runtime.h"

#include <devkit/gfx/frame_buffer.h>
#include <devkit/io/asset_factories/scene.h>
#include <devkit/io/asset_factories/shader.h>
#include <devkit/io/asset_factories/texture.h>

#include <chrono>
#include <format>

struct RenderPassFactory : dk::io::assets::PODFactory<RenderPass> {
	RenderPassFactory(RenderContext& render_context)
		: m_render_context(render_context)
	{ }

	void modify(RenderPass& rp, dk::io::assets::ModificationContext& ctx) {
		dk::io::assets::PODFactory<RenderPass>::modify(rp, ctx);
		rp.reset_runtime();
		m_render_context.updated = true;
	}

	RenderContext& m_render_context;
};

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
	assets.register_factory("render_pass", RenderPassFactory(*this));
}

void RenderContext::begin_frame(const dk::io::Frame& current_frame)
{
	frame = &current_frame;
	time += current_frame.dt<std::chrono::seconds>();
}
