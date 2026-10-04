# Compiles every public header of the built libraries on its own, so each one
# includes everything it needs. On Windows each header is also compiled after
# windows.h and windowsx.h, without NOMINMAX or WIN32_LEAN_AND_MEAN, the way
# many programs include them; that catches names their macros would break.

get_property(built_libraries GLOBAL PROPERTY EASYFORGE_BUILT_LIBRARIES)

set(checks "")
foreach(library IN LISTS built_libraries)
    file(GLOB_RECURSE headers
        RELATIVE "${PROJECT_SOURCE_DIR}/include"
        CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/include/easyforge/${library}.h"
        "${PROJECT_SOURCE_DIR}/include/easyforge/${library}/*.h")
    set(headers_${library} ${headers})
endforeach()

# The header with everything, and the bridges, where every library is built.
set(every_library core assets window input graphics data ui script sound physics network)
set(extra_headers "")
set(all_built TRUE)
foreach(library IN LISTS every_library)
    if(NOT TARGET easyforge::${library})
        set(all_built FALSE)
    endif()
endforeach()
if(all_built)
    file(GLOB bridge_headers RELATIVE "${PROJECT_SOURCE_DIR}/include" CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/include/easyforge/bridges/*.h")
    set(extra_headers easyforge/easyforge.h ${bridge_headers})
endif()

foreach(library IN LISTS built_libraries ITEMS extra)
    if(library STREQUAL "extra")
        set(headers ${extra_headers})
    else()
        set(headers ${headers_${library}})
    endif()

    foreach(header IN LISTS headers)
        string(MAKE_C_IDENTIFIER "${header}" name)

        set(alone "${CMAKE_CURRENT_BINARY_DIR}/alone_${name}.cpp")
        file(CONFIGURE OUTPUT "${alone}" CONTENT "#include <${header}>\n" @ONLY)
        list(APPEND checks "${alone}")

        if(WIN32)
            set(after_windows "${CMAKE_CURRENT_BINARY_DIR}/after_windows_${name}.cpp")
            file(CONFIGURE OUTPUT "${after_windows}"
                CONTENT "#include <windows.h>\n#include <windowsx.h>\n#include <${header}>\n" @ONLY)
            list(APPEND checks "${after_windows}")
        endif()
    endforeach()
endforeach()

add_library(easyforge-header-checks OBJECT ${checks})
target_link_libraries(easyforge-header-checks PRIVATE easyforge::easyforge)
easyforge_set_warnings(easyforge-header-checks)
