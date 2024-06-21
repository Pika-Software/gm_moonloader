cmake_minimum_required(VERSION 3.22)
enable_language(CXX C)

# Require C++ 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable -fPIC flag
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Enable IDE folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Force x86_64 architecture on MacOSX
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64")

    # Disable deprecation warnings
    add_compile_options(
        -Wno-deprecated-declarations
    )
endif()

# Debug build is unsupported on MSVC
if(MSVC)
    list(REMOVE_ITEM CMAKE_CONFIGURATION_TYPES "Debug")
endif()

# Include extensions
add_subdirectory(cmake)

# Entropia File System Watcher
add_subdirectory(third-party/efsw)

# Include embedded lua
# add_subdirectory(third-party/lua)
add_subdirectory(third-party/lpeg)

# Add moonengine library
add_subdirectory(moonengine)

# Include garrysmod_common
find_garrysmod_common()
if(NOT GARRYSMOD_COMMON_FOUND) # Check if garrysmod_common has been found
    message(FATAL_ERROR "garrysmod_common not found")
endif()

# Get the latest abbreviated commit hash of the working branch
execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get the current working branch
if(NOT DEFINED GIT_BRANCH)
    execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

file(STRINGS "VERSION" GM_MOONLOADER_VERSION)
project(gm_moonloader 
    VERSION ${GM_MOONLOADER_VERSION}
    LANGUAGES CXX 
    HOMEPAGE_URL "https://github.com/Pika-Software/gm_moonloader"
)

add_subdirectory(source)
