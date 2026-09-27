easyforge_add_library(assets
    SOURCES
        Compression.cpp
        Files.cpp
        fonts/FontData.cpp
        fonts/FontRasterizer.cpp
        images/Bmp.cpp
        images/ImageData.cpp
        images/Jpeg.cpp
        images/Png.cpp
        images/Qoi.cpp
        images/Tga.cpp
        models/ModelData.cpp
        sounds/Qoa.cpp
        sounds/SoundData.cpp
        sounds/Wav.cpp)

if(WIN32)
    target_sources(easyforge_assets PRIVATE windows/ProgramFolder.cpp)
else()
    target_sources(easyforge_assets PRIVATE standard/ProgramFolder.cpp)
endif()
