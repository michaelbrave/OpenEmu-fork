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
#include "src/state.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cstdlib>

/* FCEUX exposes the raw indexed video buffer via this extern */
extern uint8 *XBuf;

/* Palette maintained by fceu_driver.cpp */
extern uint32_t *fceu_get_palette(void);

#define NES_WIDTH  256
#define NES_HEIGHT 240
/* Sound buffer size: ~100ms at 48000 Hz, stereo int16_t samples */
#define SIZESOUNDBUFFER (48000 / 10 * 2)

struct OECoreState {
    uint32_t *rgba_buffer;   /* NES_WIDTH × NES_HEIGHT RGBA8888 */
    uint32_t  joypad[2];     /* input state for ports 0 and 1 (bitmask) */

    int16_t  *sound_buffer;
    size_t    sound_head;
    size_t    sound_tail;
    size_t    sound_count;
};

/* -----------------------------------------------------------------------
 * Button mapping
 * ----------------------------------------------------------------------- */
enum OENESButton
{
    OENESButtonUp,
    OENESButtonDown,
    OENESButtonLeft,
    OENESButtonRight,
    OENESButtonA,
    OENESButtonB,
    OENESButtonX,
    OENESButtonY,
    OENESButtonL,
    OENESButtonR,
    OENESButtonStart,
    OENESButtonSelect,
    OENESButtonCount
};

static uint32_t nes_button_to_fceu(int button) {
    switch (button) {
        case OENESButtonUp:     return JOY_UP;
        case OENESButtonDown:   return JOY_DOWN;
        case OENESButtonLeft:   return JOY_LEFT;
        case OENESButtonRight:  return JOY_RIGHT;
        case OENESButtonA:      return JOY_A;
        case OENESButtonB:      return JOY_B;
        case OENESButtonStart:  return JOY_START;
        case OENESButtonSelect: return JOY_SELECT;
        default: return 0;
    }
}

/* -----------------------------------------------------------------------
 * oe_core_interface implementation
 * ----------------------------------------------------------------------- */

static OECoreState *fceu_create(void)
{
    OECoreState *state = (OECoreState *)calloc(1, sizeof(OECoreState));
    if (!state) return NULL;

    state->rgba_buffer  = (uint32_t *)malloc(NES_WIDTH * NES_HEIGHT * 4);
    state->sound_buffer = (int16_t  *)malloc(SIZESOUNDBUFFER * sizeof(int16_t));

    if (!state->rgba_buffer || !state->sound_buffer) {
        free(state->rgba_buffer);
        free(state->sound_buffer);
        free(state);
        return NULL;
    }

    memset(state->rgba_buffer, 0, NES_WIDTH * NES_HEIGHT * 4);
    return state;
}

static void fceu_destroy(OECoreState *state)
{
    if (!state) return;
    FCEUI_CloseGame();
    FCEUI_Kill();
    free(state->rgba_buffer);
    free(state->sound_buffer);
    free(state);
}

static int fceu_load_rom(OECoreState *state, const char *path)
{
    FCEUI_Initialize();

    /* Enable sound: 48kHz, stereo, 8-bit? No, FCEUI_Sound takes rate.
       FCEUI_SetSoundVolume(100);
    */
    FCEUI_Sound(48000);
    FCEUI_SetSoundQuality(1);
    FCEUI_SetSoundVolume(100); /* 150 caused int32→int16 overflow/distortion */

    /* Set base directory for saves in /tmp */
    FCEUI_SetBaseDirectory("/tmp/openemu_fceu");

    FCEUGI *game_info = FCEUI_LoadGame(path, 1, true);
    if (!game_info) return -1;

    /* Configure two standard gamepads */
    FCEUI_SetInput(0, SI_GAMEPAD, &state->joypad[0], 0);
    FCEUI_SetInput(1, SI_GAMEPAD, &state->joypad[1], 0);

    /* Initialise the palette */
    FCEU_ResetPalette();

    return 0;
}

