cmake_minimum_required(VERSION 3.20)
project(lua51 C)

set(LUA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../third-party/lua5.1)
cmake_path(NORMAL_PATH YUESCRIPT_ROOT)

set(LUA_SRC lapi.c lcode.c ldebug.c ldo.c ldump.c lfunc.c 
	lgc.c llex.c lmem.c loadlib.c lobject.c lopcodes.c lparser.c lstate.c 
	lstring.c ltable.c ltm.c lundump.c lvm.c lzio.c lauxlib.c 
	lbaselib.c ldblib.c liolib.c lmathlib.c loslib.c ltablib.c 
	lstrlib.c linit.c)

set(LUA_PUBLIC_HEADERS lauxlib.h lua.h luaconf.h lualib.h)

list(TRANSFORM LUA_SRC PREPEND ${LUA_ROOT}/)
list(TRANSFORM LUA_PUBLIC_HEADERS PREPEND ${LUA_ROOT}/)

add_library(lua51 ${LUA_SRC})

target_compile_definitions(lua51
    PRIVATE
    $<$<PLATFORM_ID:Linux>:LUA_USE_LINUX LUA_COMPAT_5_2>)

target_compile_options(lua51
    PRIVATE
    $<$<OR:$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:Clang>,$<C_COMPILER_ID:GNU>>:
    -Wextra -Wshadow -Wsign-compare -Wundef -Wwrite-strings -Wredundant-decls
    -Wdisabled-optimization -Waggregate-return -Wdouble-promotion -Wdeclaration-after-statement
    -Wmissing-prototypes -Wnested-externs -Wstrict-prototypes -Wc++-compat -Wold-style-definition>)

set(LUA_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
file(COPY ${LUA_PUBLIC_HEADERS} DESTINATION ${LUA_INCLUDE_DIR})

target_include_directories(lua51 PRIVATE ${LUA_ROOT})
target_include_directories(lua51 INTERFACE ${LUA_INCLUDE_DIR})

add_library(lua::lib ALIAS lua51)
