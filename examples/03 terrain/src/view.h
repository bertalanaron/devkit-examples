#pragma once
#include <devkit/io/input_combination.h>
#include <devkit/io/frame.h>
#include <devkit/gfx/camera.h>
#include <devkit/gfx/viewport.h>
#include <devkit/gfx/uniforms.h>

class View {
public:
	void update(const dk::io::Frame& frame, auto& cameraController)
	{
		m_currentFrame = &frame;
		cameraController(m_camera, frame);
		m_uc.set("u_camera.VP"       , m_camera.P() * m_camera.V());
		m_uc.set("u_camera.position" , m_camera.position);
		m_uc.set("u_camera.direction", m_camera.lookat - m_camera.position);
		m_uc.set("u_viewport.size"   , (glm::vec2)frame.viewport().size());
		m_uc.set("u_viewport.offset" , (glm::vec2)frame.viewport().offset());
		m_uc.set("u_cursor"          , frame.cursorN());
	}

	const auto cursorRay() const
	{ return m_camera.castRay(m_currentFrame->cursorN()); }

	const dk::gfx::UniformCollection& uniforms() const
	{ return m_uc; }

	const auto& frame() const
	{ return *m_currentFrame; }

	auto& camera()
	{ return m_camera; }

private:
	const dk::io::Frame*       m_currentFrame = nullptr;
	dk::gfx::Camera            m_camera;
	dk::gfx::UniformCollection m_uc;
};
