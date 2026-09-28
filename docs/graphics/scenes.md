# Scenes

A `Scene` holds 3D models, a camera, and a sun. It is drawn with
`Canvas::Draw`, into the whole canvas or an area of it.

```cpp
Scene scene = Scene::New();
Model ship = Model::Load("ship.obj");
SceneObject object = scene.Add(ship, { .Position = { 0, 0, 0 } });

scene.Camera = { .Position = { 0, 2, 6 }, .Target = { 0, 0, 0 } };
scene.Sun = { .Direction = { -1, -2, -1 }, .Color = Color::White };

float angle = 0.0f;
window.OnFrame = [&](float deltaSeconds) {
    angle += deltaSeconds;
    object.Rotation = Quaternion::FromAngles(0, angle, 0);

    Canvas canvas = renderer.BeginFrame();
    canvas.Draw(scene);
    renderer.EndFrame();
};
```

easyforge's 3D conventions are those of [core](../core/overview.md): right
handed, Y up, and a camera that looks down -Z.

## Models

`Model::Load` reads an OBJ file with its MTL materials through
[ModelData](../assets/models.md); `Model::FromData` uses one already loaded.
Each material's base color is used, and its base color texture when it has one.
A texture that cannot be loaded is left out with a warning, and the material's
color is used alone.

`model.Bounds()` is the box around the model, which helps to place the camera.

## Objects

`scene.Add(model, settings)` places a model and gives back a `SceneObject`,
whose properties can change at any time:

| Property | Default | Meaning |
|---|---|---|
| `Position` | 0, 0, 0 | Where the model's origin is |
| `Rotation` | none | A `Quaternion` |
| `Scale` | 1, 1, 1 | Size along each axis |
| `Visible` | true | Whether it is drawn |

The same model can be added any number of times; its meshes are sent to the GPU
once. `scene.Remove(object)` takes an object out, and `scene.ObjectCount()`
counts them.

## The camera

| Camera | Default | Meaning |
|---|---|---|
| `Position` | 0, 2, 6 | Where the camera is |
| `Target` | 0, 0, 0 | The point it looks at |
| `Up` | 0, 1, 0 | Which way is the top of the picture |
| `FieldOfView` | 60 | How much it sees from bottom to top, in degrees |
| `Near`, `Far` | 0.1, 1000 | Nothing closer or farther is drawn |

The picture takes the shape of the area it is drawn into, so a wider area sees
more to the sides.

## Light

| Sun | Default | Meaning |
|---|---|---|
| `Direction` | -1, -2, -1 | The way the light travels, from the sun toward the ground |
| `Color` | white | |
| `Intensity` | 1 | Multiplies the color |

`scene.Ambient` is light that reaches every surface equally, so the side away
from the sun is not black, and `scene.Background` is the color behind every
object.

Surfaces are lit by how directly they face the sun, with a soft highlight that
grows sharper for materials with a lower roughness. Lighting is worked out in
linear light.

## How it is drawn

A scene is drawn into its own picture with four samples per pixel, which
smooths the edges of models, before the frame's 2D shapes, and then placed on
the canvas where `Draw` was called. Its objects, camera, and light are taken as
they are when `Draw` is called, so changing them afterwards affects the next
frame.

## Limitations

- One sun and ambient light; no point lights, spot lights, or shadows yet.
- Materials use only their base color, texture, and roughness; metals, normal
  maps, and emissive light arrive with the rest of 3D in stage 6.
- No transparency within a scene, and no picking objects with the mouse.
- No skinned or animated models, and only OBJ files, until stage 6.
