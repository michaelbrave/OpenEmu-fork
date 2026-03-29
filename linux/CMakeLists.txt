cmake_minimum_required(VERSION 3.18)
project(OpenEMU_Linux C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

# Output shared libs and executables to build/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Repo root (one level above this file)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(cores/mgba)
add_subdirectory(cores/snes9x)

# FCEU depends on zlib headers/libs. Prefer system zlib, but fall back to
# vendored zlib under linux/third_party.
set(OE_BUILD_FCEU OFF)
set(OE_ZLIB_INCLUDE_DIR "")
if (EXISTS "/usr/include/zlib.h")
    set(OE_BUILD_FCEU ON)
    set(OE_ZLIB_INCLUDE_DIR "/usr/include")
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/zlib.h")
    set(OE_BUILD_FCEU ON)
    set(OE_ZLIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1")

    add_library(oe_zlib STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/adler32.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/compress.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/crc32.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/deflate.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/gzclose.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/gzlib.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/gzread.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/gzwrite.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/inflate.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/infback.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/inftrees.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/inffast.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/trees.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/uncompr.c
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib-1.3.1/zutil.c
    )
    target_include_directories(oe_zlib PUBLIC "${OE_ZLIB_INCLUDE_DIR}")
    set_target_properties(oe_zlib PROPERTIES POSITION_INDEPENDENT_CODE ON)
else()
    message(WARNING "Skipping FCEU: zlib headers not found (system or vendored)")
endif()

if (OE_BUILD_FCEU)
    add_subdirectory(cores/fceu)
endif()

add_subdirectory(test)
