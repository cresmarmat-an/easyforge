# graphics

`graphics` will draw 2D and 3D on Direct3D 12, Vulkan, Metal, and WebGPU, with
one interface that works the same on all four.

> [!NOTE] Not available yet
> `graphics` is step 5 of stage 1, starting with Direct3D 12. This page will
> describe how to use it once it exists.

What it is planned to do:

- A `Canvas` for 2D: rectangles with rounded corners, lines, circles, images,
  and text, batched into as few GPU calls as possible. The same `Canvas` is used
  inside `ui` for custom drawing.
- A `Scene` for 3D: models, a camera, and lights, later with shadows, skinned
  animation, and physically based materials.
- Textures, fonts, models, and shaders that can be loaded anywhere and are sent
  to the GPU the first time something draws them.
- `GraphicsDevice`, the explicit layer underneath, for people who want to write
  their own rendering.
- An easyforge shader language that compiles to HLSL, SPIR-V, WGSL, and MSL, so
  one shader works on every backend.

The planned design is in
[DESIGN.md](https://github.com/cresmarmat-an/easyforge/blob/main/DESIGN.md#graphics).
