cmake_minimum_required(VERSION 3.10)
project(lua53 C)

file(GLOB LUA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
# Remove lua.c (standalone interpreter) and luac.c (compiler) to avoid duplicate main()
list(REMOVE_ITEM LUA_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/lua.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/luac.c"
)

add_library(lua53 STATIC ${LUA_SOURCES})
target_include_directories(lua53 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(UNIX)
    target_compile_definitions(lua53 PRIVATE LUA_USE_POSIX)
    target_link_libraries(lua53 PRIVATE m dl)
endif()
