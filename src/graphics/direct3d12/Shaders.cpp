#include "../gpu/Gpu.h"
#include "Shaders.h"

namespace easyforge::internal::gpu
{
    BuiltInShaders ShadersOfBackend()
    {
        return { direct3d12::ShapeShader, direct3d12::MeshShader };
    }
}
