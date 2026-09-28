#include <easyforge/graphics/Scene.h>

#include <algorithm>

#include "Resources.h"

namespace easyforge
{
    namespace
    {
        using internal::SceneObjectRecord;
        using internal::SceneState;

        SceneObjectRecord& RecordOf(void* owner)
        {
            return *static_cast<SceneObjectRecord*>(owner);
        }

        const SceneObjectRecord& RecordOf(const void* owner)
        {
            return *static_cast<const SceneObjectRecord*>(owner);
        }

        SceneState& SceneOf(void* owner)
        {
            return *static_cast<SceneState*>(owner);
        }

        const SceneState& SceneOf(const void* owner)
        {
            return *static_cast<const SceneState*>(owner);
        }
    }

    SceneObject::SceneObject() : SceneObject(std::make_shared<SceneObjectRecord>())
    {
    }

    SceneObject::SceneObject(const SceneObject& other) : SceneObject(other.Shared)
    {
    }

    SceneObject& SceneObject::operator=(const SceneObject& other)
    {
        if (this != &other)
        {
            Shared = other.Shared;
            RebindProperties();
        }
        return *this;
    }

    SceneObject::~SceneObject() = default;

    SceneObject::SceneObject(std::shared_ptr<internal::SceneObjectRecord> record)
        : Position(record.get(),
              [](const void* owner) { return RecordOf(owner).Position; },
              [](void* owner, const Vector3& value) { RecordOf(owner).Position = value; }),
          Rotation(record.get(),
              [](const void* owner) { return RecordOf(owner).Rotation; },
              [](void* owner, const Quaternion& value) { RecordOf(owner).Rotation = value; }),
          Scale(record.get(),
              [](const void* owner) { return RecordOf(owner).Scale; },
              [](void* owner, const Vector3& value) { RecordOf(owner).Scale = value; }),
          Visible(record.get(),
              [](const void* owner) { return RecordOf(owner).Visible; },
              [](void* owner, const bool& value) { RecordOf(owner).Visible = value; }),
          Shared(std::move(record))
    {
    }

    void SceneObject::RebindProperties()
    {
        Position.Rebind(Shared.get());
        Rotation.Rebind(Shared.get());
        Scale.Rebind(Shared.get());
        Visible.Rebind(Shared.get());
    }

    SceneObject::operator bool() const
    {
        return Shared->InScene;
    }

    Scene Scene::New()
    {
        auto state = std::make_shared<SceneState>();
        state->Made = true;
        return Scene(state);
    }

    Scene::Scene() : Scene(std::make_shared<SceneState>())
    {
    }

    Scene::Scene(const Scene& other) : Scene(other.Shared)
    {
    }

    Scene& Scene::operator=(const Scene& other)
    {
        if (this != &other)
        {
            Shared = other.Shared;
            RebindProperties();
        }
        return *this;
    }

    Scene::~Scene() = default;

    Scene::Scene(std::shared_ptr<internal::SceneState> state)
        : Camera(state.get(),
              [](const void* owner) { return SceneOf(owner).ViewPoint; },
              [](void* owner, const easyforge::Camera& value) { SceneOf(owner).ViewPoint = value; }),
          Sun(state.get(),
              [](const void* owner) { return SceneOf(owner).Light; },
              [](void* owner, const easyforge::Sun& value) { SceneOf(owner).Light = value; }),
          Ambient(state.get(),
              [](const void* owner) { return SceneOf(owner).Ambient; },
              [](void* owner, const Color& value) { SceneOf(owner).Ambient = value; }),
          Background(state.get(),
              [](const void* owner) { return SceneOf(owner).Background; },
              [](void* owner, const Color& value) { SceneOf(owner).Background = value; }),
          Shared(std::move(state))
    {
    }

    void Scene::RebindProperties()
    {
        Camera.Rebind(Shared.get());
        Sun.Rebind(Shared.get());
        Ambient.Rebind(Shared.get());
        Background.Rebind(Shared.get());
    }

    Scene::operator bool() const
    {
        return Shared->Made;
    }

    SceneObject Scene::Add(const Model& model, const SceneObjectSettings& settings) const
    {
        auto record = std::make_shared<SceneObjectRecord>();
        record->Model = model.State();
        record->Position = settings.Position;
        record->Rotation = settings.Rotation;
        record->Scale = settings.Scale;
        record->Visible = settings.Visible;
        if (Shared->Made && model)
        {
            record->InScene = true;
            Shared->Objects.push_back(record);
        }
        return SceneObject(record);
    }

    void Scene::Remove(const SceneObject& object) const
    {
        auto& objects = Shared->Objects;
        auto found = std::find(objects.begin(), objects.end(), object.Record());
        if (found != objects.end())
        {
            (*found)->InScene = false;
            objects.erase(found);
        }
    }

    std::size_t Scene::ObjectCount() const
    {
        return Shared->Objects.size();
    }
}
