add_executable(easyforge-physics-tests
    Main.cpp
    BodyTests.cpp
    CollisionTests.cpp
    JointTests.cpp)

target_link_libraries(easyforge-physics-tests PRIVATE easyforge::physics)
easyforge_set_warnings(easyforge-physics-tests)

add_test(NAME physics COMMAND easyforge-physics-tests)
