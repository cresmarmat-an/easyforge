#include "Solver.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace easyforge::internal::physics2d
{
    Softness MakeSoft(float hertz, float damping, float step)
    {
        if (hertz <= 0.0f)
        {
            return {};
        }
        float omega = 2.0f * std::numbers::pi_v<float> * hertz;
        float first = 2.0f * damping + step * omega;
        float second = step * omega * first;
        float third = 1.0f / (1.0f + second);
        return { omega / first, second * third, third };
    }

    float RelativeAngle(Rotation first, Rotation second)
    {
        return Between(first, second).Angle();
    }

    namespace
    {
        struct PointConstraint
        {
            // From each body's center of mass to the point, at the start of the step.
            Vector2 ArmFirst;
            Vector2 ArmSecond;
            // The separation with the arms taken out, so the current separation
            // follows from how far the bodies have moved.
            float BaseSeparation = 0.0f;
            float NormalMass = 0.0f;
            float TangentMass = 0.0f;
            float NormalImpulse = 0.0f;
            float TangentImpulse = 0.0f;
            float MaximumNormalImpulse = 0.0f;
            float RelativeVelocity = 0.0f;
        };

        struct ContactConstraint
        {
            std::size_t Contact = 0;
            int First = -1;
            int Second = -1;
            Vector2 Normal;
            float Friction = 0.0f;
            float Restitution = 0.0f;
            PointConstraint Points[2];
            int Count = 0;
        };

        void Apply(BodyData& first, BodyData& second, Vector2 armFirst, Vector2 armSecond, Vector2 impulse)
        {
            first.Velocity -= impulse * first.InverseMass;
            first.AngularVelocity -= first.InverseInertia * Cross(armFirst, impulse);
            second.Velocity += impulse * second.InverseMass;
            second.AngularVelocity += second.InverseInertia * Cross(armSecond, impulse);
        }

        Vector2 RelativeVelocityAt(const BodyData& first, const BodyData& second, Vector2 armFirst, Vector2 armSecond)
        {
            return second.Velocity + TurnVelocity(second.AngularVelocity, armSecond) - first.Velocity -
                   TurnVelocity(first.AngularVelocity, armFirst);
        }

        void SolveContact(ContactConstraint& constraint, std::vector<BodyData>& bodies, const Softness& softness, float inverseStep, bool useBias)
        {
            BodyData& first = bodies[static_cast<std::size_t>(constraint.First)];
            BodyData& second = bodies[static_cast<std::size_t>(constraint.Second)];
            Rotation turnFirst = Between(first.StartTurn, first.Origin.Turn);
            Rotation turnSecond = Between(second.StartTurn, second.Origin.Turn);
            Vector2 normal = constraint.Normal;

            for (int index = 0; index < constraint.Count; ++index)
            {
                PointConstraint& point = constraint.Points[index];
                // The separation now, from how far the bodies have moved and turned.
                Vector2 moved = (second.DeltaPosition - first.DeltaPosition) +
                                (Rotate(turnSecond, point.ArmSecond) - Rotate(turnFirst, point.ArmFirst));
                float separation = Dot(moved, normal) + point.BaseSeparation;

                float bias = 0.0f;
                float massScale = 1.0f;
                float impulseScale = 0.0f;
                if (separation > 0.0f)
                {
                    // Not touching yet: allow closing exactly the gap this substep.
                    bias = separation * inverseStep;
                }
                else if (useBias)
                {
                    bias = std::max(softness.BiasRate * separation, -ContactPushSpeed);
                    massScale = softness.MassScale;
                    impulseScale = softness.ImpulseScale;
                }

                float approach = Dot(RelativeVelocityAt(first, second, point.ArmFirst, point.ArmSecond), normal);
                float impulse = -point.NormalMass * massScale * (approach + bias) - impulseScale * point.NormalImpulse;
                float total = std::max(point.NormalImpulse + impulse, 0.0f);
                impulse = total - point.NormalImpulse;
                point.NormalImpulse = total;
                point.MaximumNormalImpulse = std::max(point.MaximumNormalImpulse, impulse);
                Apply(first, second, point.ArmFirst, point.ArmSecond, normal * impulse);
            }

            Vector2 tangent = RightPerpendicular(normal);
            for (int index = 0; index < constraint.Count; ++index)
            {
                PointConstraint& point = constraint.Points[index];
                float sliding = Dot(RelativeVelocityAt(first, second, point.ArmFirst, point.ArmSecond), tangent);
                float impulse = -point.TangentMass * sliding;
                float limit = constraint.Friction * point.NormalImpulse;
                float total = std::clamp(point.TangentImpulse + impulse, -limit, limit);
                impulse = total - point.TangentImpulse;
                point.TangentImpulse = total;
                Apply(first, second, point.ArmFirst, point.ArmSecond, tangent * impulse);
            }
        }
    }
}

