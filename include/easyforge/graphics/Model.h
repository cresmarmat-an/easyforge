#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <easyforge/assets/ModelData.h>
#include <easyforge/core/BoundingBox.h>

namespace easyforge
{
    namespace internal
    {
        class ModelState;
    }

    // A 3D model ready to draw: meshes, materials, and their textures. Like a
    // texture, it is sent to the GPU the first time a scene with it is drawn.
    //
    //     Model ship = Model::Load("ship.obj");
    //     scene.Add(ship);
    //
    // Model is a handle: copies refer to the same model.
    class Model
    {
    public:
        static Model Load(std::string_view path);
        static Model FromData(ModelData data);

        // No model. Tests as false.
        Model();

        explicit operator bool() const;
        const std::string& Error() const;

        // The box around every vertex, before any object's position, rotation, or scale.
        BoundingBox Bounds() const;

        const ModelData& Data() const;

        // The shared state; used by the graphics library itself.
        const std::shared_ptr<internal::ModelState>& State() const { return Shared; }

    private:
        explicit Model(std::shared_ptr<internal::ModelState> state);

        std::shared_ptr<internal::ModelState> Shared;
    };
}
