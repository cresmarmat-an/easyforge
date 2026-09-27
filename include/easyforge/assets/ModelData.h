#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/assets/Pending.h>
#include <easyforge/core/BoundingBox.h>
#include <easyforge/core/Color.h>
#include <easyforge/core/Vector.h>

namespace easyforge
{
    struct MeshVertex
    {
        Vector3 Position;
        Vector3 Normal;

        // Texture coordinates with (0, 0) at the top left of the image, matching
        // ImageData's rows. OBJ files store V from the bottom; it is flipped on loading.
        Vector2 TextureCoordinate;
    };

    // Triangles that share one material.
    struct MeshData
    {
        std::string Name;
        std::vector<MeshVertex> Vertices;

        // Three indices into Vertices per triangle, counterclockwise when seen from the front.
        std::vector<std::uint32_t> Indices;

        // Index into ModelData::Materials, or -1 for none.
        int MaterialIndex = -1;
    };

    // How a surface looks. Texture paths are ready to pass to ImageData::Load or
    // Texture::Load: they are relative to the same place the model was loaded from.
    struct MaterialData
    {
        std::string Name;
        Color BaseColor = Color::White;
        std::string BaseColorTexture;
        float Metallic = 0.0f;
        float Roughness = 0.5f;
        Color Emissive = Color::Black;
        std::string NormalTexture;
    };

    // A 3D model in memory: meshes and the materials they use.
    //
    //     ModelData ship = ModelData::Load("ship.obj");
    //
    // Reads OBJ files with their MTL material libraries. glTF and FBX arrive with
    // 3D in stage 6.
    class ModelData
    {
    public:
        std::vector<MeshData> Meshes;
        std::vector<MaterialData> Materials;

        ModelData() = default;

        static ModelData Load(std::string_view path);
        static Pending<ModelData> LoadInBackground(std::string_view path);

        // Reads OBJ text from memory. `materialText` is the MTL library, if any;
        // `folder` is put in front of texture paths.
        static ModelData DecodeObj(
            std::string_view objectText, std::string_view materialText = {}, std::string_view folder = {});

        explicit operator bool() const { return !Meshes.empty() && ErrorText.empty(); }
        const std::string& Error() const { return ErrorText; }

        // The box around every vertex of every mesh.
        BoundingBox Bounds() const;

    private:
        std::string ErrorText;
    };
}