namespace easyforge::internal
{
    using namespace physics2d;

    void WorldState2D::Solve(float seconds)
    {
        int substeps = std::max(Substeps, 1);
        StepContext context;
        context.Step = seconds / static_cast<float>(substeps);
        context.InverseStep = 1.0f / context.Step;
        float contactHertz = std::min(ContactHertz, 0.25f * context.InverseStep);
        Softness contactSoftness = MakeSoft(contactHertz, ContactDampingRatio, context.Step);
        context.Joints = MakeSoft(2.0f * contactHertz, JointDampingRatio, context.Step);

        for (BodyData& body : Bodies)
        {
            body.DeltaPosition = {};
            body.StartTurn = body.Origin.Turn;
        }

        // Contact constraints, from this step's manifolds, starting from last
        // step's impulses.
        std::vector<ContactConstraint> constraints;
        constraints.reserve(Contacts.size());
        for (std::size_t index = 0; index < Contacts.size(); ++index)
        {
            const ContactData& contact = Contacts[index];
            if (contact.Sensor || contact.Points.Count == 0)
            {
                continue;
            }
            const BodyData& first = Bodies[static_cast<std::size_t>(contact.First)];
            const BodyData& second = Bodies[static_cast<std::size_t>(contact.Second)];
            if (first.InverseMass == 0.0f && second.InverseMass == 0.0f && first.InverseInertia == 0.0f && second.InverseInertia == 0.0f)
            {
                continue;
            }
            ContactConstraint constraint;
            constraint.Contact = index;
            constraint.First = contact.First;
            constraint.Second = contact.Second;
            constraint.Normal = contact.Points.Normal;
            // Mixed from the bodies every step, so changing either takes effect.
            constraint.Friction = std::sqrt(first.Friction * second.Friction);
            constraint.Restitution = std::max(first.Restitution, second.Restitution);
            constraint.Count = contact.Points.Count;
            Vector2 tangent = RightPerpendicular(constraint.Normal);
            for (int point = 0; point < constraint.Count; ++point)
            {
                const ManifoldPoint& source = contact.Points.Points[point];
                PointConstraint& target = constraint.Points[point];
                target.ArmFirst = source.Point - first.Center;
                target.ArmSecond = source.Point - second.Center;
                target.BaseSeparation = source.Separation - Dot(target.ArmSecond - target.ArmFirst, constraint.Normal);
                float normalFirst = Cross(target.ArmFirst, constraint.Normal);
                float normalSecond = Cross(target.ArmSecond, constraint.Normal);
                float normalMass = first.InverseMass + second.InverseMass + first.InverseInertia * normalFirst * normalFirst +
                                   second.InverseInertia * normalSecond * normalSecond;
                target.NormalMass = normalMass > 0.0f ? 1.0f / normalMass : 0.0f;
                float tangentFirst = Cross(target.ArmFirst, tangent);
                float tangentSecond = Cross(target.ArmSecond, tangent);
                float tangentMass = first.InverseMass + second.InverseMass + first.InverseInertia * tangentFirst * tangentFirst +
                                    second.InverseInertia * tangentSecond * tangentSecond;
                target.TangentMass = tangentMass > 0.0f ? 1.0f / tangentMass : 0.0f;
                target.NormalImpulse = source.NormalImpulse;
                target.TangentImpulse = source.TangentImpulse;
                target.RelativeVelocity = Dot(RelativeVelocityAt(first, second, target.ArmFirst, target.ArmSecond), constraint.Normal);
            }
            constraints.push_back(constraint);
        }

        std::vector<JointData*> joints;
        for (JointData& joint : Joints)
        {
            if (joint.Alive)
            {
                joints.push_back(&joint);
            }
        }

        for (int substep = 0; substep < substeps; ++substep)
        {
            // Velocities from gravity, forces, and damping.
            for (BodyData& body : Bodies)
            {
                if (!body.Alive || body.Type != BodyType::Dynamic)
                {
                    continue;
                }
                body.Velocity += (Gravity * body.GravityScale + body.Force * body.InverseMass) * context.Step;
                body.AngularVelocity += body.InverseInertia * body.Torque * context.Step;
                body.Velocity *= 1.0f / (1.0f + context.Step * body.LinearDamping);
                body.AngularVelocity *= 1.0f / (1.0f + context.Step * body.AngularDamping);
                float speed = Length(body.Velocity);
                if (speed > MaximumLinearSpeed)
                {
                    body.Velocity *= MaximumLinearSpeed / speed;
                }
            }

            // Last substep's impulses first, which is most of the answer when
            // things rest.
            for (JointData* joint : joints)
            {
                WarmStartJoint(*joint, Bodies);
            }
            for (ContactConstraint& constraint : constraints)
            {
                BodyData& first = Bodies[static_cast<std::size_t>(constraint.First)];
                BodyData& second = Bodies[static_cast<std::size_t>(constraint.Second)];
                Vector2 tangent = RightPerpendicular(constraint.Normal);
                for (int index = 0; index < constraint.Count; ++index)
                {
                    const PointConstraint& point = constraint.Points[index];
                    Vector2 impulse = constraint.Normal * point.NormalImpulse + tangent * point.TangentImpulse;
                    Apply(first, second, point.ArmFirst, point.ArmSecond, impulse);
                }
            }

            for (JointData* joint : joints)
            {
                SolveJoint(*joint, Bodies, context, true);
            }
            for (ContactConstraint& constraint : constraints)
            {
                SolveContact(constraint, Bodies, contactSoftness, context.InverseStep, true);
            }

            for (BodyData& body : Bodies)
            {
                if (!body.Alive || body.Type == BodyType::Static)
                {
                    continue;
                }
                Vector2 move = body.Velocity * context.Step;
                body.Center += move;
                body.DeltaPosition += move;
                body.Origin.Turn = Integrate(body.Origin.Turn, body.AngularVelocity * context.Step);
            }

            for (JointData* joint : joints)
            {
                SolveJoint(*joint, Bodies, context, false);
            }
            for (ContactConstraint& constraint : constraints)
            {
                SolveContact(constraint, Bodies, contactSoftness, context.InverseStep, false);
            }
        }

        // Bounces, from the speed each contact had coming in.
        for (ContactConstraint& constraint : constraints)
        {
            if (constraint.Restitution == 0.0f)
            {
                continue;
            }
            BodyData& first = Bodies[static_cast<std::size_t>(constraint.First)];
            BodyData& second = Bodies[static_cast<std::size_t>(constraint.Second)];
            for (int index = 0; index < constraint.Count; ++index)
            {
                PointConstraint& point = constraint.Points[index];
                if (point.RelativeVelocity > -RestitutionThreshold || point.MaximumNormalImpulse == 0.0f)
                {
                    continue;
                }
                float approach = Dot(RelativeVelocityAt(first, second, point.ArmFirst, point.ArmSecond), constraint.Normal);
                float impulse = -point.NormalMass * (approach + constraint.Restitution * point.RelativeVelocity);
                float total = std::max(point.NormalImpulse + impulse, 0.0f);
                impulse = total - point.NormalImpulse;
                point.NormalImpulse = total;
                point.MaximumNormalImpulse = std::max(point.MaximumNormalImpulse, impulse);
                Apply(first, second, point.ArmFirst, point.ArmSecond, constraint.Normal * impulse);
            }
        }

        // Keep the impulses for next step.
        for (const ContactConstraint& constraint : constraints)
        {
            Manifold& manifold = Contacts[constraint.Contact].Points;
            for (int index = 0; index < constraint.Count; ++index)
            {
                manifold.Points[index].NormalImpulse = constraint.Points[index].NormalImpulse;
                manifold.Points[index].TangentImpulse = constraint.Points[index].TangentImpulse;
                manifold.Points[index].MaximumNormalImpulse = constraint.Points[index].MaximumNormalImpulse;
            }
        }
    }
}
