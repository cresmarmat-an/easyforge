#include <easyforge/core/Testing.h>

#include "TestData.h"

using namespace easyforge;

EASYFORGE_TEST(ObjMeshesAndMaterials)
{
    ModelData model = ModelData::Load(testdata::Path("models/cube.obj"));
    EASYFORGE_REQUIRE(model);
    EASYFORGE_REQUIRE(model.Meshes.size() == 2);
    EASYFORGE_REQUIRE(model.Materials.size() == 2);

    // Quads become two triangles; corners that differ only in normal or texture
    // coordinate stay separate vertices.
    const MeshData& painted = model.Meshes[0];
    EASYFORGE_EXPECT_EQUAL(painted.Name, std::string("Cube"));
    EASYFORGE_EXPECT_EQUAL(painted.Indices.size(), std::size_t { 4 * 2 * 3 });
    EASYFORGE_EXPECT_EQUAL(painted.Vertices.size(), std::size_t { 16 });
    EASYFORGE_EXPECT_EQUAL(model.Materials[static_cast<std::size_t>(painted.MaterialIndex)].Name, std::string("Painted"));

    const MeshData& metal = model.Meshes[1];
    EASYFORGE_EXPECT_EQUAL(metal.Indices.size(), std::size_t { 2 * 2 * 3 });
    EASYFORGE_EXPECT_EQUAL(model.Materials[static_cast<std::size_t>(metal.MaterialIndex)].Name, std::string("Metal"));

    // The first vertex: position 1, texture coordinate (0, 0) flipped to (0, 1), normal +Z.
    EASYFORGE_EXPECT_EQUAL(painted.Vertices[0].Position, (Vector3 { -0.5f, -0.5f, 0.5f }));
    EASYFORGE_EXPECT_EQUAL(painted.Vertices[0].TextureCoordinate, (Vector2 { 0.0f, 1.0f }));
    EASYFORGE_EXPECT_EQUAL(painted.Vertices[0].Normal, (Vector3 { 0.0f, 0.0f, 1.0f }));

    const MaterialData& paint = model.Materials[0];
    EASYFORGE_EXPECT_NEAR(paint.BaseColor, Color::FromLinear({ 0.5f, 0.25f, 1.0f, 0.75f }), 0.0001f);
    EASYFORGE_EXPECT_NEAR(paint.Roughness, 0.14142f, 0.0001f);
    EASYFORGE_EXPECT(paint.BaseColorTexture.ends_with("models/textures/paint.png"));

    const MaterialData& metalMaterial = model.Materials[1];
    EASYFORGE_EXPECT_EQUAL(metalMaterial.Metallic, 1.0f);
    EASYFORGE_EXPECT_EQUAL(metalMaterial.Roughness, 0.2f);
    EASYFORGE_EXPECT(metalMaterial.NormalTexture.ends_with("models/textures/metal_normal.png"));

    BoundingBox bounds = model.Bounds();
    EASYFORGE_EXPECT_EQUAL(bounds.Minimum, (Vector3 { -0.5f, -0.5f, -0.5f }));
    EASYFORGE_EXPECT_EQUAL(bounds.Maximum, (Vector3 { 0.5f, 0.5f, 0.5f }));
}

EASYFORGE_TEST(ObjSmoothNormalsAndNegativeIndices)
{
    ModelData model = ModelData::Load(testdata::Path("models/pyramid.obj"));
    EASYFORGE_REQUIRE(model);
    EASYFORGE_REQUIRE(model.Meshes.size() == 1);
    const MeshData& mesh = model.Meshes[0];
    EASYFORGE_EXPECT_EQUAL(mesh.Vertices.size(), std::size_t { 5 });
    EASYFORGE_EXPECT_EQUAL(mesh.Indices.size(), std::size_t { 18 });
    EASYFORGE_EXPECT_EQUAL(mesh.MaterialIndex, -1);

    for (const MeshVertex& vertex : mesh.Vertices)
    {
        EASYFORGE_EXPECT_NEAR(Length(vertex.Normal), 1.0f, 0.0001f);
        if (vertex.Position == Vector3 { 0.5f, 1.0f, 0.5f })
        {
            // The apex is surrounded evenly by the four sides.
            EASYFORGE_EXPECT_NEAR(vertex.Normal, (Vector3 { 0.0f, 1.0f, 0.0f }), 0.0001f);
        }
    }
}

EASYFORGE_TEST(ObjErrors)
{
    ModelData broken = ModelData::Load(testdata::Path("models/broken.obj"));
    EASYFORGE_EXPECT(!broken);
    EASYFORGE_EXPECT(broken.Error().find("line 3") != std::string::npos);

    ModelData fbx = ModelData::Load("ship.fbx");
    EASYFORGE_EXPECT(!fbx);
    EASYFORGE_EXPECT(fbx.Error().find("not supported yet") != std::string::npos);

    ModelData empty = ModelData::DecodeObj("v 0 0 0\n");
    EASYFORGE_EXPECT(!empty);
    EASYFORGE_EXPECT_EQUAL(empty.Error(), std::string("it has no faces"));

    ModelData badNumber = ModelData::DecodeObj("v 0 zero 0\n");
    EASYFORGE_EXPECT(badNumber.Error().find("line 1") != std::string::npos);
}

EASYFORGE_TEST(ObjFromText)
{
    ModelData model = ModelData::DecodeObj("v 0 0 0\r\nv 1 0 0\r\nv 0 1 0\r\ng Triangle\r\nf 1 2 3\r\n",
        "newmtl Unused\nKd 1 0 0\n", "art/");
    EASYFORGE_REQUIRE(model);
    EASYFORGE_EXPECT_EQUAL(model.Meshes[0].Name, std::string("Triangle"));
    EASYFORGE_EXPECT_EQUAL(model.Materials.size(), std::size_t { 1 });
    EASYFORGE_EXPECT_NEAR(model.Meshes[0].Vertices[0].Normal, (Vector3 { 0.0f, 0.0f, 1.0f }), 0.0001f);
}
