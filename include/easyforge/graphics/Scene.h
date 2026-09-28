#pragma once

#include <cstddef>
#include <memory>

#include <easyforge/core/Color.h>
#include <easyforge/core/Property.h>
#include <easyforge/core/Quaternion.h>
#include <easyforge/core/Vector.h>
#include <easyforge/graphics/Model.h>

namespace easyforge
{
    namespace internal
    {
        class SceneState;
        struct SceneObjectRecord;
    }

    // Where a scene is seen from. The camera looks from Position toward Target,
    // with Up pointing to the top of the picture.
    struct Camera
    {
        Vector3 Position { 0.0f, 2.0f, 6.0f };
        Vector3 Target;
        Vector3 Up { 0.0f, 1.0f, 0.0f };

        // How much the camera sees from the bottom of the picture to the top, in degrees.
        float FieldOfView = 60.0f;

        // Nothing closer than Near or farther than Far is drawn.
        float Near = 0.1f;
        float Far = 1000.0f;

        bool operator==(const Camera&) const = default;
    };

    // Light from far away that reaches everything from one direction.
    struct Sun
    {
        // The way the light travels, from the sun toward the ground.
        Vector3 Direction { -1.0f, -2.0f, -1.0f };
        easyforge::Color Color = easyforge::Color::White;
        float Intensity = 1.0f;

        bool operator==(const Sun&) const = default;
    };

    struct SceneObjectSettings
    {
        Vector3 Position;
        Quaternion Rotation;
        Vector3 Scale { 1.0f, 1.0f, 1.0f };
        bool Visible = true;
    };

    // A model placed in a scene. Change it through its properties:
    //
    //     SceneObject ship = scene.Add(model);
    //     ship.Position = { 0, 1, 0 };
    //     ship.Rotation = Quaternion::FromAngles(0, angle, 0);
    //
    // SceneObject is a handle: copies refer to the same object.
    class SceneObject
    {
    public:
        // No object. Tests as false.
        SceneObject();

        SceneObject(const SceneObject& other);
        SceneObject& operator=(const SceneObject& other);
        ~SceneObject();

        // True while the object is in a scene.
        explicit operator bool() const;

        Property<Vector3> Position;
        Property<Quaternion> Rotation;
        Property<Vector3> Scale;
        Property<bool> Visible;

        // The record the scene keeps; used by the graphics library itself.
        const std::shared_ptr<internal::SceneObjectRecord>& Record() const { return Shared; }

    private:
        friend class Scene;
        explicit SceneObject(std::shared_ptr<internal::SceneObjectRecord> record);
        void RebindProperties();

        std::shared_ptr<internal::SceneObjectRecord> Shared;
    };

    // 3D models, a camera, and light, drawn with Canvas::Draw.
    //
    //     Scene scene = Scene::New();
    //     scene.Add(Model::Load("ship.obj"));
    //     scene.Camera = { .Position = { 0, 2, 6 }, .Target = { 0, 0, 0 } };
    //
    // Scene is a handle: copies refer to the same scene.
    class Scene
    {
    public:
        static Scene New();

        // No scene. Tests as false.
        Scene();

        Scene(const Scene& other);
        Scene& operator=(const Scene& other);
        ~Scene();

        explicit operator bool() const;

        SceneObject Add(const Model& model, const SceneObjectSettings& settings = {}) const;
        void Remove(const SceneObject& object) const;
        std::size_t ObjectCount() const;

        Property<easyforge::Camera> Camera;
        Property<easyforge::Sun> Sun;

        // Light that reaches every surface equally, so the side away from the sun
        // is not black.
        Property<easyforge::Color> Ambient;

        // The color behind every object.
        Property<easyforge::Color> Background;

        // The shared state; used by the graphics library itself.
        const std::shared_ptr<internal::SceneState>& State() const { return Shared; }

    private:
        explicit Scene(std::shared_ptr<internal::SceneState> state);
        void RebindProperties();

        std::shared_ptr<internal::SceneState> Shared;
    };
}
