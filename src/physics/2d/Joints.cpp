#include <algorithm>
#include <cmath>
#include <numbers>

#include "Solver.h"

// Each joint works from the bodies as they are now: the arms from their centers
// of mass to the anchors are turned with the bodies every substep.

namespace easyforge::internal::physics2d
{
    namespace
    {
        float Wrap(float angle)
        {
            constexpr float pi = std::numbers::pi_v<float>;
            while (angle > pi)
            {
                angle -= 2.0f * pi;
            }
            while (angle < -pi)
            {
                angle += 2.0f * pi;
            }
            return angle;
        }

        struct Frame
        {
            BodyData& First;
            BodyData& Second;
            Vector2 ArmFirst;
            Vector2 ArmSecond;
        };

        Frame FrameOf(const JointData& joint, std::vector<BodyData>& bodies)
        {
            BodyData& first = bodies[static_cast<std::size_t>(joint.First)];
            BodyData& second = bodies[static_cast<std::size_t>(joint.Second)];
            if (joint.Kind == JointKind::Motor)
            {
                // The target is the second body's origin at an offset from the first's.
                return { first, second, Rotate(first.Origin.Turn, joint.LinearOffset - first.LocalCenter),
                    Rotate(second.Origin.Turn, -second.LocalCenter) };
            }
            return { first, second, Rotate(first.Origin.Turn, joint.LocalArmFirst), Rotate(second.Origin.Turn, joint.LocalArmSecond) };
        }

        Vector2 VelocityError(const Frame& frame)
        {
            return frame.Second.Velocity + TurnVelocity(frame.Second.AngularVelocity, frame.ArmSecond) - frame.First.Velocity -
                   TurnVelocity(frame.First.AngularVelocity, frame.ArmFirst);
        }

        Vector2 PositionError(const Frame& frame)
        {
            return (frame.Second.Center + frame.ArmSecond) - (frame.First.Center + frame.ArmFirst);
        }

        // Pushes the two bodies apart at the arms by `impulse`, with extra turning.
        void Push(Frame& frame, Vector2 impulse, float turnFirst = 0.0f, float turnSecond = 0.0f)
        {
            frame.First.Velocity -= impulse * frame.First.InverseMass;
            frame.First.AngularVelocity -= frame.First.InverseInertia * (Cross(frame.ArmFirst, impulse) + turnFirst);
            frame.Second.Velocity += impulse * frame.Second.InverseMass;
            frame.Second.AngularVelocity += frame.Second.InverseInertia * (Cross(frame.ArmSecond, impulse) + turnSecond);
        }

        void Turn(Frame& frame, float impulse)
        {
            frame.First.AngularVelocity -= frame.First.InverseInertia * impulse;
            frame.Second.AngularVelocity += frame.Second.InverseInertia * impulse;
        }

        // Solves K x = b for the 2x2 mass of a point constraint.
        Vector2 SolvePointMass(const Frame& frame, Vector2 right)
        {
            float masses = frame.First.InverseMass + frame.Second.InverseMass;
            float first = frame.First.InverseInertia;
            float second = frame.Second.InverseInertia;
            Vector2 armFirst = frame.ArmFirst;
            Vector2 armSecond = frame.ArmSecond;
            float k11 = masses + first * armFirst.Y * armFirst.Y + second * armSecond.Y * armSecond.Y;
            float k12 = -first * armFirst.X * armFirst.Y - second * armSecond.X * armSecond.Y;
            float k22 = masses + first * armFirst.X * armFirst.X + second * armSecond.X * armSecond.X;
            float determinant = k11 * k22 - k12 * k12;
            if (determinant == 0.0f)
            {
                return {};
            }
            determinant = 1.0f / determinant;
            return { determinant * (k22 * right.X - k12 * right.Y), determinant * (k11 * right.Y - k12 * right.X) };
        }

        struct Pull
        {
            float Bias = 0.0f;
            float MassScale = 1.0f;
            float ImpulseScale = 0.0f;
        };

