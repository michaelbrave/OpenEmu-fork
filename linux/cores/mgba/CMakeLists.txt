cmake_minimum_required(VERSION 3.18)

get_filename_component(MGBA_ROOT "${REPO_ROOT}/mGBA" ABSOLUTE)
get_filename_component(MGBA_SRC  "${MGBA_ROOT}/src"  ABSOLUTE)

# -----------------------------------------------------------------------
# Generated headers — flags.h and version.c live in this directory
# -----------------------------------------------------------------------
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/flags.h"
    "${CMAKE_CURRENT_BINARY_DIR}/mgba/flags.h"
    COPYONLY
)

# -----------------------------------------------------------------------
# Source files — mirrors what the upstream CMakeLists would select for a
# minimal GBA-only, software-renderer, no-threading build.
# -----------------------------------------------------------------------

set(MGBA_ARM_SOURCES
    ${MGBA_SRC}/arm/arm.c
    ${MGBA_SRC}/arm/decoder-arm.c
    ${MGBA_SRC}/arm/decoder.c
    ${MGBA_SRC}/arm/decoder-thumb.c
    ${MGBA_SRC}/arm/isa-arm.c
    ${MGBA_SRC}/arm/isa-thumb.c
)

set(MGBA_CORE_SOURCES
    ${MGBA_SRC}/core/bitmap-cache.c
    ${MGBA_SRC}/core/cache-set.c
    ${MGBA_SRC}/core/cheats.c
    ${MGBA_SRC}/core/config.c
    ${MGBA_SRC}/core/core.c
    ${MGBA_SRC}/core/directories.c
    ${MGBA_SRC}/core/input.c
    ${MGBA_SRC}/core/interface.c
    ${MGBA_SRC}/core/log.c
    ${MGBA_SRC}/core/map-cache.c
    ${MGBA_SRC}/core/mem-search.c
    ${MGBA_SRC}/core/rewind.c
    ${MGBA_SRC}/core/serialize.c
    ${MGBA_SRC}/core/sync.c
    ${MGBA_SRC}/core/tile-cache.c
    ${MGBA_SRC}/core/timing.c
    # thread.c requires pthreads and is guarded by DISABLE_THREADING
    # library.c requires SQLite; skip
    # lockstep.c is only for link cable; skip for now
)

set(MGBA_GBA_SOURCES
    ${MGBA_SRC}/gba/audio.c
    ${MGBA_SRC}/gba/bios.c
    ${MGBA_SRC}/gba/cart/gpio.c
    ${MGBA_SRC}/gba/cart/ereader.c
    ${MGBA_SRC}/gba/cart/matrix.c
    ${MGBA_SRC}/gba/cart/vfame.c
    ${MGBA_SRC}/gba/cheats.c
    ${MGBA_SRC}/gba/cheats/codebreaker.c
    ${MGBA_SRC}/gba/cheats/gameshark.c
    ${MGBA_SRC}/gba/cheats/parv3.c
    ${MGBA_SRC}/gba/core.c
    ${MGBA_SRC}/gba/dma.c
    ${MGBA_SRC}/gba/gba.c
    ${MGBA_SRC}/gba/hle-bios.c
    ${MGBA_SRC}/gba/input.c
    ${MGBA_SRC}/gba/io.c
    ${MGBA_SRC}/gba/memory.c
    ${MGBA_SRC}/gba/overrides.c
    ${MGBA_SRC}/gba/renderers/cache-set.c
    ${MGBA_SRC}/gba/renderers/common.c
    ${MGBA_SRC}/gba/renderers/software-bg.c
    ${MGBA_SRC}/gba/renderers/software-mode0.c
    ${MGBA_SRC}/gba/renderers/software-obj.c
    ${MGBA_SRC}/gba/renderers/video-software.c
    ${MGBA_SRC}/gba/savedata.c
    ${MGBA_SRC}/gba/serialize.c
    ${MGBA_SRC}/gba/sio.c
    ${MGBA_SRC}/gba/sio/dolphin.c
    ${MGBA_SRC}/gba/sio/gbp.c
    ${MGBA_SRC}/gba/sio/joybus.c
    ${MGBA_SRC}/gba/timer.c
    ${MGBA_SRC}/gba/video.c
    # gba/renderers/gl.c — requires OpenGL, excluded in Phase 0
)

# GB audio module is referenced by gba/audio.c
set(MGBA_GB_SOURCES
    ${MGBA_SRC}/gb/audio.c
)

set(MGBA_UTIL_SOURCES
    ${MGBA_SRC}/util/circle-buffer.c
    ${MGBA_SRC}/util/configuration.c
    ${MGBA_SRC}/util/crc32.c
    ${MGBA_SRC}/util/formatting.c
    ${MGBA_SRC}/util/gbk-table.c
    ${MGBA_SRC}/util/hash.c
    ${MGBA_SRC}/util/memory.c
    ${MGBA_SRC}/util/patch.c
    ${MGBA_SRC}/util/patch-ips.c
    ${MGBA_SRC}/util/patch-ups.c
    ${MGBA_SRC}/util/ring-fifo.c
    ${MGBA_SRC}/util/string.c
    ${MGBA_SRC}/util/table.c
    ${MGBA_SRC}/util/vfs.c
    ${MGBA_SRC}/util/vfs/vfs-fd.c
    ${MGBA_SRC}/util/vfs/vfs-file.c
    ${MGBA_SRC}/util/vfs/vfs-fifo.c
    ${MGBA_SRC}/util/vfs/vfs-mem.c
    ${MGBA_SRC}/util/vfs/vfs-dirent.c
    # png-io.c requires libpng; skip for Phase 0
)

set(MGBA_BLIP_SOURCES
    ${MGBA_SRC}/third-party/blip_buf/blip_buf.c
)

# inih — used by configuration.c
set(MGBA_INIH_SOURCES
    ${MGBA_SRC}/third-party/inih/ini.c
)

# -----------------------------------------------------------------------
# Shared library
# -----------------------------------------------------------------------
add_library(oe_core_mgba SHARED
    mgba_adapter.c
    version.c
    ${MGBA_ARM_SOURCES}
    ${MGBA_CORE_SOURCES}
    ${MGBA_GBA_SOURCES}
    ${MGBA_GB_SOURCES}
    ${MGBA_UTIL_SOURCES}
    ${MGBA_BLIP_SOURCES}
    ${MGBA_INIH_SOURCES}
)

target_include_directories(oe_core_mgba PRIVATE
    # Our generated flags.h lives here
    ${CMAKE_CURRENT_BINARY_DIR}
    # mGBA internal headers are included as "gba/...", "util/...", etc.
    ${MGBA_SRC}
    # mGBA public headers
    ${MGBA_ROOT}/include
    # Our oe_core_interface.h
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(oe_core_mgba PRIVATE
    M_CORE_GBA
    DISABLE_THREADING
    # Prevent mGBA's formatting.h from redefining locale_t which conflicts
    # with the system <locale.h> definition on Linux glibc.
    # formatting.h guards the typedef with: #elif !defined(HAVE_LOCALE)
    HAVE_LOCALE
    # Disable optional features that pull in external deps
    # (no USE_PNG, no USE_SQLITE3, no BUILD_GL)
)

# Position-independent code required for shared libs
set_target_properties(oe_core_mgba PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Link math library (used by audio resampler)
target_link_libraries(oe_core_mgba PRIVATE m)
