#include <easyforge/physics/Physics2D.h>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "Solver.h"
#include "World.h"

namespace easyforge
{
    using internal::BodyReference2D;
    using internal::JointReference2D;
    using internal::WorldState2D;
    using namespace internal::physics2d;

    namespace
    {
        // A body handle's world and body, while both are there.
        struct Resolved
        {
            std::shared_ptr<WorldState2D> World;
            BodyData* Body = nullptr;
            int Index = -1;

            explicit operator bool() const { return Body != nullptr; }
        };

        Resolved Resolve(const BodyReference2D& reference)
        {
            Resolved resolved;
            resolved.World = reference.World.lock();
            if (resolved.World)
            {
                resolved.Body = resolved.World->Find(reference.Index, reference.Generation);
                resolved.Index = reference.Index;
            }
            return resolved;
        }

        Resolved Resolve(const void* owner)
        {
            return Resolve(*static_cast<const BodyReference2D*>(owner));
        }

        struct ResolvedJoint
        {
            std::shared_ptr<WorldState2D> World;
            JointData* Joint = nullptr;
            int Index = -1;
        };

        ResolvedJoint ResolveJoint(const void* owner)
        {
            const auto& reference = *static_cast<const JointReference2D*>(owner);
            ResolvedJoint resolved;
            resolved.World = reference.World.lock();
            if (resolved.World)
            {
                resolved.Joint = resolved.World->FindJoint(reference.Index, reference.Generation);
                resolved.Index = reference.Index;
            }
            return resolved;
        }

        // A property of the body, read as `fallback` when the body is gone.
        template <typename Value, auto Member>
        Value ReadField(const void* owner)
        {
            Resolved resolved = Resolve(owner);
            return resolved ? static_cast<Value>(resolved.Body->*Member) : Value {};
        }

        template <typename Value, auto Member>
        void WriteField(void* owner, const Value& value)
        {
            if (Resolved resolved = Resolve(owner))
            {
                resolved.Body->*Member = value;
            }
        }

        float Wrapped(float angle)
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
    }

    // ---- Body2D ------------------------------------------------------------------

    Body2D::Body2D() : Body2D(std::make_shared<BodyReference2D>())
    {
    }

