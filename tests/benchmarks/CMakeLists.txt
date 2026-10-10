# Programs that time the libraries' heavy work, for finding what to make
# faster. They are not tests and are not run by ctest. Build them in Release:
#
#     cmake -S . -B build -D EASYFORGE_BUILD_BENCHMARKS=ON
#     cmake --build build --config Release --target easyforge-benchmarks
#     build/tests/benchmarks/Release/easyforge-benchmarks [name]
#
# Without a name every benchmark runs; with one, those whose name contains it.

add_executable(easyforge-benchmarks Main.cpp)
target_link_libraries(easyforge-benchmarks PRIVATE easyforge::core)
target_compile_definitions(easyforge-benchmarks PRIVATE
    EASYFORGE_BENCHMARK_FONT="${PROJECT_SOURCE_DIR}/tests/assets/data/fonts/test_regular.ttf")
easyforge_set_warnings(easyforge-benchmarks)

# Each library's benchmarks, where that library is built.
foreach(pair physics:PhysicsBenchmarks data:DataBenchmarks script:ScriptBenchmarks graphics:CanvasBenchmarks
        ui:UiBenchmarks network:NetworkBenchmarks)
    string(REPLACE ":" ";" parts ${pair})
    list(GET parts 0 library)
    list(GET parts 1 file)
    if(TARGET easyforge::${library})
        target_sources(easyforge-benchmarks PRIVATE ${file}.cpp)
        target_link_libraries(easyforge-benchmarks PRIVATE easyforge::${library})
    endif()
endforeach()

# The network benchmarks also time the library's own sealing, which is not public.
if(TARGET easyforge::network)
    target_include_directories(easyforge-benchmarks PRIVATE ${PROJECT_SOURCE_DIR}/src/network)
endif()