        // A rigid constraint's pull: soft toward the target only while biased.
        Pull Rigid(const StepContext& context, float error, bool useBias)
        {
            if (!useBias)
            {
                return {};
            }
            return { context.Joints.BiasRate * error, context.Joints.MassScale, context.Joints.ImpulseScale };
        }

        // A one-sided limit: speculative while there is still room, soft past it.
        Pull Limit(const StepContext& context, float room, bool useBias)
        {
            if (room > 0.0f)
            {
                return { room * context.InverseStep, 1.0f, 0.0f };
            }
            return Rigid(context, room, useBias);
        }

        float AngularMass(const Frame& frame)
        {
            float sum = frame.First.InverseInertia + frame.Second.InverseInertia;
            return sum > 0.0f ? 1.0f / sum : 0.0f;
        }

        // A limit on the angle or along an axis: `velocity` is how fast the room
        // left grows, and the impulse, which only ever pushes, is applied by `apply`.
        template <typename ApplyImpulse>
        void SolveLimit(float& accumulated, float mass, float velocity, const Pull& pull, ApplyImpulse&& apply)
        {
            float impulse = -mass * pull.MassScale * (velocity + pull.Bias) - pull.ImpulseScale * accumulated;
            float total = std::max(accumulated + impulse, 0.0f);
            impulse = total - accumulated;
            accumulated = total;
            apply(impulse);
        }

        // ---- Distance ------------------------------------------------------------

        void SolveDistance(JointData& joint, Frame& frame, const StepContext& context, bool useBias)
        {
            Vector2 between = PositionError(frame);
            float length = Length(between);
            Vector2 axis = length > 1e-9f ? between / length : Vector2 { 1.0f, 0.0f };
            float crossFirst = Cross(frame.ArmFirst, axis);
            float crossSecond = Cross(frame.ArmSecond, axis);
            float k = frame.First.InverseMass + frame.Second.InverseMass + frame.First.InverseInertia * crossFirst * crossFirst +
                      frame.Second.InverseInertia * crossSecond * crossSecond;
            float mass = k > 0.0f ? 1.0f / k : 0.0f;
            auto along = [&](float impulse) { Push(frame, axis * impulse); };

            if (joint.Stiffness > 0.0f)
            {
                // A spring works in every pass; it is meant to give.
                Softness spring = MakeSoft(joint.Stiffness, joint.Damping, context.Step);
                float velocity = Dot(axis, VelocityError(frame));
                float impulse = -spring.MassScale * mass * (velocity + spring.BiasRate * (length - joint.Length)) -
                                spring.ImpulseScale * joint.AxialImpulse;
                joint.AxialImpulse += impulse;
                along(impulse);

                if (joint.MinimumLength < joint.MaximumLength)
                {
                    SolveLimit(joint.LowerImpulse, mass, Dot(axis, VelocityError(frame)),
                        Limit(context, length - joint.MinimumLength, useBias), along);
                    if (std::isfinite(joint.MaximumLength))
                    {
                        SolveLimit(joint.UpperImpulse, mass, -Dot(axis, VelocityError(frame)),
                            Limit(context, joint.MaximumLength - length, useBias), [&](float impulse) { along(-impulse); });
                    }
                }
                return;
            }

            Pull pull = Rigid(context, length - joint.Length, useBias);
            float velocity = Dot(axis, VelocityError(frame));
            float impulse = -mass * pull.MassScale * (velocity + pull.Bias) - pull.ImpulseScale * joint.AxialImpulse;
            joint.AxialImpulse += impulse;
            along(impulse);
        }

        // ---- Hinge ---------------------------------------------------------------