    Body2D::Body2D(std::shared_ptr<BodyReference2D> reference)
        : Position(reference.get(),
              [](const void* owner) {
                  Resolved resolved = Resolve(owner);
                  return resolved ? resolved.Body->Origin.Position : Vector2 {};
              },
              [](void* owner, const Vector2& value) {
                  if (Resolved resolved = Resolve(owner))
                  {
                      resolved.Body->Origin.Position = value;
                      resolved.World->Placed(resolved.Index);
                  }
              }),
          Rotation(reference.get(),
              [](const void* owner) {
                  Resolved resolved = Resolve(owner);
                  return resolved ? resolved.Body->Origin.Turn.Angle() : 0.0f;
              },
              [](void* owner, const float& value) {
                  if (Resolved resolved = Resolve(owner))
                  {
                      resolved.Body->Origin.Turn = internal::physics2d::Rotation::FromAngle(value);
                      resolved.World->Placed(resolved.Index);
                  }
              }),
          Velocity(reference.get(), ReadField<Vector2, &BodyData::Velocity>,
              [](void* owner, const Vector2& value) {
                  Resolved resolved = Resolve(owner);
                  if (resolved && resolved.Body->Type != BodyType::Static)
                  {
                      resolved.Body->Velocity = value;
                  }
              }),
          AngularVelocity(reference.get(), ReadField<float, &BodyData::AngularVelocity>,
              [](void* owner, const float& value) {
                  Resolved resolved = Resolve(owner);
                  if (resolved && resolved.Body->Type != BodyType::Static && !resolved.Body->FixedRotation)
                  {
                      resolved.Body->AngularVelocity = value;
                  }
              }),
          Type(reference.get(), ReadField<BodyType, &BodyData::Type>,
              [](void* owner, const BodyType& value) {
                  Resolved resolved = Resolve(owner);
                  if (resolved && resolved.Body->Type != value)
                  {
                      resolved.Body->Type = value;
                      resolved.World->Retyped(resolved.Index);
                  }
              }),
          Friction(reference.get(), ReadField<float, &BodyData::Friction>,
              [](void* owner, const float& value) { WriteField<float, &BodyData::Friction>(owner, std::max(value, 0.0f)); }),
          Restitution(reference.get(), ReadField<float, &BodyData::Restitution>,
              [](void* owner, const float& value) { WriteField<float, &BodyData::Restitution>(owner, std::max(value, 0.0f)); }),
          LinearDamping(reference.get(), ReadField<float, &BodyData::LinearDamping>,
              [](void* owner, const float& value) { WriteField<float, &BodyData::LinearDamping>(owner, std::max(value, 0.0f)); }),
          AngularDamping(reference.get(), ReadField<float, &BodyData::AngularDamping>,
              [](void* owner, const float& value) { WriteField<float, &BodyData::AngularDamping>(owner, std::max(value, 0.0f)); }),
          GravityScale(reference.get(), ReadField<float, &BodyData::GravityScale>, WriteField<float, &BodyData::GravityScale>),
          FixedRotation(reference.get(), ReadField<bool, &BodyData::FixedRotation>,
              [](void* owner, const bool& value) {
                  if (Resolved resolved = Resolve(owner))
                  {
                      resolved.Body->FixedRotation = value;
                      if (value)
                      {
                          resolved.Body->AngularVelocity = 0.0f;
                      }
                      resolved.World->Retyped(resolved.Index);
                  }
              }),
          Layer(reference.get(), ReadField<std::uint32_t, &BodyData::Layer>,
              [](void* owner, const std::uint32_t& value) {
                  if (Resolved resolved = Resolve(owner))
                  {
                      resolved.Body->Layer = value;
                      resolved.World->Retyped(resolved.Index);
                  }
              }),
          CollidesWith(reference.get(), ReadField<std::uint32_t, &BodyData::CollidesWith>,
              [](void* owner, const std::uint32_t& value) {
                  if (Resolved resolved = Resolve(owner))
                  {
                      resolved.Body->CollidesWith = value;
                      resolved.World->Retyped(resolved.Index);
                  }
              }),
          OnTouch(reference.get(), ReadField<std::function<void(const Contact2D&)>, &BodyData::OnTouch>,
              WriteField<std::function<void(const Contact2D&)>, &BodyData::OnTouch>),
          OnSeparate(reference.get(), ReadField<std::function<void(const Contact2D&)>, &BodyData::OnSeparate>,
              WriteField<std::function<void(const Contact2D&)>, &BodyData::OnSeparate>),
          Reference(std::move(reference))
    {
    }

    Body2D::Body2D(const Body2D& other) : Body2D(other.Reference)
    {
    }

    Body2D& Body2D::operator=(const Body2D& other)
    {
        if (this != &other)
        {
            Reference = other.Reference;
            RebindProperties();
        }
        return *this;
    }

    Body2D::~Body2D() = default;

    void Body2D::RebindProperties()
    {
        void* owner = Reference.get();
        Position.Rebind(owner);
        Rotation.Rebind(owner);
        Velocity.Rebind(owner);
        AngularVelocity.Rebind(owner);
        Type.Rebind(owner);
        Friction.Rebind(owner);
        Restitution.Rebind(owner);
        LinearDamping.Rebind(owner);
        AngularDamping.Rebind(owner);
        GravityScale.Rebind(owner);
        FixedRotation.Rebind(owner);
        Layer.Rebind(owner);
        CollidesWith.Rebind(owner);
        OnTouch.Rebind(owner);
        OnSeparate.Rebind(owner);
    }

    Body2D::operator bool() const
    {
        return static_cast<bool>(Resolve(*Reference));
    }

    bool Body2D::operator==(const Body2D& other) const
    {
        return Reference->World.lock() == other.Reference->World.lock() && Reference->Index == other.Reference->Index &&
               Reference->Generation == other.Reference->Generation;
    }

    std::uint64_t Body2D::Id() const
    {
        return Reference->Index < 0 ? 0 : (static_cast<std::uint64_t>(Reference->Generation) << 32) | static_cast<std::uint32_t>(Reference->Index);
    }

    ShapeKind2D Body2D::Shape() const
    {
        Resolved resolved = Resolve(*Reference);
        return resolved ? resolved.Body->Outline.Kind : ShapeKind2D::Circle;
    }

