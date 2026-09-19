#include "csm.h"

#include <cmath>
#include <limits>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

namespace csm {

float camera_ndc_depth(const dk::gfx::Camera& camera, float distance)
{
	if (camera.projection == dk::gfx::Camera::Projection::Orthographic)
		return (2.f * distance - (camera.fp + camera.np)) / (camera.fp - camera.np);
	return (camera.fp + camera.np - 2.f * camera.np * camera.fp / distance) /
		(camera.fp - camera.np);
}

std::array<glm::vec3, 8> frustum_corners(
	const dk::gfx::Camera& camera, float near_distance, float far_distance)
{
	const auto inverse_vp = glm::inverse(camera.P() * camera.V());
	std::array<glm::vec3, 8> result{};
	std::size_t index = 0;
	for (const auto distance : { near_distance, far_distance }) {
		const auto ndc_z = camera_ndc_depth(camera, distance);
		for (const auto x : { -1.f, 1.f }) {
			for (const auto y : { -1.f, 1.f }) {
				const auto world = inverse_vp * glm::vec4(x, y, ndc_z, 1.f);
				result[index++] = glm::vec3(world) / world.w;
			}
		}
	}
	return result;
}

glm::mat4 lightspace_matrix(
	const dk::gfx::Camera& main_camera,
	const glm::vec3& sun_direction,
	float near_distance, float far_distance)
{
	const auto corners = frustum_corners(main_camera, near_distance, far_distance);
	glm::vec3 center(0.f);
	for (const auto& corner : corners)
		center += corner;
	center /= static_cast<float>(corners.size());

	const auto direction_length = glm::length(sun_direction);
	if (direction_length <= std::numeric_limits<float>::epsilon())
		throw std::runtime_error("csm_sun.direction must not be zero");
	const auto direction = sun_direction / direction_length;
	const auto light_position = center - direction * (far_distance - near_distance + 50.f);
	const auto up = std::abs(glm::dot(direction, glm::vec3(0, 1, 0))) > 0.95f
		? glm::vec3(1, 0, 0)
		: glm::vec3(0, 1, 0);
	const auto light_view = glm::lookAt(light_position, center, up);

	glm::vec3 minimum(std::numeric_limits<float>::max());
	glm::vec3 maximum(std::numeric_limits<float>::lowest());
	for (const auto& corner : corners) {
		const auto light_space = light_view * glm::vec4(corner, 1.f);
		minimum = glm::min(minimum, glm::vec3(light_space));
		maximum = glm::max(maximum, glm::vec3(light_space));
	}

	constexpr float padding = 10.f;
	return glm::ortho(
		minimum.x - padding, maximum.x + padding,
		minimum.y - padding, maximum.y + padding,
		// Light-view Z is negative in front of the light; ortho expects distances.
        -maximum.z - padding, -minimum.z + padding) * light_view;
}

std::vector<float> split_depths(float near_distance, float far_distance, unsigned count)
{
    if (!std::isfinite(near_distance) || !std::isfinite(far_distance) ||
        near_distance <= 0.f || far_distance <= near_distance || count == 0 || count > max_cascades)
        throw std::runtime_error("invalid cascade range or count");
    std::vector<float> splits;
    splits.reserve(count);
    constexpr float lambda = 0.6f;
    for (unsigned cascade = 1; cascade <= count; ++cascade) {
        const float fraction = static_cast<float>(cascade) / count;
        const float logarithmic = near_distance * std::pow(far_distance / near_distance, fraction);
        const float uniform = near_distance + (far_distance - near_distance) * fraction;
        splits.push_back(cascade == count ? far_distance : logarithmic * lambda + uniform * (1.f - lambda));
    }
    return splits;
}

} // namespace csm
