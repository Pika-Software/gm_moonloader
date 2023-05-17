project(lpeg LANGUAGES C)

add_library(lpeg STATIC EXCLUDE_FROM_ALL lpcap.c lpcode.c lpprint.c lptree.c lpvm.c)
target_include_directories(lpeg PRIVATE .)
target_include_directories(lpeg PUBLIC include)
target_link_libraries(lpeg PUBLIC lua::lib)