cmake_minimum_required(VERSION 3.18)

get_filename_component(FCEU_ROOT "${REPO_ROOT}/FCEU" ABSOLUTE)
get_filename_component(FCEU_SRC  "${FCEU_ROOT}/src"  ABSOLUTE)

# -----------------------------------------------------------------------
# Core FCEU source files
# Excludes: lua-engine.cpp, movie.cpp (complex), debug.cpp, netplay.cpp,
#           wave.cpp (WAV recording), profiler.cpp, conddebug.cpp (debugger)
# -----------------------------------------------------------------------
set(FCEU_CORE_SOURCES
    ${FCEU_SRC}/asm.cpp
    ${FCEU_SRC}/cart.cpp
    ${FCEU_SRC}/cheat.cpp
    ${FCEU_SRC}/config.cpp
    ${FCEU_SRC}/conddebug.cpp
    ${FCEU_SRC}/debugsymboltable.cpp
    ${FCEU_SRC}/drawing.cpp
    ${FCEU_SRC}/debug.cpp
    ${FCEU_SRC}/emufile.cpp
    ${FCEU_SRC}/fceu.cpp
    ${FCEU_SRC}/fds.cpp
    ${FCEU_SRC}/file.cpp
    ${FCEU_SRC}/filter.cpp
    ${FCEU_SRC}/ines.cpp
    ${FCEU_SRC}/input.cpp
    ${FCEU_SRC}/movie.cpp
    ${FCEU_SRC}/netplay.cpp
    ${FCEU_SRC}/nsf.cpp
    ${FCEU_SRC}/oldmovie.cpp
    ${FCEU_SRC}/palette.cpp
    ${FCEU_SRC}/ppu.cpp
    ${FCEU_SRC}/sound.cpp
    ${FCEU_SRC}/state.cpp
    ${FCEU_SRC}/unif.cpp
    ${FCEU_SRC}/video.cpp
    ${FCEU_SRC}/vsuni.cpp
    ${FCEU_SRC}/wave.cpp
    ${FCEU_SRC}/x6502.cpp
    # Excluded: ld65dbg.cpp (debugger companion)
    # Excluded: lua-engine.cpp (Lua scripting)
    # Excluded: profiler.cpp (performance profiler)
)

# Input devices
set(FCEU_INPUT_SOURCES
    ${FCEU_SRC}/input/arkanoid.cpp
    ${FCEU_SRC}/input/bworld.cpp
    ${FCEU_SRC}/input/cursor.cpp
    ${FCEU_SRC}/input/fkb.cpp
    ${FCEU_SRC}/input/fns.cpp
    ${FCEU_SRC}/input/ftrainer.cpp
    ${FCEU_SRC}/input/hypershot.cpp
    ${FCEU_SRC}/input/lcdcompzapper.cpp
    ${FCEU_SRC}/input/mahjong.cpp
    ${FCEU_SRC}/input/mouse.cpp
    ${FCEU_SRC}/input/oekakids.cpp
    ${FCEU_SRC}/input/pec586kb.cpp
    ${FCEU_SRC}/input/powerpad.cpp
    ${FCEU_SRC}/input/quiz.cpp
    ${FCEU_SRC}/input/shadow.cpp
    ${FCEU_SRC}/input/snesmouse.cpp
    ${FCEU_SRC}/input/suborkb.cpp
    ${FCEU_SRC}/input/toprider.cpp
    ${FCEU_SRC}/input/virtualboy.cpp
    ${FCEU_SRC}/input/zapper.cpp
)

# Mapper/board files — use CMake GLOB since there are 171 of them
file(GLOB FCEU_BOARD_SOURCES "${FCEU_SRC}/boards/*.cpp")
list(APPEND FCEU_BOARD_SOURCES ${FCEU_SRC}/boards/emu2413.c)

set(FCEU_UTIL_SOURCES
    ${FCEU_SRC}/utils/endian.cpp
    ${FCEU_SRC}/utils/general.cpp
    ${FCEU_SRC}/utils/guid.cpp
    ${FCEU_SRC}/utils/md5.cpp
    ${FCEU_SRC}/utils/memory.cpp
    ${FCEU_SRC}/utils/mutex.cpp
    ${FCEU_SRC}/utils/timeStamp.cpp
    ${FCEU_SRC}/utils/ioapi.cpp
    ${FCEU_SRC}/utils/unzip.cpp
    ${FCEU_SRC}/utils/xstring.cpp
    ${FCEU_SRC}/utils/crc32.cpp
)

# Common driver utilities (not Qt-specific)
set(FCEU_COMMON_SOURCES
    ${FCEU_SRC}/drivers/common/args.cpp
    ${FCEU_SRC}/drivers/common/config.cpp
    ${FCEU_SRC}/drivers/common/configSys.cpp
    ${FCEU_SRC}/drivers/common/nes_ntsc.c
    ${FCEU_SRC}/drivers/common/os_utils.cpp
    ${FCEU_SRC}/drivers/common/vidblit.cpp
)

# -----------------------------------------------------------------------
# Shared library
# -----------------------------------------------------------------------
add_library(oe_core_fceu SHARED
    fceu_adapter.cpp
    fceu_driver.cpp
    ${FCEU_CORE_SOURCES}
    ${FCEU_INPUT_SOURCES}
    ${FCEU_BOARD_SOURCES}
    ${FCEU_UTIL_SOURCES}
    ${FCEU_COMMON_SOURCES}
)

target_include_directories(oe_core_fceu PRIVATE
    ${FCEU_ROOT}
    ${FCEU_SRC}
    ${FCEU_SRC}/drivers
    ${FCEU_SRC}/drivers/common
    ${OE_ZLIB_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(oe_core_fceu PRIVATE
    # PSS_STYLE=1 → forward-slash path separator (Linux/POSIX)
    PSS_STYLE=1
    # Tell FCEUX this is an SDL-style driver
    __SDL__
)

target_compile_options(oe_core_fceu PRIVATE
    -Wno-write-strings
    -Wno-unused-result
    -Wno-narrowing
    # FCEUX uses some deprecated C-style casts
    -fpermissive
)

set_target_properties(oe_core_fceu PROPERTIES POSITION_INDEPENDENT_CODE ON)

if (TARGET oe_zlib)
    target_link_libraries(oe_core_fceu PRIVATE m oe_zlib)
else()
    target_link_libraries(oe_core_fceu PRIVATE m z)
endif()
