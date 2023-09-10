configure_file(config.hpp.in ${CMAKE_BINARY_DIR}/include/config.hpp)

# Include every source file in source directory
file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_LIST_DIR} *.cpp *.hpp)
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} PREFIX "Sources" FILES ${SOURCES})

add_library(moonloader SHARED
    # Minimal set of files for clientside
    main.cpp 
    global.hpp 
    utils.hpp
    core.cpp core.hpp
    lua_api.cpp lua_api.hpp
)
if(NOT ${CLIENT_DLL})
    target_sources(moonloader PRIVATE ${SOURCES})

    target_link_libraries(moonloader PRIVATE
        gmod::common
        gmod::detouring
        gmod::helpers_extended
        sourcesdk::common
        sourcesdk::tier0
        sourcesdk::tier1

        efsw
        moonengine
    )

    if(APPLE)
        target_link_libraries(moonloader PRIVATE sourcesdk::lzma)
    endif()
else()
    target_link_libraries(moonloader PRIVATE
        gmod::common
        moonengine
    )
endif()

target_include_directories(moonloader PRIVATE 
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_BINARY_DIR}/include
)

set_gmod_suffix_prefix(moonloader)

# Autoinstall
set(AUTOINSTALL "" CACHE PATH "Autoinstall path")
if(IS_DIRECTORY ${AUTOINSTALL})
    autoinstall(moonloader ${AUTOINSTALL})
endif()