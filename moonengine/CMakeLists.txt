project(moonengine VERSION 1.0.0 LANGUAGES CXX)

file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_LIST_DIR} src/*.cpp src/*.hpp src/*.h)
source_group(TREE ${CMAKE_CURRENT_LIST_DIR}/src PREFIX "Sources" FILES ${SOURCES})

file(GLOB_RECURSE HEADERS RELATIVE ${CMAKE_CURRENT_LIST_DIR} include/*.hpp)
source_group(TREE ${CMAKE_CURRENT_LIST_DIR}/include PREFIX "Headers" FILES ${HEADERS})

SET(MOONSCRIPT_ROOT ${CMAKE_CURRENT_LIST_DIR}/../third-party/moonscript)
file(GLOB_RECURSE MOONSCRIPT_RESOURCES RELATIVE ${CMAKE_CURRENT_LIST_DIR} ${MOONSCRIPT_ROOT}/*.lua)

cmrc_add_resource_library(
    moongengine_resources

    ALIAS moonengine::resources
    NAMESPACE MoonEngine
    WHENCE ${MOONSCRIPT_ROOT}

    ${MOONSCRIPT_RESOURCES}
)

add_library(moonengine STATIC EXCLUDE_FROM_ALL ${SOURCES} ${HEADERS})
target_link_libraries(moonengine PRIVATE
    moonengine::resources
    lua::lib
    lpeg

    sourcesdk::tier1
)

target_include_directories(moonengine PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(moonengine PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include/moonengine)
target_include_directories(moonengine PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
