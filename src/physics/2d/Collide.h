#pragma once

#include <cstdint>

#include "Geometry.h"

namespace easyforge::internal::physics2d
{
    struct ManifoldPoint
    {
        // In the world.
        Vector2 Point;

        // Negative when overlapping.
        float Separation = 0.0f;

        // Names the features that made the point, so a point found again next
        // step keeps its impulses.
        std::uint32_t Id = 0;

        // Solver state carried between steps.
        float NormalImpulse = 0.0f;
        float TangentImpulse = 0.0f;
        float MaximumNormalImpulse = 0.0f;
    };

    struct Manifold
    {
        // From the first shape toward the second, in the world.
        Vector2 Normal;
        ManifoldPoint Points[2];
        int Count = 0;
    };

    // Where two shapes touch, or will within the speculative distance.
    Manifold Collide(const Shape& first, const Pose& firstPose, const Shape& second, const Pose& secondPose);
}
