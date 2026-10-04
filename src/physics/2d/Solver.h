#pragma once

#include <vector>

#include "World.h"

// The solver keeps constraints soft: each acts like a very stiff, heavily
// damped spring, which settles stacks without jitter and needs no separate
// pass to push overlapping bodies apart. Each step is cut into substeps; in
// each, velocities are solved with the push toward correct positions, the
// bodies move, and the velocities are solved again without it ("relaxed"), so
// the correction leaves no speed behind.

namespace easyforge::internal::physics2d
{
    // Contacts, as springs: how fast they are, how damped, and how fast at most
    // they push overlapping bodies apart.
    inline constexpr float ContactHertz = 30.0f;
    inline constexpr float ContactDampingRatio = 10.0f;
    inline constexpr float ContactPushSpeed = 3.0f;

    inline constexpr float JointHertz = 60.0f;
    inline constexpr float JointDampingRatio = 2.0f;

    // Collisions slower than this, in metres a second, do not bounce.
    inline constexpr float RestitutionThreshold = 1.0f;

    inline constexpr float MaximumLinearSpeed = 400.0f;

    struct Softness
    {
        float BiasRate = 0.0f;
        float MassScale = 1.0f;
        float ImpulseScale = 0.0f;
    };

    // A constraint as a spring of the given hertz and damping ratio, for
    // substeps of `step` seconds. Zero hertz is rigid.
    Softness MakeSoft(float hertz, float damping, float step);

    struct StepContext
    {
        float Step = 0.0f;
        float InverseStep = 0.0f;
        Softness Joints;
    };

    void WarmStartJoint(JointData& joint, std::vector<BodyData>& bodies);
    void SolveJoint(JointData& joint, std::vector<BodyData>& bodies, const StepContext& context, bool useBias);

    // The angle from the first turn to the second, from -pi to pi.
    float RelativeAngle(Rotation first, Rotation second);
}