static void fceu_run_frame(OECoreState *state)
{
    static int debugFrames = 0;
    uint8 *xbuf = NULL;
    int32_t *sound = NULL;
    int32_t sound_size = 0;

    FCEUI_Emulate(&xbuf, &sound, &sound_size, 0);

    /* Convert indexed-8 → RGBA8888 */
    if (xbuf) {
        uint32_t *pal   = fceu_get_palette();
        uint32_t *dst   = state->rgba_buffer;
        const uint8 *src = xbuf;
        for (int i = 0; i < NES_WIDTH * NES_HEIGHT; i++) {
            dst[i] = pal[src[i]];
        }
    }

    /*
     * FCEUX returns mono int32_t samples in signed 16-bit range at volume=100.
     * Clamp before casting so any future volume bumps don't wrap into distortion.
     * Duplicate each mono sample for both L and R channels (stereo interleaved).
     */
    if (sound && sound_size > 0) {
        if (std::getenv("OPENEMU_DEBUG_AUDIO") && debugFrames < 10) {
            fprintf(stderr, "[fceu-audio] sound_size=%d first=%d\n", sound_size, (int)sound[0]);
            debugFrames++;
        }
        for (int i = 0; i < sound_size; i++) {
            int32_t raw = sound[i];
            if (raw >  32767) raw =  32767;
            if (raw < -32768) raw = -32768;
            int16_t sample = (int16_t)raw;
            for (int channel = 0; channel < 2; ++channel) {
                if (state->sound_count < SIZESOUNDBUFFER) {
                    state->sound_buffer[state->sound_head] = sample;
                    state->sound_head = (state->sound_head + 1) % SIZESOUNDBUFFER;
                    state->sound_count++;
                }
            }
        }
    }
}

static void fceu_reset(OECoreState *state)
{
    (void)state;
    ResetNES();
}

static int fceu_save_state(OECoreState *state, const char *path)
{
    (void)state;
    FCEUI_SaveState(path, "wb");
    return 0;
}

static int fceu_load_state(OECoreState *state, const char *path)
{
    (void)state;
    FCEUI_LoadState(path, "rb");
    return 0;
}

static void fceu_set_button(OECoreState *state, int player, int button, int pressed)
{
    if (player < 0 || player > 1) return;
    uint32_t mask = nes_button_to_fceu(button);
    if (mask) {
        if (pressed)
            state->joypad[player] |= mask;
        else
            state->joypad[player] &= ~mask;
    }
}

static void fceu_set_axis(OECoreState *state, int player, int axis, int value)
{
    (void)state; (void)player; (void)axis; (void)value;
}

static void fceu_get_video_size(OECoreState *state, int *width, int *height)
{
    (void)state;
    *width  = NES_WIDTH;
    *height = NES_HEIGHT;
}

static OEPixelFormat fceu_get_pixel_format(OECoreState *state)
{
    (void)state;
    return OE_PIXEL_RGBA8888;
}

static const void *fceu_get_video_buffer(OECoreState *state)
{
    return state->rgba_buffer;
}

static int fceu_get_audio_sample_rate(OECoreState *state)
{
    (void)state;
    return 48000;
}

static size_t fceu_read_audio(OECoreState *state, int16_t *buffer, size_t max_samples)
{
    size_t count = state->sound_count;
    if (count > max_samples) count = max_samples;

    for (size_t i = 0; i < count; i++) {
        buffer[i] = state->sound_buffer[state->sound_tail];
        state->sound_tail = (state->sound_tail + 1) % SIZESOUNDBUFFER;
    }
    state->sound_count -= count;
    return count;
}

static const OECoreInterface fceu_interface = {
    fceu_create,
    fceu_destroy,
    fceu_load_rom,
    fceu_run_frame,
    fceu_reset,
    fceu_save_state,
    fceu_load_state,
    fceu_set_button,
    fceu_set_axis,
    fceu_get_video_size,
    fceu_get_pixel_format,
    fceu_get_video_buffer,
    fceu_get_audio_sample_rate,
    fceu_read_audio
};

extern "C" const OECoreInterface *oe_get_core_interface(void)
{
    return &fceu_interface;
}

/* -----------------------------------------------------------------------
 * Phase 0 Compatibility Exports
 * ----------------------------------------------------------------------- */
extern "C" OECoreState *oe_core_create(void) { return fceu_create(); }
extern "C" int oe_core_load_rom(OECoreState *s, const char *p) { return fceu_load_rom(s, p); }
extern "C" void oe_core_run_frame(OECoreState *s) { fceu_run_frame(s); }
extern "C" const void *oe_core_video_buffer(OECoreState *s) { return fceu_get_video_buffer(s); }
extern "C" void oe_core_video_size(OECoreState *s, int *w, int *h) { fceu_get_video_size(s, w, h); }
extern "C" OEPixelFormat oe_core_pixel_format(OECoreState *s) { return fceu_get_pixel_format(s); }
extern "C" void oe_core_reset(OECoreState *s) { fceu_reset(s); }
extern "C" void oe_core_destroy(OECoreState *s) { fceu_destroy(s); }
