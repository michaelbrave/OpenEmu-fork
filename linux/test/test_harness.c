/*
 * test_harness.c — OpenEMU Linux Port, Phase 0
 *
 * Minimal headless test: init → loadROM → runFrame × N → write frame.ppm
 *
 * Usage:
 *   ./test_harness <core> <rom_path> [num_frames]
 *
 *   <core>       : "mgba", "snes9x", or "fceu"
 *   <rom_path>   : path to a ROM file
 *   [num_frames] : number of frames to run before writing the PPM (default 60)
 *
 * Output:
 *   frame_<core>_<N>.ppm  — NetPBM P6 RGB image of the last rendered frame
 *
 * Pixel format handling:
 *   RGBA8888 (mGBA, FCEU) : R,G,B extracted directly from bytes 0,1,2
 *   RGB565   (SNES9x)     : converted to R8G8B8 for PPM output
 */

#include "oe_core_interface.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Function pointer types matching oe_core_interface.h
 * ----------------------------------------------------------------------- */
typedef OECoreState *  (*fn_create)(void);
typedef int            (*fn_load_rom)(OECoreState *, const char *);
typedef void           (*fn_run_frame)(OECoreState *);
typedef const void *   (*fn_video_buffer)(OECoreState *);
typedef void           (*fn_video_size)(OECoreState *, int *, int *);
typedef OEPixelFormat  (*fn_pixel_format)(OECoreState *);
typedef void           (*fn_reset)(OECoreState *);
typedef void           (*fn_destroy)(OECoreState *);

typedef struct {
    void           *handle;
    fn_create       create;
    fn_load_rom     load_rom;
    fn_run_frame    run_frame;
    fn_video_buffer video_buffer;
    fn_video_size   video_size;
    fn_pixel_format pixel_format;
    fn_reset        reset;
    fn_destroy      destroy;
} CoreLib;

#define LOAD_SYM(lib, sym) do {                                     \
    lib.sym = (fn_##sym)dlsym(lib.handle, "oe_core_" #sym);         \
    if (!lib.sym) {                                                 \
        fprintf(stderr, "dlsym oe_core_%s: %s\n", #sym, dlerror()); \
        return 1;                                                   \
    }                                                               \
} while (0)

/* -----------------------------------------------------------------------
 * PPM writer
 * ----------------------------------------------------------------------- */
static void write_ppm_rgb565(const char *path, const uint16_t *buf,
                              int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        uint16_t px = buf[i];
        uint8_t r = (uint8_t)(((px >> 11) & 0x1F) * 255 / 31);
        uint8_t g = (uint8_t)(((px >>  5) & 0x3F) * 255 / 63);
        uint8_t b = (uint8_t)(((px >>  0) & 0x1F) * 255 / 31);
        fwrite(&r, 1, 1, f);
        fwrite(&g, 1, 1, f);
        fwrite(&b, 1, 1, f);
    }
    fclose(f);
}

static void write_ppm_rgba8888(const char *path, const uint32_t *buf,
                                int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        uint32_t px = buf[i];
        uint8_t r = (px >> 24) & 0xFF;
        uint8_t g = (px >> 16) & 0xFF;
        uint8_t b = (px >>  8) & 0xFF;
        fwrite(&r, 1, 1, f);
        fwrite(&g, 1, 1, f);
        fwrite(&b, 1, 1, f);
    }
    fclose(f);
}

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <core> <rom_path> [num_frames]\n", argv[0]);
        fprintf(stderr, "  core: mgba | snes9x | fceu\n");
        return 1;
    }

    const char *core_name  = argv[1];
    const char *rom_path   = argv[2];
    int         num_frames = (argc >= 4) ? atoi(argv[3]) : 60;

    /* Resolve .so path from the binary directory */
    char lib_path[512];
    snprintf(lib_path, sizeof(lib_path), "%s/liboe_core_%s.so",
             /* same directory as this binary */ ".", core_name);

    CoreLib lib;
    memset(&lib, 0, sizeof(lib));

    lib.handle = dlopen(lib_path, RTLD_LAZY);
    if (!lib.handle) {
        fprintf(stderr, "dlopen %s: %s\n", lib_path, dlerror());
        return 1;
    }

    LOAD_SYM(lib, create);
    LOAD_SYM(lib, load_rom);
    LOAD_SYM(lib, run_frame);
    LOAD_SYM(lib, video_buffer);
    LOAD_SYM(lib, video_size);
    LOAD_SYM(lib, pixel_format);
    LOAD_SYM(lib, reset);
    LOAD_SYM(lib, destroy);

    printf("[test] Creating core '%s'...\n", core_name);
    OECoreState *state = lib.create();
    if (!state) {
        fprintf(stderr, "oe_core_create() returned NULL\n");
        return 1;
    }

    printf("[test] Loading ROM: %s\n", rom_path);
    if (lib.load_rom(state, rom_path) != 0) {
        fprintf(stderr, "oe_core_load_rom() failed\n");
        lib.destroy(state);
        return 1;
    }

    printf("[test] Running %d frames...\n", num_frames);
    for (int i = 0; i < num_frames; i++) {
        lib.run_frame(state);
        if ((i + 1) % 10 == 0)
            printf("[test]   frame %d / %d\n", i + 1, num_frames);
    }

    int w = 0, h = 0;
    lib.video_size(state, &w, &h);
    OEPixelFormat fmt = lib.pixel_format(state);
    const void *buf = lib.video_buffer(state);

    printf("[test] Frame dimensions: %d × %d, format: %s\n",
           w, h,
           fmt == OE_PIXEL_RGBA8888 ? "RGBA8888" :
           fmt == OE_PIXEL_RGB565   ? "RGB565"   : "INDEXED8");

    char out_path[256];
    snprintf(out_path, sizeof(out_path), "frame_%s_%d.ppm", core_name, num_frames);

    if (fmt == OE_PIXEL_RGB565) {
        write_ppm_rgb565(out_path, (const uint16_t *)buf, w, h);
    } else if (fmt == OE_PIXEL_RGBA8888) {
        write_ppm_rgba8888(out_path, (const uint32_t *)buf, w, h);
    } else {
        fprintf(stderr, "[test] Unsupported pixel format for PPM output\n");
    }

    printf("[test] Wrote %s\n", out_path);

    lib.destroy(state);
    /* Keep the core library loaded until process exit to avoid shutdown-time
     * crashes in partially wired Phase 0 backends. */
    return 0;
}
