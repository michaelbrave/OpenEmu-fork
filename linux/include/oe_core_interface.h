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
 * OECoreInterface — Function table for interacting with an emulator core.
 * Each core .so must export a single symbol:
 *   const OECoreInterface *oe_get_core_interface(void);
 */
typedef struct {
    /* Lifecycle */
    OECoreState * (*create)(void);
    void          (*destroy)(OECoreState *state);

    /* ROM Loading */
    int           (*load_rom)(OECoreState *state, const char *path);

    /* Emulation */
    void          (*run_frame)(OECoreState *state);
    void          (*reset)(OECoreState *state);

    /* Persistence */
    int           (*save_state)(OECoreState *state, const char *path);
    int           (*load_state)(OECoreState *state, const char *path);

    /* Input */
    void          (*set_button)(OECoreState *state, int player, int button, int pressed);
    void          (*set_axis)(OECoreState *state, int player, int axis, int value);

    /* Video Metadata */
    void          (*get_video_size)(OECoreState *state, int *width, int *height);
    OEPixelFormat (*get_pixel_format)(OECoreState *state);
    const void *  (*get_video_buffer)(OECoreState *state);

    /* Audio */
    int           (*get_audio_sample_rate)(OECoreState *state);
    /* Returns number of samples written to buffer. Samples are interleaved S16. */
    size_t        (*read_audio)(OECoreState *state, int16_t *buffer, size_t max_samples);

} OECoreInterface;

/* The single entry point exported by the core .so */
const OECoreInterface *oe_get_core_interface(void);

/*
 * Legacy Phase 0 exports (for test_harness compatibility).
 * These will be removed once the Swift bridge is fully operational.
 */
OECoreState *oe_core_create(void);
int oe_core_load_rom(OECoreState *state, const char *path);
void oe_core_run_frame(OECoreState *state);
const void *oe_core_video_buffer(OECoreState *state);
void oe_core_video_size(OECoreState *state, int *width, int *height);
OEPixelFormat oe_core_pixel_format(OECoreState *state);
void oe_core_reset(OECoreState *state);
void oe_core_destroy(OECoreState *state);

#ifdef __cplusplus
}
#endif

#endif /* OE_CORE_INTERFACE_H */
