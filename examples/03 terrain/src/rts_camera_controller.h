#pragma once
#include <devkit/io/input_combination.h>
#include <devkit/io/frame.h>
#include <devkit/gfx/camera.h>
#include <devkit/gfx/viewport.h>

class RTSCameraController {
public:
	bool                        enabled = true;
	dk::io::InputManager        inputs;
	double                      shiftRate = 2.;
	double                      tiltRate = .004;

	void operator()(dk::gfx::Camera& camera, const dk::io::Frame& frame)
	{
		if (!enabled)
			return;

		// Tilt
		if (inputs.active("tilt") && !inputs.active("shift"))
		{
			const auto tiltBy = (glm::vec2)frame.cursorDeltaP() * glm::vec2(tiltRate, tiltRate);
			dk::gfx::Camera::Orbit::tilt(camera, tiltBy);
		}

		// Shift by pointing to window edge
		if (!inputs.active("shift"))
		{
			glm::dvec2 edgeDirection(0, 0);
			if (frame.cursorP().x == 0) edgeDirection.x =  1.;
			if (frame.cursorP().y == 0) edgeDirection.y = -1.;
			if (frame.cursorP().x == frame.viewport().size().x - 1) edgeDirection.x = -1.;
			if (frame.cursorP().y == frame.viewport().size().y - 1) edgeDirection.y =  1.;
			const glm::dvec3 right   = glm::normalize(glm::cross(dk::geom::axis::Y, glm::dvec3(camera.lookat - camera.position)));
			const glm::dvec3 forward = glm::normalize(glm::cross(dk::geom::axis::Y, right));
			const glm::dvec3 direction = right * (double)edgeDirection.x + forward * (double)edgeDirection.y;
			const glm::dvec3 shift = direction * frame.dt<std::chrono::seconds>() * shiftRate * (double)glm::length(camera.lookat - camera.position);
			dk::gfx::Camera::Orbit::shift(camera, shift);
		}

		// Shift by dragging
		if (inputs.active("shift")) 
			[&] {
			auto cursor = frame.cursorP();
			if (frame.viewport().wrapPoint(cursor, 1))
			{
				frame.warpCursor(cursor);
				return;
			}
			const auto cursorProjection     = dk::geom::intersection(camera.castRay(frame.cursorN()), dk::geom::plane::Y());
			const auto prevCursorProjection = dk::geom::intersection(camera.castRay(frame.cursorN() - frame.cursorDeltaN()), dk::geom::plane::Y());
			const auto shift = prevCursorProjection - cursorProjection;
			dk::gfx::Camera::Orbit::shift(camera, shift);
		}();

		// Zoom
		if (dk::io::wheel::up && !dk::io::button::middle)
			dk::gfx::Camera::Orbit::zoom(camera, 0.9);
		if (dk::io::wheel::down && !dk::io::button::middle)
			dk::gfx::Camera::Orbit::zoom(camera, 1.1);

		// Set camera aspect ratio
		camera.asp = frame.viewport().aspectRatio();
	}
};
