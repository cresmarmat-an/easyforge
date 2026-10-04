easyforge_add_library(sound
    SOURCES
        Engine.cpp
        Mixer.cpp
        MixerState.cpp
        Sound.cpp)

if(WIN32)
    target_sources(easyforge_sound PRIVATE windows/WasapiDevice.cpp)
    target_link_libraries(easyforge_sound PRIVATE ole32 avrt)
else()
    # Output devices on Linux, Apple platforms, Android, and the web arrive with
    # those platforms; offline mixing works everywhere.
    target_sources(easyforge_sound PRIVATE standard/NoDevice.cpp)
endif()