    Vector2 Body2D::Size() const
    {
        Resolved resolved = Resolve(*Reference);
        return resolved ? resolved.Body->Size : Vector2 {};
    }

    float Body2D::Radius() const
    {
        Resolved resolved = Resolve(*Reference);
        return resolved ? resolved.Body->Outline.Radius : 0.0f;
    }

    float Body2D::Length() const
    {
        Resolved resolved = Resolve(*Reference);
        return resolved ? resolved.Body->Length : 0.0f;
    }

    std::vector<Vector2> Body2D::Points() const
    {
        Resolved resolved = Resolve(*Reference);
        if (!resolved)
        {
            return {};
        }
        const internal::physics2d::Shape& outline = resolved.Body->Outline;
        return std::vector<Vector2>(outline.Points, outline.Points + outline.Count);
    }

    bool Body2D::IsSensor() const
    {
        Resolved resolved = Resolve(*Reference);
        return resolved && resolved.Body->Sensor;
    }

    float Body2D::Mass() const
    {
        Resolved resolved = Resolve(*Reference);
        return resolved ? resolved.Body->Mass : 0.0f;
    }

    float Body2D::Inertia() const
    {
        Resolved resolved = Resolve(*Reference);
        return resolved ? resolved.Body->Inertia : 0.0f;
    }

    Vector2 Body2D::Center() const
    {
        Resolved resolved = Resolve(*Reference);
        return resolved ? resolved.Body->Center : Vector2 {};
    }

    Rectangle Body2D::Bounds() const
    {
        Resolved resolved = Resolve(*Reference);
        if (!resolved)
        {
            return {};
        }
        Extent extent = ComputeExtent(resolved.Body->Outline, resolved.Body->Origin);
        return { extent.Lower.X, extent.Lower.Y, extent.Upper.X - extent.Lower.X, extent.Upper.Y - extent.Lower.Y };
    }

    Vector2 Body2D::WorldPoint(Vector2 local) const
    {
        Resolved resolved = Resolve(*Reference);
        return resolved ? Apply(resolved.Body->Origin, local) : local;
    }

    Vector2 Body2D::LocalPoint(Vector2 world) const
    {
        Resolved resolved = Resolve(*Reference);
        return resolved ? Unapply(resolved.Body->Origin, world) : world;
    }

    void Body2D::ApplyForce(Vector2 force) const
    {
        Resolved resolved = Resolve(*Reference);
        if (resolved && resolved.Body->Type == BodyType::Dynamic)
        {
            resolved.Body->Force += force;
        }
    }

    void Body2D::ApplyForce(Vector2 force, Vector2 point) const
    {
        Resolved resolved = Resolve(*Reference);
        if (resolved && resolved.Body->Type == BodyType::Dynamic)
        {
            resolved.Body->Force += force;
            resolved.Body->Torque += Cross(point - resolved.Body->Center, force);
        }
    }

    void Body2D::ApplyImpulse(Vector2 impulse) const
    {
        Resolved resolved = Resolve(*Reference);
        if (resolved && resolved.Body->Type == BodyType::Dynamic)
        {
            resolved.Body->Velocity += impulse * resolved.Body->InverseMass;
        }
    }

    void Body2D::ApplyImpulse(Vector2 impulse, Vector2 point) const
    {
        Resolved resolved = Resolve(*Reference);
        if (resolved && resolved.Body->Type == BodyType::Dynamic)
        {
            resolved.Body->Velocity += impulse * resolved.Body->InverseMass;
            resolved.Body->AngularVelocity += resolved.Body->InverseInertia * Cross(point - resolved.Body->Center, impulse);
        }
    }

    void Body2D::ApplyTorque(float torque) const
    {
        Resolved resolved = Resolve(*Reference);
        if (resolved && resolved.Body->Type == BodyType::Dynamic)
        {
            resolved.Body->Torque += torque;
        }
    }

    void Body2D::ApplyAngularImpulse(float impulse) const
    {
        Resolved resolved = Resolve(*Reference);
        if (resolved && resolved.Body->Type == BodyType::Dynamic)
        {
            resolved.Body->AngularVelocity += resolved.Body->InverseInertia * impulse;
        }
    }

