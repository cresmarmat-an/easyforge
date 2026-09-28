# Shaders in the build

`easyforge_add_shaders` checks a program's shaders every time they change and
copies them next to the program, where `Shader::Load` finds them:

```cmake
add_executable(notes main.cpp)
target_link_libraries(notes PRIVATE easyforge::window easyforge::graphics)
easyforge_add_shaders(notes ripple.shader shaders/glow.shader)
```

A shader with a problem stops the build, with its file, line, and column:

```
ripple.shader:6:22: 'wave' is not declared
```

The check runs the whole shader through the same compiler the program uses, and
then through the GPU's own compiler on Windows' software renderer, so a shader
that builds is one the program can draw. Relative paths are relative to the
folder of the `CMakeLists.txt` that calls the function.

## The tool

The check is done by `easyforge-shader`, a small program built from
easyforge's `tools` folder whenever `graphics` is built. When easyforge is
fetched, it is only compiled if a program uses `easyforge_add_shaders`. It can
be run by hand:

```
easyforge-shader ripple.shader glow.shader
easyforge-shader --code ripple.shader
```

It prints every problem and exits with 1 if there were any; `--code` also prints
the code each shader became for the GPU.

## Limitations

- Shaders are copied as source and turned into the GPU's language when the
  program first draws them; there is no packing of compiled shaders yet.
- The copy goes into the program's folder, not into a pack.