        void SolveHinge(JointData& joint, Frame& frame, const StepContext& context, bool useBias)
        {
            float angularMass = AngularMass(frame);
            if (joint.Motor && angularMass > 0.0f)
            {
                float spinning = frame.Second.AngularVelocity - frame.First.AngularVelocity;
                float impulse = -angularMass * (spinning - joint.MotorSpeed);
                float limit = joint.MaximumMotor * context.Step;
                float total = std::clamp(joint.MotorImpulse + impulse, -limit, limit);
                Turn(frame, total - joint.MotorImpulse);
                joint.MotorImpulse = total;
            }
            if (joint.Limit && angularMass > 0.0f)
            {
                float angle = Wrap(RelativeAngle(frame.First.Origin.Turn, frame.Second.Origin.Turn) - joint.ReferenceAngle);
                SolveLimit(joint.LowerImpulse, angularMass, frame.Second.AngularVelocity - frame.First.AngularVelocity,
                    Limit(context, angle - joint.Lower, useBias), [&](float impulse) { Turn(frame, impulse); });
                SolveLimit(joint.UpperImpulse, angularMass, frame.First.AngularVelocity - frame.Second.AngularVelocity,
                    Limit(context, joint.Upper - angle, useBias), [&](float impulse) { Turn(frame, -impulse); });
            }

            Vector2 bias;
            float massScale = 1.0f;
            float impulseScale = 0.0f;
            if (useBias)
            {
                bias = PositionError(frame) * context.Joints.BiasRate;
                massScale = context.Joints.MassScale;
                impulseScale = context.Joints.ImpulseScale;
            }
            Vector2 impulse = SolvePointMass(frame, VelocityError(frame) + bias) * -massScale - joint.LinearImpulse * impulseScale;
            joint.LinearImpulse += impulse;
            Push(frame, impulse);
        }

        // ---- Slider --------------------------------------------------------------

        struct SliderGeometry
        {
            Vector2 Axis;
            Vector2 Across;
            Vector2 Between;
            float AxisFirst = 0.0f;
            float AxisSecond = 0.0f;
            float AcrossFirst = 0.0f;
            float AcrossSecond = 0.0f;
        };

        SliderGeometry SliderOf(const JointData& joint, const Frame& frame)
        {
            SliderGeometry geometry;
            geometry.Axis = Rotate(frame.First.Origin.Turn, joint.LocalAxis);
            geometry.Across = LeftPerpendicular(geometry.Axis);
            geometry.Between = PositionError(frame);
            geometry.AxisFirst = Cross(geometry.Between + frame.ArmFirst, geometry.Axis);
            geometry.AxisSecond = Cross(frame.ArmSecond, geometry.Axis);
            geometry.AcrossFirst = Cross(geometry.Between + frame.ArmFirst, geometry.Across);
            geometry.AcrossSecond = Cross(frame.ArmSecond, geometry.Across);
            return geometry;
        }

        void PushSlider(Frame& frame, const SliderGeometry& geometry, float across, float turning, float axial)
        {
            Vector2 impulse = geometry.Across * across + geometry.Axis * axial;
            float turnFirst = across * geometry.AcrossFirst + turning + axial * geometry.AxisFirst;
            float turnSecond = across * geometry.AcrossSecond + turning + axial * geometry.AxisSecond;
            frame.First.Velocity -= impulse * frame.First.InverseMass;
            frame.First.AngularVelocity -= frame.First.InverseInertia * turnFirst;
            frame.Second.Velocity += impulse * frame.Second.InverseMass;
            frame.Second.AngularVelocity += frame.Second.InverseInertia * turnSecond;
        }