    void Body2D::Remove() const
    {
        if (Resolved resolved = Resolve(*Reference))
        {
            resolved.World->RemoveBody(resolved.Index);
        }
    }

    // ---- Joint2D -----------------------------------------------------------------

    Joint2D::Joint2D() : Joint2D(std::make_shared<JointReference2D>())
    {
    }

    Joint2D::Joint2D(std::shared_ptr<JointReference2D> reference)
        : MotorSpeed(reference.get(),
              [](const void* owner) {
                  ResolvedJoint resolved = ResolveJoint(owner);
                  return resolved.Joint ? resolved.Joint->MotorSpeed : 0.0f;
              },
              [](void* owner, const float& value) {
                  if (ResolvedJoint resolved = ResolveJoint(owner); resolved.Joint)
                  {
                      resolved.Joint->MotorSpeed = value;
                  }
              }),
          Length(reference.get(),
              [](const void* owner) {
                  ResolvedJoint resolved = ResolveJoint(owner);
                  return resolved.Joint ? resolved.Joint->Length : 0.0f;
              },
              [](void* owner, const float& value) {
                  if (ResolvedJoint resolved = ResolveJoint(owner); resolved.Joint)
                  {
                      resolved.Joint->Length = std::max(value, 0.0f);
                  }
              }),
          LinearOffset(reference.get(),
              [](const void* owner) {
                  ResolvedJoint resolved = ResolveJoint(owner);
                  return resolved.Joint ? resolved.Joint->LinearOffset : Vector2 {};
              },
              [](void* owner, const Vector2& value) {
                  if (ResolvedJoint resolved = ResolveJoint(owner); resolved.Joint)
                  {
                      resolved.Joint->LinearOffset = value;
                  }
              }),
          AngularOffset(reference.get(),
              [](const void* owner) {
                  ResolvedJoint resolved = ResolveJoint(owner);
                  return resolved.Joint ? resolved.Joint->AngularOffset : 0.0f;
              },
              [](void* owner, const float& value) {
                  if (ResolvedJoint resolved = ResolveJoint(owner); resolved.Joint)
                  {
                      resolved.Joint->AngularOffset = value;
                  }
              }),
          Reference(std::move(reference))
    {
    }

    Joint2D::Joint2D(const Joint2D& other) : Joint2D(other.Reference)
    {
    }

    Joint2D& Joint2D::operator=(const Joint2D& other)
    {
        if (this != &other)
        {
            Reference = other.Reference;
            RebindProperties();
        }
        return *this;
    }

    Joint2D::~Joint2D() = default;

    void Joint2D::RebindProperties()
    {
        void* owner = Reference.get();
        MotorSpeed.Rebind(owner);
        Length.Rebind(owner);
        LinearOffset.Rebind(owner);
        AngularOffset.Rebind(owner);
    }

    Joint2D::operator bool() const
    {
        return ResolveJoint(Reference.get()).Joint != nullptr;
    }

    Body2D Joint2D::First() const
    {
        ResolvedJoint resolved = ResolveJoint(Reference.get());
        return resolved.Joint ? BodyReference2D::Make(resolved.World, resolved.Joint->First) : Body2D();
    }

    Body2D Joint2D::Second() const
    {
        ResolvedJoint resolved = ResolveJoint(Reference.get());
        return resolved.Joint ? BodyReference2D::Make(resolved.World, resolved.Joint->Second) : Body2D();
    }

    float Joint2D::Angle() const
    {
        ResolvedJoint resolved = ResolveJoint(Reference.get());
        if (!resolved.Joint)
        {
            return 0.0f;
        }
        const BodyData& first = resolved.World->Bodies[static_cast<std::size_t>(resolved.Joint->First)];
        const BodyData& second = resolved.World->Bodies[static_cast<std::size_t>(resolved.Joint->Second)];
        return Wrapped(RelativeAngle(first.Origin.Turn, second.Origin.Turn) - resolved.Joint->ReferenceAngle);
    }

