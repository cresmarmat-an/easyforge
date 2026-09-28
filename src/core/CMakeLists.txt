easyforge_add_library(core
    SOURCES
        Color.cpp
        Jobs.cpp
        Keys.cpp
        language/Parser.cpp
        language/Tokens.cpp
        Log.cpp
        Matrix.cpp
        Quaternion.cpp
        Random.cpp
        Result.cpp
        Testing.cpp
        Transform.cpp)

if(WIN32)
    target_sources(easyforge_core PRIVATE windows/DebuggerOutput.cpp)
else()
    target_sources(easyforge_core PRIVATE standard/DebuggerOutput.cpp)
endif()

find_package(Threads REQUIRED)
target_link_libraries(easyforge_core PUBLIC Threads::Threads)
