/*
 * oe_core_interface.h — Pure C emulator core interface for OpenEMU Linux port.
 *
 * Phase 0: Minimal interface needed to run a ROM headlessly and get video frames.
 * Phase 1 will extend this into the full OECoreInterface function table.
 *
 * Video pixel formats vary by core:
 *   mGBA  : RGBA8888 (4 bytes/pixel, R at lowest address)
 *   SNES9x: RGB565   (2 bytes/pixel)
 *   FCEU  : Indexed8 (1 byte/pixel) — caller must apply palette via oe_fceu_get_palette()
 */

#ifndef OE_CORE_INTERFACE_H
#define OE_CORE_INTERFACE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque core handle — each core allocates its own state struct */
typedef struct OECoreState OECoreState;

/* Pixel format tag returned by oe_core_pixel_format() */
typedef enum {
    OE_PIXEL_RGBA8888  = 0,  /* 4 bytes/pixel: R G B A */
    OE_PIXEL_RGB565    = 1,  /* 2 bytes/pixel: 5R 6G 5B */
    OE_PIXEL_INDEXED8  = 2,  /* 1 byte/pixel, palette via oe_fceu_get_palette() */
} OEPixelFormat;

/*
 * Each core .so exports these symbols. Callers use them directly (no dlsym
 * gymnastics needed when statically linking in tests; use dlsym for runtime).
 */

/* Allocate and initialise core state. Returns NULL on failure. */
OECoreState *oe_core_create(void);

/* Load ROM from file path. Returns 0 on success, non-zero on error. */
int oe_core_load_rom(OECoreState *state, const char *path);

/* Run one video frame. Video buffer is updated after this call. */
void oe_core_run_frame(OECoreState *state);

/* Return pointer to the video pixel buffer (valid until next oe_core_run_frame). */
const void *oe_core_video_buffer(OECoreState *state);

/* Return video frame dimensions. */
void oe_core_video_size(OECoreState *state, int *width, int *height);

/* Return the pixel format for this core's video buffer. */
OEPixelFormat oe_core_pixel_format(OECoreState *state);

/* Reset emulation to power-on state. */
void oe_core_reset(OECoreState *state);

/* Destroy core state and free all resources. */
void oe_core_destroy(OECoreState *state);

#ifdef __cplusplus
}
#endif

#endif /* OE_CORE_INTERFACE_H */
