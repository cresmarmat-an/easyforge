easyforge_add_library(input
    SOURCES
        Binding.cpp
        Controls.cpp
        Gamepad.cpp
        Names.cpp
        Recording.cpp)

if(WIN32)
    target_sources(easyforge_input PRIVATE windows/XInputGamepads.cpp)
else()
    # Gamepads on Linux, Apple platforms, Android, and the web arrive with those platforms.
    target_sources(easyforge_input PRIVATE standard/NoGamepads.cpp)
endif()