        void SolveSlider(JointData& joint, Frame& frame, const StepContext& context, bool useBias)
        {
            SliderGeometry geometry = SliderOf(joint, frame);
            float masses = frame.First.InverseMass + frame.Second.InverseMass;
            float inertiaFirst = frame.First.InverseInertia;
            float inertiaSecond = frame.Second.InverseInertia;
            float axialK = masses + inertiaFirst * geometry.AxisFirst * geometry.AxisFirst + inertiaSecond * geometry.AxisSecond * geometry.AxisSecond;
            float axialMass = axialK > 0.0f ? 1.0f / axialK : 0.0f;
            auto axialSpeed = [&] {
                return Dot(geometry.Axis, frame.Second.Velocity - frame.First.Velocity) + geometry.AxisSecond * frame.Second.AngularVelocity -
                       geometry.AxisFirst * frame.First.AngularVelocity;
            };

            if (joint.Motor)
            {
                float impulse = axialMass * (joint.MotorSpeed - axialSpeed());
                float limit = joint.MaximumMotor * context.Step;
                float total = std::clamp(joint.MotorImpulse + impulse, -limit, limit);
                PushSlider(frame, geometry, 0.0f, 0.0f, total - joint.MotorImpulse);
                joint.MotorImpulse = total;
            }
            if (joint.Limit)
            {
                float translation = Dot(geometry.Axis, geometry.Between);
                SolveLimit(joint.LowerImpulse, axialMass, axialSpeed(), Limit(context, translation - joint.Lower, useBias),
                    [&](float impulse) { PushSlider(frame, geometry, 0.0f, 0.0f, impulse); });
                SolveLimit(joint.UpperImpulse, axialMass, -axialSpeed(), Limit(context, joint.Upper - translation, useBias),
                    [&](float impulse) { PushSlider(frame, geometry, 0.0f, 0.0f, -impulse); });
            }

            // Across the axis and in angle, the bodies stay put.
            Vector2 velocity { Dot(geometry.Across, frame.Second.Velocity - frame.First.Velocity) +
                                   geometry.AcrossSecond * frame.Second.AngularVelocity - geometry.AcrossFirst * frame.First.AngularVelocity,
                frame.Second.AngularVelocity - frame.First.AngularVelocity };
            float k11 = masses + inertiaFirst * geometry.AcrossFirst * geometry.AcrossFirst + inertiaSecond * geometry.AcrossSecond * geometry.AcrossSecond;
            float k12 = inertiaFirst * geometry.AcrossFirst + inertiaSecond * geometry.AcrossSecond;
            float k22 = inertiaFirst + inertiaSecond;
            if (k22 == 0.0f)
            {
                k22 = 1.0f;
            }
            Vector2 bias;
            float massScale = 1.0f;
            float impulseScale = 0.0f;
            if (useBias)
            {
                Vector2 error { Dot(geometry.Across, geometry.Between),
                    Wrap(RelativeAngle(frame.First.Origin.Turn, frame.Second.Origin.Turn) - joint.ReferenceAngle) };
                bias = error * context.Joints.BiasRate;
                massScale = context.Joints.MassScale;
                impulseScale = context.Joints.ImpulseScale;
            }
            Vector2 right = velocity + bias;
            float determinant = k11 * k22 - k12 * k12;
            Vector2 solved;
            if (determinant != 0.0f)
            {
                determinant = 1.0f / determinant;
                solved = { determinant * (k22 * right.X - k12 * right.Y), determinant * (k11 * right.Y - k12 * right.X) };
            }
            Vector2 impulse = solved * -massScale - joint.LinearImpulse * impulseScale;
            joint.LinearImpulse += impulse;
            PushSlider(frame, geometry, impulse.X, impulse.Y, 0.0f);
        }

        // ---- Weld ----------------------------------------------------------------

        void SolveWeld(JointData& joint, Frame& frame, const StepContext& context, bool useBias)
        {
            auto pull = [&](float error) -> Pull {
                if (joint.Stiffness > 0.0f)
                {
                    Softness spring = MakeSoft(joint.Stiffness, joint.Damping, context.Step);
                    return { spring.BiasRate * error, spring.MassScale, spring.ImpulseScale };
                }
                return Rigid(context, error, useBias);
            };

            float angularMass = AngularMass(frame);
            Pull angular = pull(Wrap(RelativeAngle(frame.First.Origin.Turn, frame.Second.Origin.Turn) - joint.ReferenceAngle));
            float spinning = frame.Second.AngularVelocity - frame.First.AngularVelocity;
            float turnImpulse = -angularMass * angular.MassScale * (spinning + angular.Bias) - angular.ImpulseScale * joint.AngularImpulse;
            joint.AngularImpulse += turnImpulse;
            Turn(frame, turnImpulse);

            Vector2 error = PositionError(frame);
            Pull linear = pull(1.0f);
            Vector2 impulse = SolvePointMass(frame, VelocityError(frame) + error * linear.Bias) * -linear.MassScale -
                              joint.LinearImpulse * linear.ImpulseScale;
            joint.LinearImpulse += impulse;
            Push(frame, impulse);
        }

