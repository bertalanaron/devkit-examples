#pragma once

#include <devkit/gfx/camera.h>

#include <array>
#include <vector>

namespace csm {

inline constexpr unsigned max_cascades = 4;

std::array<glm::vec3, 8> frustum_corners(
    const dk::gfx::Camera& camera, float near_distance, float far_distance);
glm::mat4 lightspace_matrix(
    const dk::gfx::Camera& camera, const glm::vec3& sun_direction,
    float near_distance, float far_distance);
std::vector<float> split_depths(float near_distance, float far_distance, unsigned count);

} // namespace csm
