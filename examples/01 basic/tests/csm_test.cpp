#include "../src/csm.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>

TEST(Csm, CascadeSplitsCoverTheConfiguredRange)
{
    for (unsigned count = 1; count <= csm::max_cascades; ++count) {
        const auto splits = csm::split_depths(0.1f, 100.f, count);
        ASSERT_EQ(splits.size(), count);
        EXPECT_FLOAT_EQ(splits.back(), 100.f);
        float previous = 0.1f;
        for (float split : splits) {
            EXPECT_GT(split, previous);
            previous = split;
        }
    }
}

TEST(Csm, RejectsInvalidCascadeRangesAndCounts)
{
    EXPECT_THROW(csm::split_depths(0.f, 100.f, 3), std::runtime_error);
    EXPECT_THROW(csm::split_depths(1.f, 1.f, 3), std::runtime_error);
    EXPECT_THROW(csm::split_depths(0.1f, 100.f, 0), std::runtime_error);
    EXPECT_THROW(csm::split_depths(0.1f, 100.f, 5), std::runtime_error);
    EXPECT_THROW(csm::split_depths(0.1f, std::numeric_limits<float>::infinity(), 3), std::runtime_error);
}

TEST(Csm, LightProjectionContainsEveryCascadeFrustum)
{
    dk::gfx::Camera camera;
    camera.position = {12.f, 10.f, -22.f};
    camera.lookat = {0.f, 0.f, 0.f};
    camera.np = 0.1f;
    camera.fp = 500.f;
    camera.asp = 1.5f;
    for (auto projection : {dk::gfx::Camera::Projection::Perspective, dk::gfx::Camera::Projection::Orthographic}) {
        camera.projection = projection;
        for (auto direction : {glm::vec3(-0.5f, -1.f, -0.25f), glm::vec3(0.f, -1.f, 0.f)}) {
            float near = 0.1f;
            for (float far : csm::split_depths(near, 100.f, 4)) {
                const auto matrix = csm::lightspace_matrix(camera, direction, near, far);
                for (const auto& corner : csm::frustum_corners(camera, near, far)) {
                    const auto clip = matrix * glm::vec4(corner, 1.f);
                    for (int axis = 0; axis < 3; ++axis) {
                        EXPECT_TRUE(std::isfinite(clip[axis]));
                        EXPECT_LE(std::abs(clip[axis] / clip.w), 1.0001f);
                    }
                }
                near = far;
            }
        }
    }
}

TEST(Csm, DepthIncreasesAwayFromTheLight)
{
    dk::gfx::Camera camera;
    const glm::vec3 direction = glm::normalize(glm::vec3(-0.5f, -1.f, -0.25f));
    const auto matrix = csm::lightspace_matrix(camera, direction, 0.1f, 100.f);
    const auto caster = matrix * glm::vec4(0.f, 5.f, 0.f, 1.f);
    const auto receiver = matrix * glm::vec4(glm::vec3(0.f, 5.f, 0.f) + direction * 10.f, 1.f);
    EXPECT_LT(caster.z / caster.w, receiver.z / receiver.w);
    EXPECT_NEAR(caster.x, receiver.x, 1e-5f);
    EXPECT_NEAR(caster.y, receiver.y, 1e-5f);
    EXPECT_THROW(csm::lightspace_matrix(camera, glm::vec3(0.f), 0.1f, 100.f), std::runtime_error);
}