        // ---- Motor ---------------------------------------------------------------

        // A drive rather than a constraint: it pulls toward its target in every
        // pass, since relaxing it without the pull would brake the body each substep.
        void SolveMotor(JointData& joint, Frame& frame, const StepContext& context, bool)
        {
            float correction = joint.Correction * context.InverseStep;

            float angularMass = AngularMass(frame);
            float angle = Wrap(RelativeAngle(frame.First.Origin.Turn, frame.Second.Origin.Turn) - joint.AngularOffset);
            float spinning = frame.Second.AngularVelocity - frame.First.AngularVelocity;
            float turnImpulse = -angularMass * (spinning + correction * angle);
            float torqueLimit = joint.MaximumTorque * context.Step;
            float turnTotal = std::clamp(joint.AngularImpulse + turnImpulse, -torqueLimit, torqueLimit);
            Turn(frame, turnTotal - joint.AngularImpulse);
            joint.AngularImpulse = turnTotal;

            Vector2 impulse = -SolvePointMass(frame, VelocityError(frame) + PositionError(frame) * correction);
            Vector2 previous = joint.LinearImpulse;
            Vector2 total = previous + impulse;
            float forceLimit = joint.MaximumForce * context.Step;
            float size = Length(total);
            if (size > forceLimit && size > 0.0f)
            {
                total *= forceLimit / size;
            }
            joint.LinearImpulse = total;
            Push(frame, total - previous);
        }
    }

    void WarmStartJoint(JointData& joint, std::vector<BodyData>& bodies)
    {
        Frame frame = FrameOf(joint, bodies);
        switch (joint.Kind)
        {
        case JointKind::Distance:
        {
            Vector2 between = PositionError(frame);
            float length = Length(between);
            Vector2 axis = length > 1e-9f ? between / length : Vector2 { 1.0f, 0.0f };
            Push(frame, axis * (joint.AxialImpulse + joint.LowerImpulse - joint.UpperImpulse));
            break;
        }
        case JointKind::Hinge:
        {
            float turning = joint.MotorImpulse + joint.LowerImpulse - joint.UpperImpulse;
            Push(frame, joint.LinearImpulse, turning, turning);
            break;
        }
        case JointKind::Slider:
        {
            SliderGeometry geometry = SliderOf(joint, frame);
            PushSlider(frame, geometry, joint.LinearImpulse.X, joint.LinearImpulse.Y, joint.MotorImpulse + joint.LowerImpulse - joint.UpperImpulse);
            break;
        }
        case JointKind::Weld:
        case JointKind::Motor:
            Push(frame, joint.LinearImpulse, joint.AngularImpulse, joint.AngularImpulse);
            break;
        }
    }

    void SolveJoint(JointData& joint, std::vector<BodyData>& bodies, const StepContext& context, bool useBias)
    {
        Frame frame = FrameOf(joint, bodies);
        switch (joint.Kind)
        {
        case JointKind::Distance: SolveDistance(joint, frame, context, useBias); break;
        case JointKind::Hinge: SolveHinge(joint, frame, context, useBias); break;
        case JointKind::Slider: SolveSlider(joint, frame, context, useBias); break;
        case JointKind::Weld: SolveWeld(joint, frame, context, useBias); break;
        case JointKind::Motor: SolveMotor(joint, frame, context, useBias); break;
        }
    }
}
