#include "World.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Solver.h"

namespace easyforge::internal
{
    using namespace physics2d;

    namespace
    {
        std::uint64_t PairKey(int first, int second)
        {
            int lower = std::min(first, second);
            int higher = std::max(first, second);
            return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(lower)) << 32) | static_cast<std::uint32_t>(higher);
        }

        void SetMass(BodyData& body)
        {
            body.Mass = 0.0f;
            body.InverseMass = 0.0f;
            body.Inertia = 0.0f;
            body.InverseInertia = 0.0f;
            body.LocalCenter = {};
            if (body.Type == BodyType::Dynamic)
            {
                MassProperties mass = ComputeMass(body.Outline, body.Density);
                // A dynamic body always has some mass, so forces can move it.
                body.Mass = mass.Mass > 0.0f ? mass.Mass : 1.0f;
                body.InverseMass = 1.0f / body.Mass;
                body.LocalCenter = mass.Mass > 0.0f ? mass.Center : Vector2 {};
                body.Inertia = mass.Mass > 0.0f ? mass.Inertia : 0.0f;
                body.InverseInertia = !body.FixedRotation && body.Inertia > 0.0f ? 1.0f / body.Inertia : 0.0f;
            }
            body.Center = Apply(body.Origin, body.LocalCenter);
        }

        Vector2 ApproachVelocity(const BodyData& first, const BodyData& second, Vector2 point)
        {
            Vector2 armFirst = point - first.Center;
            Vector2 armSecond = point - second.Center;
            return second.Velocity + TurnVelocity(second.AngularVelocity, armSecond) - first.Velocity -
                   TurnVelocity(first.AngularVelocity, armFirst);
        }
    }

    Body2D BodyReference2D::Make(const std::shared_ptr<WorldState2D>& world, int index)
    {
        return Make(world, index, world->Bodies[static_cast<std::size_t>(index)].Generation);
    }

    Body2D BodyReference2D::Make(const std::shared_ptr<WorldState2D>& world, int index, std::uint32_t generation)
    {
        auto reference = std::make_shared<BodyReference2D>();
        reference->World = world;
        reference->Index = index;
        reference->Generation = generation;
        return Body2D(std::move(reference));
    }

    WorldState2D::WorldState2D(const Physics2DSettings& settings) : Gravity(settings.Gravity), Substeps(std::max(settings.Substeps, 1))
    {
    }

    BodyData* WorldState2D::Find(int index, std::uint32_t generation)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= Bodies.size())
        {
            return nullptr;
        }
        BodyData& body = Bodies[static_cast<std::size_t>(index)];
        return body.Alive && body.Generation == generation ? &body : nullptr;
    }

    JointData* WorldState2D::FindJoint(int index, std::uint32_t generation)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= Joints.size())
        {
            return nullptr;
        }
        JointData& joint = Joints[static_cast<std::size_t>(index)];
        return joint.Alive && joint.Generation == generation ? &joint : nullptr;
    }

    // ---- Bodies -----------------------------------------------------------------

    int WorldState2D::AddBody(const BodyDefinition& definition)
    {
        int index;
        if (!FreeBodies.empty())
        {
            index = FreeBodies.back();
            FreeBodies.pop_back();
        }
        else
        {
            index = static_cast<int>(Bodies.size());
            Bodies.emplace_back();
        }
        BodyData& body = Bodies[static_cast<std::size_t>(index)];
        std::uint32_t generation = body.Generation;
        body = BodyData {};
        body.Generation = generation;
        body.Alive = true;
        body.Type = definition.Type;
        body.Outline = definition.Outline;
        body.Size = definition.Size;
        body.Length = definition.Length;
        body.Origin = { definition.Position, Rotation::FromAngle(definition.Rotation) };
        body.Density = std::max(definition.Density, 0.0f);
        body.Friction = std::max(definition.Friction, 0.0f);
        body.Restitution = std::max(definition.Restitution, 0.0f);
        body.LinearDamping = std::max(definition.LinearDamping, 0.0f);
        body.AngularDamping = std::max(definition.AngularDamping, 0.0f);
        body.GravityScale = definition.GravityScale;
        body.FixedRotation = definition.FixedRotation;
        body.Sensor = definition.Sensor;
        body.Layer = definition.Layer;
        body.CollidesWith = definition.CollidesWith;
        SetMass(body);
        if (body.Type != BodyType::Static)
        {
            body.Velocity = definition.Velocity;
            body.AngularVelocity = body.FixedRotation ? 0.0f : definition.AngularVelocity;
        }
        body.Proxy = Tree.Insert(ComputeExtent(body.Outline, body.Origin), index);
        Moved.push_back(index);
        return index;
    }

    void WorldState2D::RemoveBody(int index)
    {
        BodyData& body = Bodies[static_cast<std::size_t>(index)];
        if (!body.Alive)
        {
            return;
        }
        std::vector<int> joints = body.Joints;
        for (int joint : joints)
        {
            RemoveJoint(joint);
        }

        // The bodies it was touching are told it has gone.
        std::vector<TouchEvent> separations;
        for (std::size_t contact = 0; contact < Contacts.size();)
        {
            ContactData& data = Contacts[contact];
            if (data.First != index && data.Second != index)
            {
                ++contact;
                continue;
            }
            if (data.Touching)
            {
                TouchEvent event;
                event.First = data.First;
                event.FirstGeneration = Bodies[static_cast<std::size_t>(data.First)].Generation;
                event.Second = data.Second;
                event.SecondGeneration = Bodies[static_cast<std::size_t>(data.Second)].Generation;
                event.Point = data.Points.Count > 0 ? data.Points.Points[0].Point : Vector2 {};
                event.Normal = data.Points.Normal;
                event.Sensor = data.Sensor;
                event.Began = false;
                separations.push_back(event);
            }
            DestroyContact(contact);
        }

        Tree.Remove(body.Proxy);
        body.Proxy = -1;
        body.Alive = false;
        ++body.Generation;
        body.OnTouch = nullptr;
        body.OnSeparate = nullptr;
        FreeBodies.push_back(index);
        Report(separations);
    }

    void WorldState2D::Placed(int index)
    {
        BodyData& body = Bodies[static_cast<std::size_t>(index)];
        body.Center = Apply(body.Origin, body.LocalCenter);
        if (Tree.Move(body.Proxy, ComputeExtent(body.Outline, body.Origin)))
        {
            Moved.push_back(index);
        }
    }

    void WorldState2D::Retyped(int index)
    {
        BodyData& body = Bodies[static_cast<std::size_t>(index)];
        SetMass(body);
        if (body.Type == BodyType::Static)
        {
            body.Velocity = {};
            body.AngularVelocity = 0.0f;
        }
        Moved.push_back(index);
    }

    // ---- Joints -----------------------------------------------------------------

    int WorldState2D::AddJoint(JointData joint)
    {
        int index;
        if (!FreeJoints.empty())
        {
            index = FreeJoints.back();
            FreeJoints.pop_back();
        }
        else
        {
            index = static_cast<int>(Joints.size());
            Joints.emplace_back();
        }
        joint.Generation = Joints[static_cast<std::size_t>(index)].Generation;
        joint.Alive = true;
        Joints[static_cast<std::size_t>(index)] = joint;
        Bodies[static_cast<std::size_t>(joint.First)].Joints.push_back(index);
        Bodies[static_cast<std::size_t>(joint.Second)].Joints.push_back(index);
        if (!joint.Collide)
        {
            auto found = ContactIndex.find(PairKey(joint.First, joint.Second));
            if (found != ContactIndex.end())
            {
                DestroyContact(found->second);
            }
        }
        return index;
    }

    void WorldState2D::RemoveJoint(int index)
    {
        JointData& joint = Joints[static_cast<std::size_t>(index)];
        if (!joint.Alive)
        {
            return;
        }
        for (int body : { joint.First, joint.Second })
        {
            std::vector<int>& list = Bodies[static_cast<std::size_t>(body)].Joints;
            list.erase(std::remove(list.begin(), list.end(), index), list.end());
            // Bodies that may collide again look for each other.
            Moved.push_back(body);
        }
        joint.Alive = false;
        ++joint.Generation;
        FreeJoints.push_back(index);
    }

    // ---- Contacts ----------------------------------------------------------------

    bool WorldState2D::ShouldCollide(int first, int second) const
    {
        const BodyData& one = Bodies[static_cast<std::size_t>(first)];
        const BodyData& other = Bodies[static_cast<std::size_t>(second)];
        if (!one.Alive || !other.Alive)
        {
            return false;
        }
        bool sensor = one.Sensor || other.Sensor;
        if (one.Type != BodyType::Dynamic && other.Type != BodyType::Dynamic && !sensor)
        {
            return false;
        }
        if (one.Type == BodyType::Static && other.Type == BodyType::Static)
        {
            return false;
        }
        if ((one.Layer & other.CollidesWith) == 0 || (other.Layer & one.CollidesWith) == 0)
        {
            return false;
        }
        for (int joint : one.Joints)
        {
            const JointData& data = Joints[static_cast<std::size_t>(joint)];
            bool joins = (data.First == first && data.Second == second) || (data.First == second && data.Second == first);
            if (joins && !data.Collide)
            {
                return false;
            }
        }
        return true;
    }

    void WorldState2D::FindNewContacts()
    {
        if (Moved.empty())
        {
            return;
        }
        std::sort(Moved.begin(), Moved.end());
        Moved.erase(std::unique(Moved.begin(), Moved.end()), Moved.end());
        std::vector<std::uint64_t> found;
        for (int index : Moved)
        {
            const BodyData& body = Bodies[static_cast<std::size_t>(index)];
            if (!body.Alive)
            {
                continue;
            }
            Tree.Query(Tree.StoredExtent(body.Proxy), [&](int other) {
                if (other != index && ShouldCollide(index, other))
                {
                    std::uint64_t key = PairKey(index, other);
                    if (!ContactIndex.contains(key))
                    {
                        found.push_back(key);
                    }
                }
                return true;
            });
        }
        Moved.clear();

        // Sorted, so contacts are made in the same order every run.
        std::sort(found.begin(), found.end());
        found.erase(std::unique(found.begin(), found.end()), found.end());
        for (std::uint64_t key : found)
        {
            ContactData contact;
            contact.First = static_cast<int>(key >> 32);
            contact.Second = static_cast<int>(key & 0xFFFFFFFFu);
            const BodyData& first = Bodies[static_cast<std::size_t>(contact.First)];
            const BodyData& second = Bodies[static_cast<std::size_t>(contact.Second)];
            contact.Sensor = first.Sensor || second.Sensor;
            ContactIndex.emplace(key, Contacts.size());
            Contacts.push_back(contact);
        }
    }

    void WorldState2D::DestroyContact(std::size_t index)
    {
        ContactIndex.erase(PairKey(Contacts[index].First, Contacts[index].Second));
        if (index + 1 != Contacts.size())
        {
            Contacts[index] = Contacts.back();
            ContactIndex[PairKey(Contacts[index].First, Contacts[index].Second)] = index;
        }
        Contacts.pop_back();
    }

    void WorldState2D::UpdateContacts(std::vector<TouchEvent>& events)
    {
        for (std::size_t index = 0; index < Contacts.size();)
        {
            ContactData& contact = Contacts[index];
            const BodyData& first = Bodies[static_cast<std::size_t>(contact.First)];
            const BodyData& second = Bodies[static_cast<std::size_t>(contact.Second)];

            TouchEvent event;
            event.First = contact.First;
            event.FirstGeneration = first.Generation;
            event.Second = contact.Second;
            event.SecondGeneration = second.Generation;
            event.Sensor = contact.Sensor;

            bool near = Tree.StoredExtent(first.Proxy).Overlaps(Tree.StoredExtent(second.Proxy));
            if (!near || !ShouldCollide(contact.First, contact.Second))
            {
                if (contact.Touching)
                {
                    event.Began = false;
                    event.Point = contact.Points.Count > 0 ? contact.Points.Points[0].Point : Vector2 {};
                    event.Normal = contact.Points.Normal;
                    events.push_back(event);
                }
                DestroyContact(index);
                continue;
            }

            Manifold manifold = Collide(first.Outline, first.Origin, second.Outline, second.Origin);
            float deepest = std::numeric_limits<float>::infinity();
            for (int point = 0; point < manifold.Count; ++point)
            {
                deepest = std::min(deepest, manifold.Points[point].Separation);
                // A point found again keeps the impulses it had, to start from.
                for (int old = 0; old < contact.Points.Count; ++old)
                {
                    if (contact.Points.Points[old].Id == manifold.Points[point].Id)
                    {
                        manifold.Points[point].NormalImpulse = contact.Points.Points[old].NormalImpulse;
                        manifold.Points[point].TangentImpulse = contact.Points.Points[old].TangentImpulse;
                    }
                }
            }
            // Sensors report overlap; solid bodies report touching within the slop.
            bool touching = manifold.Count > 0 && (contact.Sensor ? deepest < 0.0f : deepest < LinearSlop);
            if (touching != contact.Touching)
            {
                const Manifold& source = touching ? manifold : contact.Points;
                event.Began = touching;
                event.Point = source.Count > 0 ? source.Points[0].Point : Vector2 {};
                event.Normal = source.Normal;
                if (touching)
                {
                    event.Speed = std::max(-Dot(ApproachVelocity(first, second, event.Point), event.Normal), 0.0f);
                }
                events.push_back(event);
            }
            contact.Points = manifold;
            contact.Touching = touching;
            ++index;
        }
    }

    // ---- Stepping ----------------------------------------------------------------

    void WorldState2D::Step(float seconds)
    {
        if (!(seconds > 0.0f) || !std::isfinite(seconds))
        {
            return;
        }
        std::vector<TouchEvent> events;
        FindNewContacts();
        UpdateContacts(events);
        Solve(seconds);
        FinishBodies();
        Report(events);
    }

    void WorldState2D::FinishBodies()
    {
        for (std::size_t index = 0; index < Bodies.size(); ++index)
        {
            BodyData& body = Bodies[index];
            if (!body.Alive)
            {
                continue;
            }
            body.Force = {};
            body.Torque = 0.0f;
            if (body.Type == BodyType::Static)
            {
                continue;
            }
            body.Origin.Position = body.Center - Rotate(body.Origin.Turn, body.LocalCenter);
            if (Tree.Move(body.Proxy, ComputeExtent(body.Outline, body.Origin)))
            {
                Moved.push_back(static_cast<int>(index));
            }
        }
    }

    void WorldState2D::Report(const std::vector<TouchEvent>& events)
    {
        if (events.empty())
        {
            return;
        }
        std::shared_ptr<WorldState2D> kept = Self.lock();
        for (const TouchEvent& event : events)
        {
            for (int side = 0; side < 2; ++side)
            {
                int index = side == 0 ? event.First : event.Second;
                std::uint32_t generation = side == 0 ? event.FirstGeneration : event.SecondGeneration;
                BodyData* body = Find(index, generation);
                if (!body)
                {
                    continue;
                }
                std::function<void(const Contact2D&)> callback = event.Began ? body->OnTouch : body->OnSeparate;
                if (!callback)
                {
                    continue;
                }
                Contact2D contact;
                contact.Other = side == 0 ? BodyReference2D::Make(kept, event.Second, event.SecondGeneration)
                                           : BodyReference2D::Make(kept, event.First, event.FirstGeneration);
                contact.Point = event.Point;
                contact.Normal = side == 0 ? event.Normal : -event.Normal;
                contact.Speed = event.Speed;
                contact.Sensor = event.Sensor;
                callback(contact);
            }
        }
    }

    // ---- Queries -----------------------------------------------------------------

    RayHit2D WorldState2D::CastRay(Vector2 origin, Vector2 direction, float distance, float radius, std::uint32_t layers)
    {
        std::optional<Vector2> unit = DirectionOf(direction);
        if (!unit || !(distance > 0.0f))
        {
            return {};
        }
        Vector2 translation = *unit * distance;
        int best = -1;
        float bestFraction = 1.0f;
        Vector2 bestNormal;
        Tree.CastRay(origin, translation, 1.0f, radius, [&](int index, float fraction) {
            const BodyData& body = Bodies[static_cast<std::size_t>(index)];
            if (body.Sensor || (body.Layer & layers) == 0)
            {
                return fraction;
            }
            RayResult hit = physics2d::CastRay(body.Outline, Unapply(body.Origin, origin), Unrotate(body.Origin.Turn, translation), radius);
            if (!hit.Hit || hit.Fraction > fraction || (hit.Fraction == fraction && best >= 0 && index > best))
            {
                return fraction;
            }
            best = index;
            bestFraction = hit.Fraction;
            bestNormal = Rotate(body.Origin.Turn, hit.Normal);
            return hit.Fraction;
        });
        if (best < 0)
        {
            return {};
        }
        RayHit2D result;
        result.Body = BodyReference2D::Make(Self.lock(), best);
        result.Normal = bestNormal;
        result.Point = origin + translation * bestFraction - bestNormal * radius;
        result.Distance = bestFraction * distance;
        return result;
    }

    std::vector<int> WorldState2D::BodiesAt(Vector2 point, std::uint32_t layers)
    {
        std::vector<int> found;
        Tree.Query({ point, point }, [&](int index) {
            const BodyData& body = Bodies[static_cast<std::size_t>(index)];
            if ((body.Layer & layers) != 0 && ContainsPoint(body.Outline, Unapply(body.Origin, point)))
            {
                found.push_back(index);
            }
            return true;
        });
        std::sort(found.begin(), found.end());
        return found;
    }

    std::vector<int> WorldState2D::BodiesIn(const Rectangle& area, std::uint32_t layers)
    {
        std::vector<int> found;
        if (area.Width < 0.0f || area.Height < 0.0f)
        {
            return found;
        }
        Shape box = MakeBox({ area.Width, area.Height }, 0.0f);
        Pose boxPose { area.Center(), {} };
        Extent extent { { area.X, area.Y }, { area.X + area.Width, area.Y + area.Height } };
        Tree.Query(extent, [&](int index) {
            const BodyData& body = Bodies[static_cast<std::size_t>(index)];
            if ((body.Layer & layers) == 0)
            {
                return true;
            }
            Manifold manifold = Collide(body.Outline, body.Origin, box, boxPose);
            for (int point = 0; point < manifold.Count; ++point)
            {
                if (manifold.Points[point].Separation <= 0.0f)
                {
                    found.push_back(index);
                    break;
                }
            }
            return true;
        });
        std::sort(found.begin(), found.end());
        return found;
    }
}
