# Models

`ModelData` holds a 3D model in memory: meshes of triangles, and the materials
they use.

```cpp
#include <easyforge/assets.h>

using namespace easyforge;

ModelData ship = ModelData::Load("models/ship.obj");
if (!ship)
{
    Log(LogLevel::Error, ship.Error());
    return;
}

for (const MeshData& mesh : ship.Meshes)
{
    Log("{}: {} triangles", mesh.Name, mesh.Indices.size() / 3);
}
BoundingBox bounds = ship.Bounds();
```

## What a model holds

| Type | Member | What it holds |
|---|---|---|
| `ModelData` | `Meshes`, `Materials` | The model's parts |
| | `Bounds()` | The box around every vertex |
| `MeshData` | `Name` | The object or group name from the file |
| | `Vertices` | `MeshVertex` values: `Position`, `Normal`, `TextureCoordinate` |
| | `Indices` | Three per triangle, counterclockwise seen from the front |
| | `MaterialIndex` | Index into `Materials`, or -1 for none |
| `MaterialData` | `Name` | As written in the material file |
| | `BaseColor` | Color and opacity |
| | `BaseColorTexture`, `NormalTexture` | Image paths, ready for `ImageData::Load` |
| | `Metallic`, `Roughness` | From 0 to 1 |
| | `Emissive` | Light the surface gives off |

Texture coordinates have (0, 0) at the top left of the image, matching the rows
of `ImageData`.

## OBJ files

`Load` reads the OBJ file and every material library it names with `mtllib`,
relative to the OBJ file. Texture paths from the material library are made
relative to the same place, so they can be loaded directly. A missing material
library is not an error: the model loads without materials, and a warning is
logged.

What is read:

- Positions, texture coordinates, and normals, including negative indices that
  count back from the latest.
- Faces with any number of corners. Polygons are split into triangles around
  their first corner.
- Objects and groups (`o`, `g`) and materials (`usemtl`). Each combination of
  group and material becomes one mesh, and corners that share position, texture
  coordinate, and normal become one vertex.
- Missing normals are made smooth: each position gets the average of the faces
  around it, weighted by their area.
- OBJ stores texture coordinates from the bottom of the image; they are flipped.

From the MTL file: `Kd` (base color), `d` or `Tr` (opacity), `Ns` (shininess,
turned into roughness), `Pr` and `Pm` (roughness and metallic, where the file has
them), `Ke` (emission), `map_Kd` (base color texture), and `map_Bump`, `bump`, or
`norm` (normal texture). MTL colors are reflectances in linear light and are
converted to sRGB, like every `Color`.

To read OBJ text you already have, use
`ModelData::DecodeObj(objectText, materialText, folder)`.

## Limitations

- glTF and FBX give an error saying they are not supported yet; they arrive with
  3D in stage 6, along with skeletons and animation.
- Polygons are split as if they were convex. A concave polygon, which modelling
  programs rarely export, can come out wrong.
- Lines, points, curves, surfaces, and smoothing groups are ignored.
- Texture options in MTL files (such as `-bm` or `-s`) are skipped; only the file
  name is used.
