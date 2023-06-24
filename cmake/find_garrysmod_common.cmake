function(find_garrysmod_common)
    message(STATUS "Looking for garrysmod_common...")
    set(GARRYSMOD_COMMON_PATH "GARRYSMOD_COMMON_NOT_FOUND" CACHE PATH "Path to garrysmod_common (https://github.com/dankmolot/garrysmod_common/tree/master-cmake)")
    cmake_path(ABSOLUTE_PATH GARRYSMOD_COMMON_PATH NORMALIZE)

    if(NOT IS_DIRECTORY ${GARRYSMOD_COMMON_PATH} OR NOT EXISTS ${GARRYSMOD_COMMON_PATH}/CMakeLists.txt OR ${GARRYSMOD_COMMON_PATH} STREQUAL ${CMAKE_CURRENT_LIST_DIR})
        message(FATAL_ERROR "Invalid path to garrysmod_common. Please set valid GARRYSMOD_COMMON_PATH")
    endif()

    add_subdirectory(${GARRYSMOD_COMMON_PATH} ${CMAKE_BINARY_DIR}/garrysmod_common)
endfunction()
