cmake_minimum_required(VERSION 3.18)

get_filename_component(S9X_SRC "${REPO_ROOT}/SNES9x/src" ABSOLUTE)

# -----------------------------------------------------------------------
# SNES9x C++ source files (from SNES9x/src/)
# Excludes: screenshot.cpp (libpng), loadzip.cpp (zlib), netplay.cpp,
#           server.cpp, movie.cpp, debug.cpp, lua-engine (FCEU-only)
# -----------------------------------------------------------------------

set(SNES9X_CORE_SOURCES
    ${S9X_SRC}/bml.cpp
    ${S9X_SRC}/bsx.cpp
    ${S9X_SRC}/c4.cpp
    ${S9X_SRC}/c4emu.cpp
    ${S9X_SRC}/cheats.cpp
    ${S9X_SRC}/cheats2.cpp
    ${S9X_SRC}/clip.cpp
    ${S9X_SRC}/conffile.cpp
    ${S9X_SRC}/controls.cpp
    ${S9X_SRC}/cpu.cpp
    ${S9X_SRC}/cpuexec.cpp
    ${S9X_SRC}/cpuops.cpp
    ${S9X_SRC}/crosshairs.cpp
    ${S9X_SRC}/dma.cpp
    ${S9X_SRC}/dsp.cpp
    ${S9X_SRC}/dsp1.cpp
    ${S9X_SRC}/dsp2.cpp
    ${S9X_SRC}/dsp3.cpp
    ${S9X_SRC}/dsp4.cpp
    ${S9X_SRC}/fxemu.cpp
    ${S9X_SRC}/fxinst.cpp
    ${S9X_SRC}/fscompat.cpp
    ${S9X_SRC}/gfx.cpp
    ${S9X_SRC}/globals.cpp
    ${S9X_SRC}/logger.cpp
    ${S9X_SRC}/memmap.cpp
    ${S9X_SRC}/movie.cpp
    ${S9X_SRC}/msu1.cpp
    ${S9X_SRC}/obc1.cpp
    ${S9X_SRC}/ppu.cpp
    ${S9X_SRC}/sa1.cpp
    ${S9X_SRC}/sa1cpu.cpp
    ${S9X_SRC}/sdd1.cpp
    ${S9X_SRC}/sdd1emu.cpp
    ${S9X_SRC}/seta.cpp
    ${S9X_SRC}/seta010.cpp
    ${S9X_SRC}/seta011.cpp
    ${S9X_SRC}/seta018.cpp
    ${S9X_SRC}/sha256.cpp
    ${S9X_SRC}/snapshot.cpp
    ${S9X_SRC}/snes9x.cpp
    ${S9X_SRC}/spc7110.cpp
    ${S9X_SRC}/srtc.cpp
    ${S9X_SRC}/statemanager.cpp
    ${S9X_SRC}/stream.cpp
    ${S9X_SRC}/tile.cpp
    ${S9X_SRC}/tileimpl-h2x1.cpp
    ${S9X_SRC}/tileimpl-n1x1.cpp
    ${S9X_SRC}/tileimpl-n2x1.cpp
    # Excluded: screenshot.cpp (libpng), loadzip.cpp (zlib),
    #           netplay.cpp, server.cpp, debug.cpp
)

# SNES9x BAPU (Better Audio Processing Unit) sources
set(SNES9X_APU_SOURCES
    ${S9X_SRC}/apu/apu.cpp
    ${S9X_SRC}/apu/bapu/dsp/sdsp.cpp
    ${S9X_SRC}/apu/bapu/smp/smp.cpp
    ${S9X_SRC}/apu/bapu/smp/smp_state.cpp
)

# -----------------------------------------------------------------------
# Shared library
# -----------------------------------------------------------------------
add_library(oe_core_snes9x SHARED
    snes9x_adapter.cpp
    snes9x_driver.cpp
    ${SNES9X_CORE_SOURCES}
    ${SNES9X_APU_SOURCES}
)

target_include_directories(oe_core_snes9x PRIVATE
    ${S9X_SRC}
    ${S9X_SRC}/apu
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(oe_core_snes9x PRIVATE
    # Use RGB565 pixel format (Linux default in port.h)
    PIXEL_FORMAT=RGB565
    # UNZIP_SUPPORT must NOT be defined — stream.h guards with #ifdef
    # so any definition (even =0) enables the unzip.h include
)

# Explicitly undefine UNZIP_SUPPORT in case any header tries to set it
target_compile_options(oe_core_snes9x PRIVATE -UUNZIP_SUPPORT)

target_compile_options(oe_core_snes9x PRIVATE
    -Wno-write-strings
    -Wno-unused-result
)

set_target_properties(oe_core_snes9x PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(oe_core_snes9x PRIVATE m)
