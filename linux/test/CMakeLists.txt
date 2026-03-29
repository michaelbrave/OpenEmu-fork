cmake_minimum_required(VERSION 3.18)

add_executable(test_harness test_harness.c)

target_include_directories(test_harness PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Link dl for dlopen/dlsym
target_link_libraries(test_harness PRIVATE dl)

# Ensure test_harness is built after the core libraries
add_dependencies(test_harness oe_core_mgba oe_core_snes9x)
if (TARGET oe_core_fceu)
    add_dependencies(test_harness oe_core_fceu)
endif()
