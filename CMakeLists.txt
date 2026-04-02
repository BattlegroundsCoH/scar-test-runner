cmake_minimum_required(VERSION 3.10)
project(scar-test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(vendor/lua)

set(SCAR_TEST_SOURCES
    src/main.c
    src/game_state.c
    src/scar_state.c
    src/test_runner.c
    src/test_api.c
    src/mock_entity.c
    src/mock_squad.c
    src/mock_player.c
    src/mock_egroup.c
    src/mock_sgroup.c
    src/mock_game.c
    src/mock_misc.c
    src/mock_core.c
    src/mock_blueprint.c
    src/mock_territory.c
    src/mock_ui.c
)

add_executable(scar-test ${SCAR_TEST_SOURCES})
target_link_libraries(scar-test PRIVATE lua53)
target_include_directories(scar-test PRIVATE src vendor/hoxml)

if(MSVC)
    target_compile_options(scar-test PRIVATE /W3)
    target_compile_definitions(scar-test PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(scar-test PRIVATE -Wall -Wextra -pedantic)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(scar-test PRIVATE m dl)
endif()
