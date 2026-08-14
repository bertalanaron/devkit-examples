#pragma once

#include "uniforms.h"

#include <devkit/gfx/camera.h>
#include <devkit/io/asset_manager.h>
#include <devkit/io/frame.h>

#include <memory>
#include <string>
#include <unordered_map>

struct RenderContext {
	RenderContext();

	using UniformValueCollection = std::unordered_map<std::string, std::unique_ptr<UniformValueBase>>;

	void begin_frame(const dk::io::Frame& frame);
	void update_builtin_uniforms();

	UniformValueBase& uniform(const std::string& name)
	{
		if (const auto gui = gui_uniform_values.find(name); gui != gui_uniform_values.end())
			return *gui->second;
		return *uniform_values.at(name);
	}

	dk::io::assets::Manager assets;
	dk::gfx::Camera         camera;
	const dk::io::Frame*    frame = nullptr;
	float                   time  = 0.f;
	UniformValueCollection  uniform_values;
	UniformValueCollection  gui_uniform_values;
};
