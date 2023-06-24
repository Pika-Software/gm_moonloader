configure_file(config.hpp.in ${CMAKE_BINARY_DIR}/include/config.hpp)

# Include every source file in source directory
file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_LIST_DIR} *.cpp *.hpp)
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} PREFIX "Sources" FILES ${SOURCES})

add_library(moonloader SHARED ${SOURCES})
target_link_libraries(moonloader PRIVATE 
    gmod::common
    gmod::detouring
    gmod::helpers_extended
    sourcesdk::common
    sourcesdk::tier0
    sourcesdk::tier1
    # sourcesdk::steam_api

    efsw
    moonengine
)

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