    float Joint2D::Translation() const
    {
        ResolvedJoint resolved = ResolveJoint(Reference.get());
        if (!resolved.Joint)
        {
            return 0.0f;
        }
        const JointData& joint = *resolved.Joint;
        const BodyData& first = resolved.World->Bodies[static_cast<std::size_t>(joint.First)];
        const BodyData& second = resolved.World->Bodies[static_cast<std::size_t>(joint.Second)];
        Vector2 between = (second.Center + Rotate(second.Origin.Turn, joint.LocalArmSecond)) - (first.Center + Rotate(first.Origin.Turn, joint.LocalArmFirst));
        return Dot(Rotate(first.Origin.Turn, joint.LocalAxis), between);
    }

    void Joint2D::Remove() const
    {
        ResolvedJoint resolved = ResolveJoint(Reference.get());
        if (resolved.Joint)
        {
            resolved.World->RemoveJoint(resolved.Index);
        }
    }

    // ---- Physics2D ---------------------------------------------------------------

    namespace
    {
        template <typename Settings>
        BodyDefinition Common(const Settings& settings)
        {
            BodyDefinition definition;
            definition.Position = settings.Position;
            definition.Rotation = settings.Rotation;
            definition.Type = settings.Type;
            definition.Velocity = settings.Velocity;
            definition.AngularVelocity = settings.AngularVelocity;
            definition.Density = settings.Density;
            definition.Friction = settings.Friction;
            definition.Restitution = settings.Restitution;
            definition.LinearDamping = settings.LinearDamping;
            definition.AngularDamping = settings.AngularDamping;
            definition.GravityScale = settings.GravityScale;
            definition.FixedRotation = settings.FixedRotation;
            definition.Sensor = settings.Sensor;
            definition.Layer = settings.Layer;
            definition.CollidesWith = settings.CollidesWith;
            return definition;
        }
    }

    Physics2D::Physics2D() : Physics2D(std::shared_ptr<WorldState2D>())
    {
    }

    Physics2D::Physics2D(std::shared_ptr<WorldState2D> state)
        : Gravity(state.get(),
              [](const void* owner) { return owner ? static_cast<const WorldState2D*>(owner)->Gravity : Vector2 {}; },
              [](void* owner, const Vector2& value) {
                  if (owner)
                  {
                      static_cast<WorldState2D*>(owner)->Gravity = value;
                  }
              }),
          Substeps(state.get(),
              [](const void* owner) { return owner ? static_cast<const WorldState2D*>(owner)->Substeps : 0; },
              [](void* owner, const int& value) {
                  if (owner)
                  {
                      static_cast<WorldState2D*>(owner)->Substeps = std::clamp(value, 1, 64);
                  }
              }),
          State(std::move(state))
    {
    }

    Physics2D Physics2D::New(const Physics2DSettings& settings)
    {
        auto state = std::make_shared<WorldState2D>(settings);
        state->Self = state;
        return Physics2D(std::move(state));
    }

    Physics2D::Physics2D(const Physics2D& other) : Physics2D(other.State)
    {
    }

    Physics2D& Physics2D::operator=(const Physics2D& other)
    {
        if (this != &other)
        {
            State = other.State;
            RebindProperties();
        }
        return *this;
    }

    Physics2D::~Physics2D() = default;

    void Physics2D::RebindProperties()
    {
        Gravity.Rebind(State.get());
        Substeps.Rebind(State.get());
    }

    Physics2D::operator bool() const
    {
        return State != nullptr;
    }

    Body2D Physics2D::AddBox(const BoxSettings& settings) const
    {
        if (!State)
        {
            return {};
        }
        BodyDefinition definition = Common(settings);
        definition.Outline = MakeBox(settings.Size, settings.Rounding);
        definition.Size = { std::max(settings.Size.X, LinearSlop), std::max(settings.Size.Y, LinearSlop) };
        return BodyReference2D::Make(State, State->AddBody(definition));
    }

    Body2D Physics2D::AddCircle(const CircleSettings& settings) const
    {
        if (!State)
        {
            return {};
        }
        BodyDefinition definition = Common(settings);
        definition.Outline = MakeCircle(settings.Radius);
        float diameter = 2.0f * definition.Outline.Radius;
        definition.Size = { diameter, diameter };
        return BodyReference2D::Make(State, State->AddBody(definition));
    }

