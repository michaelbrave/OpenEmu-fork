/*
 * fceu_adapter.cpp — OpenEMU Linux port, Phase 0
 *
 * Implements oe_core_interface.h for the FCEUX NES/Famicom emulator.
 * Calls FCEUI_* API directly, bypassing the macOS ObjC wrapper.
 *
 * Video: FCEUX outputs indexed-8 (256×240) via XBuf extern.
 * This adapter converts to RGBA8888 using the palette table maintained
 * by FCEUD_SetPalette (in fceu_driver.cpp).
 */

#include "oe_core_interface.h"

#include "src/driver.h"
#include "src/fceu.h"
#include "src/palette.h"
#include "src/input.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* FCEUX exposes the raw indexed video buffer via this extern */
extern uint8 *XBuf;

/* Palette maintained by fceu_driver.cpp */
extern uint32_t *fceu_get_palette(void);

#define NES_WIDTH  256
#define NES_HEIGHT 240

struct OECoreState {
    uint32_t *rgba_buffer;   /* NES_WIDTH × NES_HEIGHT RGBA8888 */
    int32_t  *sound_buffer;  /* audio scratch (discarded in Phase 0) */
    int32_t   sound_size;
    uint32_t  joypad;        /* input state for port 0 (bitmask) */
};

OECoreState *oe_core_create(void)
{
    OECoreState *state = (OECoreState *)calloc(1, sizeof(OECoreState));
    if (!state) return NULL;

    state->rgba_buffer  = (uint32_t *)malloc(NES_WIDTH * NES_HEIGHT * 4);
    state->sound_buffer = (int32_t  *)malloc(48000 / 50 * sizeof(int32_t) * 2);

    if (!state->rgba_buffer || !state->sound_buffer) {
        free(state->rgba_buffer);
        free(state->sound_buffer);
        free(state);
        return NULL;
    }

    memset(state->rgba_buffer, 0, NES_WIDTH * NES_HEIGHT * 4);
    return state;
}

int oe_core_load_rom(OECoreState *state, const char *path)
{
    FCEUI_Initialize();

    /* Disable sound for Phase 0 headless */
    FCEUI_Sound(0);

    /* Set base directory for saves in /tmp */
    FCEUI_SetBaseDirectory("/tmp/openemu_fceu");

    FCEUGI *game_info = FCEUI_LoadGame(path, 1, true);
    if (!game_info) {
        fprintf(stderr, "[fceu] Failed to load ROM: %s\n", path);
        return -1;
    }

    /* Configure two standard gamepads */
    FCEUI_SetInput(0, SI_GAMEPAD, &state->joypad, 0);
    FCEUI_SetInput(1, SI_GAMEPAD, &state->joypad, 0);

    /* Initialise the palette (calls FCEUD_SetPalette for each entry) */
    FCEU_ResetPalette();

    return 0;
}

void oe_core_run_frame(OECoreState *state)
{
    uint8 *xbuf = NULL;
    int32_t *sound = state->sound_buffer;
    state->sound_size = 0;

    /* FCEUI_Emulate may replace the sound pointer; keep ownership in state by
     * passing a local temporary pointer. */
    FCEUI_Emulate(&xbuf, &sound, &state->sound_size, 0);

    /* Convert indexed-8 → RGBA8888 using the driver palette */
    if (xbuf) {
        uint32_t *pal   = fceu_get_palette();
        uint32_t *dst   = state->rgba_buffer;
        const uint8 *src = xbuf;
        for (int i = 0; i < NES_WIDTH * NES_HEIGHT; i++) {
            dst[i] = pal[src[i]];
        }
    }
}

const void *oe_core_video_buffer(OECoreState *state)
{
    return state->rgba_buffer;
}

void oe_core_video_size(OECoreState *state, int *width, int *height)
{
    (void)state;
    *width  = NES_WIDTH;
    *height = NES_HEIGHT;
}

OEPixelFormat oe_core_pixel_format(OECoreState *state)
{
    (void)state;
    return OE_PIXEL_RGBA8888;
}

void oe_core_reset(OECoreState *state)
{
    (void)state;
    ResetNES();
}

void oe_core_destroy(OECoreState *state)
{
    if (!state) return;
    FCEUI_CloseGame();
    FCEUI_Kill();
    free(state->rgba_buffer);
    free(state->sound_buffer);
    free(state);
}
