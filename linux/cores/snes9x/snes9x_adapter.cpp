/*
 * snes9x_adapter.cpp — OpenEMU Linux port, Phase 0
 *
 * Implements oe_core_interface.h for the SNES9x Super Nintendo emulator.
 * Calls the SNES9x C++ API directly, bypassing the macOS ObjC wrapper
 * (SNES9x/SNESGameCore.mm).
 *
 * Video output: RGB565 (2 bytes/pixel), MAX_SNES_WIDTH × MAX_SNES_HEIGHT.
 * The actual rendered region is typically 256×224 or 512×224 for hi-res.
 */

#include "oe_core_interface.h"

#include "snes9x.h"
#include "memmap.h"
#include "pixform.h"
#include "gfx.h"
#include "display.h"
#include "ppu.h"
#include "apu/apu.h"
#include "controls.h"
#include "snapshot.h"
#include "cheats.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Samplerate used by original OpenEmu wrapper */
#define SAMPLERATE      32040
/* Sound buffer size: ~20ms at 32040 Hz, stereo uint16_t samples */
#define SIZESOUNDBUFFER (SAMPLERATE / 50 * 4)

struct OECoreState {
    uint16_t *video_buffer;    /* GFX.Screen points here */
    uint16_t *sound_buffer;    /* audio samples accumulator */
    int       render_width;    /* actual rendered width this frame */
    int       render_height;   /* actual rendered height this frame */
};

/* -----------------------------------------------------------------------
 * Audio sample callback — called by SNES9x when samples are ready
 * ----------------------------------------------------------------------- */
static void samples_available_callback(void *context)
{
    OECoreState *state = (OECoreState *)context;
    if (!state || !state->sound_buffer) return;

    /* Drain the samples into our buffer (discard for Phase 0) */
    int16_t tmp[SIZESOUNDBUFFER * 2];
    S9xMixSamples((uint8_t *)tmp, SIZESOUNDBUFFER);
    (void)tmp;
}

/* -----------------------------------------------------------------------
 * oe_core_interface implementation
 * ----------------------------------------------------------------------- */

OECoreState *oe_core_create(void)
{
    OECoreState *state = (OECoreState *)calloc(1, sizeof(OECoreState));
    if (!state) return NULL;

    state->video_buffer = (uint16_t *)malloc(MAX_SNES_WIDTH * MAX_SNES_HEIGHT * sizeof(uint16_t));
    state->sound_buffer = (uint16_t *)malloc(SIZESOUNDBUFFER * 2 * sizeof(uint16_t));

    if (!state->video_buffer || !state->sound_buffer) {
        free(state->video_buffer);
        free(state->sound_buffer);
        free(state);
        return NULL;
    }

    memset(state->video_buffer, 0, MAX_SNES_WIDTH * MAX_SNES_HEIGHT * sizeof(uint16_t));
    state->render_width  = 256;
    state->render_height = 224;
    return state;
}

int oe_core_load_rom(OECoreState *state, const char *path)
{
    /* --- Mirror the settings from SNESGameCore.mm loadFileAtPath --- */
    memset(&Settings, 0, sizeof(Settings));

    Settings.MouseMaster              = TRUE;
    Settings.SuperScopeMaster         = TRUE;
    Settings.MultiPlayer5Master       = TRUE;
    Settings.JustifierMaster          = TRUE;
    Settings.SoundSync                = FALSE;
    Settings.SoundPlaybackRate        = SAMPLERATE;
    Settings.SoundInputRate           = 32040;
    Settings.DynamicRateControl       = FALSE;
    Settings.DynamicRateLimit         = 5;
    Settings.Transparency             = TRUE;
    GFX.InfoStringTimeout             = 0;
    Settings.DontSaveOopsSnapshot     = TRUE;
    Settings.NoPatch                  = TRUE;
    Settings.AutoSaveDelay            = 0;
    Settings.InterpolationMethod      = DSP_INTERPOLATION_GAUSSIAN;
    Settings.OneClockCycle            = ONE_CYCLE;
    Settings.OneSlowClockCycle        = SLOW_ONE_CYCLE;
    Settings.TwoClockCycles           = TWO_CYCLES;
    Settings.SuperFXClockMultiplier   = 100;
    Settings.OverclockMode            = 0;
    Settings.SeparateEchoBuffer       = FALSE;
    Settings.DisableGameSpecificHacks = FALSE;
    Settings.BlockInvalidVRAMAccessMaster = TRUE;
    Settings.HDMATimingHack           = 100;
    Settings.MaxSpriteTilesPerLine    = 34;

    /* Point GFX.Screen at our buffer */
    GFX.Screen = state->video_buffer;

    S9xUnmapAllControls();
    /* Default: two gamepads */
    S9xSetController(0, CTL_JOYPAD, 0, 0, 0, 0);
    S9xSetController(1, CTL_JOYPAD, 1, 0, 0, 0);

    if (!Memory.Init() || !S9xInitAPU() || !S9xGraphicsInit()) {
        fprintf(stderr, "[snes9x] Init failed\n");
        return -1;
    }

    if (!S9xInitSound(100)) {
        fprintf(stderr, "[snes9x] Sound init failed (non-fatal)\n");
    }

    S9xSetSamplesAvailableCallback(samples_available_callback, state);

    if (!Memory.LoadROM(path)) {
        fprintf(stderr, "[snes9x] Failed to load ROM: %s\n", path);
        return -2;
    }

    return 0;
}

void oe_core_run_frame(OECoreState *state)
{
    IPPU.RenderThisFrame = TRUE;
    S9xMainLoop();

    /* Update render dimensions from PPU state */
    state->render_width  = IPPU.RenderedScreenWidth;
    state->render_height = IPPU.RenderedScreenHeight;
    if (state->render_width  == 0) state->render_width  = 256;
    if (state->render_height == 0) state->render_height = 224;
}

const void *oe_core_video_buffer(OECoreState *state)
{
    return state->video_buffer;
}

void oe_core_video_size(OECoreState *state, int *width, int *height)
{
    *width  = state->render_width;
    *height = state->render_height;
}

OEPixelFormat oe_core_pixel_format(OECoreState *state)
{
    (void)state;
    return OE_PIXEL_RGB565;
}

void oe_core_reset(OECoreState *state)
{
    (void)state;
    S9xSoftReset();
}

void oe_core_destroy(OECoreState *state)
{
    if (!state) return;
    S9xSetSamplesAvailableCallback(NULL, NULL);
    S9xDeinitAPU();
    S9xGraphicsDeinit();
    Memory.Deinit();
    free(state->video_buffer);
    free(state->sound_buffer);
    free(state);
}