    Body2D Physics2D::AddCapsule(const CapsuleSettings& settings) const
    {
        if (!State)
        {
            return {};
        }
        BodyDefinition definition = Common(settings);
        definition.Outline = MakeCapsule(settings.Length, settings.Radius);
        definition.Length = Distance(definition.Outline.Points[0], definition.Outline.Points[1]);
        float diameter = 2.0f * definition.Outline.Radius;
        definition.Size = { definition.Length + diameter, diameter };
        return BodyReference2D::Make(State, State->AddBody(definition));
    }

    Body2D Physics2D::AddPolygon(const PolygonSettings& settings) const
    {
        if (!State)
        {
            return {};
        }
        std::optional<internal::physics2d::Shape> outline = MakePolygon(settings.Points, settings.Rounding);
        if (!outline)
        {
            return {};
        }
        BodyDefinition definition = Common(settings);
        definition.Outline = *outline;
        Extent extent = ComputeExtent(*outline, {});
        definition.Size = extent.Upper - extent.Lower;
        return BodyReference2D::Make(State, State->AddBody(definition));
    }

    namespace
    {
        // Fills in the bodies and anchors every joint has, or returns false when
        // the bodies are not two different bodies of this world.
        bool StartJoint(const std::shared_ptr<WorldState2D>& world, const BodyReference2D& first, const BodyReference2D& second,
            Vector2 firstAnchor, Vector2 secondAnchor, JointData& joint)
        {
            if (!world)
            {
                return false;
            }
            Resolved one = Resolve(first);
            Resolved other = Resolve(second);
            if (!one || !other || one.World != world || other.World != world || one.Index == other.Index)
            {
                return false;
            }
            joint.First = one.Index;
            joint.Second = other.Index;
            joint.LocalArmFirst = Unapply(one.Body->Origin, firstAnchor) - one.Body->LocalCenter;
            joint.LocalArmSecond = Unapply(other.Body->Origin, secondAnchor) - other.Body->LocalCenter;
            joint.ReferenceAngle = RelativeAngle(one.Body->Origin.Turn, other.Body->Origin.Turn);
            return true;
        }

        std::shared_ptr<JointReference2D> AddedJoint(const std::shared_ptr<WorldState2D>& world, const JointData& joint)
        {
            int index = world->AddJoint(joint);
            auto reference = std::make_shared<JointReference2D>();
            reference->World = world;
            reference->Index = index;
            reference->Generation = world->Joints[static_cast<std::size_t>(index)].Generation;
            return reference;
        }
    }

    Joint2D Physics2D::AddDistanceJoint(const DistanceJointSettings& settings) const
    {
        JointData joint;
        joint.Kind = JointKind::Distance;
        if (!StartJoint(State, *settings.First.Reference, *settings.Second.Reference, settings.FirstAnchor, settings.SecondAnchor, joint))
        {
            return {};
        }
        joint.Length = settings.Length < 0.0f ? Distance(settings.FirstAnchor, settings.SecondAnchor) : settings.Length;
        joint.MinimumLength = std::max(settings.MinimumLength, 0.0f);
        joint.MaximumLength = std::max(settings.MaximumLength, joint.MinimumLength);
        joint.Stiffness = std::max(settings.Stiffness, 0.0f);
        joint.Damping = std::max(settings.Damping, 0.0f);
        joint.Collide = settings.Collide;
        return Joint2D(AddedJoint(State, joint));
    }

    Joint2D Physics2D::AddHingeJoint(const HingeJointSettings& settings) const
    {
        JointData joint;
        joint.Kind = JointKind::Hinge;
        if (!StartJoint(State, *settings.First.Reference, *settings.Second.Reference, settings.Anchor, settings.Anchor, joint))
        {
            return {};
        }
        joint.Limit = settings.Limit;
        joint.Lower = std::min(settings.LowerAngle, settings.UpperAngle);
        joint.Upper = std::max(settings.LowerAngle, settings.UpperAngle);
        joint.Motor = settings.Motor;
        joint.MotorSpeed = settings.MotorSpeed;
        joint.MaximumMotor = std::max(settings.MaximumMotorTorque, 0.0f);
        joint.Collide = settings.Collide;
        return Joint2D(AddedJoint(State, joint));
    }

