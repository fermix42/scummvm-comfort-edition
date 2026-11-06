cmake_minimum_required(VERSION 3.16)
project(ce_native_regressions LANGUAGES CXX)
enable_testing()
add_executable(sq5_battle_cheats test_sq5_battle_cheats.cpp)
target_include_directories(sq5_battle_cheats PRIVATE ../.. ../../engines)
target_compile_definitions(sq5_battle_cheats PRIVATE WIN32)
# Assertions are the checks in this existing regression executable.
target_compile_options(sq5_battle_cheats PRIVATE /UNDEBUG)
add_test(NAME sq5_battle_cheats COMMAND sq5_battle_cheats)
