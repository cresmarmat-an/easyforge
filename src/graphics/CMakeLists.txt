if(NOT WIN32)
    # Vulkan for Linux and Android, Metal for Apple platforms, and WebGPU for the web
    # arrive in later stages.
    message(STATUS "easyforge graphics has no backend for ${CMAKE_SYSTEM_NAME} yet, so it is not built")
    return()
endif()

easyforge_add_library(graphics
    SOURCES
        Canvas.cpp
        Font.cpp
        Model.cpp
        Renderer.cpp
        Resources.cpp
        Scene.cpp
        Shader.cpp
        Texture.cpp
        shaders/ShaderCompiler.cpp)

target_sources(easyforge_graphics PRIVATE
    direct3d12/Device.cpp
    direct3d12/Shaders.cpp)

target_link_libraries(easyforge_graphics PRIVATE d3d12 dxgi d3dcompiler dcomp dxguid)