    Joint2D Physics2D::AddSliderJoint(const SliderJointSettings& settings) const
    {
        JointData joint;
        joint.Kind = JointKind::Slider;
        if (!StartJoint(State, *settings.First.Reference, *settings.Second.Reference, settings.Anchor, settings.Anchor, joint))
        {
            return {};
        }
        Vector2 axis = DirectionOf(settings.Axis).value_or(Vector2 { 1.0f, 0.0f });
        joint.LocalAxis = Unrotate(State->Bodies[static_cast<std::size_t>(joint.First)].Origin.Turn, axis);
        joint.Limit = settings.Limit;
        joint.Lower = std::min(settings.Lower, settings.Upper);
        joint.Upper = std::max(settings.Lower, settings.Upper);
        joint.Motor = settings.Motor;
        joint.MotorSpeed = settings.MotorSpeed;
        joint.MaximumMotor = std::max(settings.MaximumMotorForce, 0.0f);
        joint.Collide = settings.Collide;
        return Joint2D(AddedJoint(State, joint));
    }

    Joint2D Physics2D::AddWeldJoint(const WeldJointSettings& settings) const
    {
        JointData joint;
        joint.Kind = JointKind::Weld;
        if (!StartJoint(State, *settings.First.Reference, *settings.Second.Reference, settings.Anchor, settings.Anchor, joint))
        {
            return {};
        }
        joint.Stiffness = std::max(settings.Stiffness, 0.0f);
        joint.Damping = std::max(settings.Damping, 0.0f);
        joint.Collide = settings.Collide;
        return Joint2D(AddedJoint(State, joint));
    }

    Joint2D Physics2D::AddMotorJoint(const MotorJointSettings& settings) const
    {
        JointData joint;
        joint.Kind = JointKind::Motor;
        if (!StartJoint(State, *settings.First.Reference, *settings.Second.Reference, {}, {}, joint))
        {
            return {};
        }
        joint.LinearOffset = settings.LinearOffset;
        joint.AngularOffset = settings.AngularOffset;
        joint.MaximumForce = std::max(settings.MaximumForce, 0.0f);
        joint.MaximumTorque = std::max(settings.MaximumTorque, 0.0f);
        joint.Correction = std::clamp(settings.Correction, 0.0f, 1.0f);
        joint.Collide = settings.Collide;
        return Joint2D(AddedJoint(State, joint));
    }

    void Physics2D::Step(float seconds) const
    {
        // Kept for the whole step: a touch handler may let go of this handle.
        if (std::shared_ptr<WorldState2D> kept = State)
        {
            kept->Step(seconds);
        }
    }

    RayHit2D Physics2D::CastRay(Vector2 origin, Vector2 direction, float distance, std::uint32_t layers) const
    {
        return State ? State->CastRay(origin, direction, distance, 0.0f, layers) : RayHit2D {};
    }

    RayHit2D Physics2D::CastCircle(Vector2 center, float radius, Vector2 direction, float distance, std::uint32_t layers) const
    {
        return State ? State->CastRay(center, direction, distance, std::max(radius, 0.0f), layers) : RayHit2D {};
    }

    std::vector<Body2D> Physics2D::BodiesAt(Vector2 point, std::uint32_t layers) const
    {
        std::vector<Body2D> bodies;
        if (State)
        {
            for (int index : State->BodiesAt(point, layers))
            {
                bodies.push_back(BodyReference2D::Make(State, index));
            }
        }
        return bodies;
    }

    std::vector<Body2D> Physics2D::BodiesIn(const Rectangle& area, std::uint32_t layers) const
    {
        std::vector<Body2D> bodies;
        if (State)
        {
            for (int index : State->BodiesIn(area, layers))
            {
                bodies.push_back(BodyReference2D::Make(State, index));
            }
        }
        return bodies;
    }

    std::vector<Body2D> Physics2D::Bodies() const
    {
        std::vector<Body2D> bodies;
        if (State)
        {
            for (std::size_t index = 0; index < State->Bodies.size(); ++index)
            {
                if (State->Bodies[index].Alive)
                {
                    bodies.push_back(BodyReference2D::Make(State, static_cast<int>(index)));
                }
            }
        }
        return bodies;
    }

    std::size_t Physics2D::BodyCount() const
    {
        if (!State)
        {
            return 0;
        }
        return static_cast<std::size_t>(std::count_if(State->Bodies.begin(), State->Bodies.end(), [](const BodyData& body) { return body.Alive; }));
    }
}
