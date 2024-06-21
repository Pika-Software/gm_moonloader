cmake_minimum_required(VERSION 3.20)
project(yue CXX)

set(YUESCRIPT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../third-party/Yuescript)
cmake_path(NORMAL_PATH YUESCRIPT_ROOT)
set(YUESCRIPT_SRC ${YUESCRIPT_ROOT}/src)

add_library(libyue
	"${YUESCRIPT_SRC}/yuescript/ast.cpp"
	"${YUESCRIPT_SRC}/yuescript/parser.cpp"
	"${YUESCRIPT_SRC}/yuescript/yue_ast.cpp"
	"${YUESCRIPT_SRC}/yuescript/yue_parser.cpp"
	"${YUESCRIPT_SRC}/yuescript/yue_compiler.cpp"
	"${YUESCRIPT_SRC}/yuescript/yuescript.cpp"
)

target_link_libraries(libyue PRIVATE lua::lib)

target_include_directories(libyue PUBLIC ${YUESCRIPT_SRC})
target_compile_definitions(libyue PRIVATE)
