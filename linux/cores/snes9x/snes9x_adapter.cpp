/*
 * snes9x_adapter.cpp — OpenEMU Linux port, Phase 0
 *
 * Implements oe_core_interface.h for the SNES9x Super Nintendo emulator.
 * Calls the SNES9x C++ API directly, bypassing the macOS ObjC wrapper
 * (SNES9x/SNESGameCore.mm).
 *
 * Video output: SNES9x renders into RGB565 internally; this adapter converts
 * to RGBA8888 for the Linux frontend upload path.
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

void S9xSetSaveDirectory(const char* path);

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <vector>

/* Samplerate used by original OpenEmu wrapper */
#define SAMPLERATE      32040
/* Sound buffer size: ~100ms at 32040 Hz, stereo uint16_t samples */
#define SIZESOUNDBUFFER (SAMPLERATE / 10 * 2)

struct OECoreState {
    /*
     * video_buffer removed: S9xGraphicsInit() overwrites GFX.Screen with a
     * pointer into its own internal ScreenBuffer, so any assignment we made
     * before init was silently discarded.  We now read directly from GFX.Screen
     * after each frame instead of maintaining a separate copy.
     */
    uint32_t *rgba_buffer;      /* RGBA8888 upload buffer */
    int16_t  *sound_buffer;     /* circular audio buffer */
    size_t    sound_head;
    size_t    sound_tail;
    size_t    sound_count;
    int       render_width;     /* actual rendered width this frame */
    int       render_height;    /* actual rendered height this frame */
};

/* -----------------------------------------------------------------------
 * Audio sample callback — called by SNES9x when samples are ready
 * ----------------------------------------------------------------------- */
static void samples_available_callback(void *context)
{
    OECoreState *state = (OECoreState *)context;
    if (!state || !state->sound_buffer) return;

    /*
     * S9xMixSamples returns FALSE and fills zeros if you ask for more
     * int16_t samples than the resampler holds.  At 32040 Hz / 60 fps only
     * ~1068 samples exist per callback.  Ask for exactly what is ready.
     */
    int available = S9xGetSampleCount();
    if (available <= 0) return;

    size_t space = SIZESOUNDBUFFER - state->sound_count;
    if (space == 0) return;

    size_t to_read = (size_t)available;
    if (to_read > space)   to_read = space;
    if (to_read > 2048)    to_read = 2048;

    int16_t tmp[2048];
    if (!S9xMixSamples((uint8_t *)tmp, (int)to_read)) return;

    for (size_t i = 0; i < to_read; i++) {
        state->sound_buffer[state->sound_head] = tmp[i];
        state->sound_head = (state->sound_head + 1) % SIZESOUNDBUFFER;
    }
    state->sound_count += to_read;
}

/* -----------------------------------------------------------------------
 * Button mapping
 * ----------------------------------------------------------------------- */
enum OESNESButton
{
    OESNESButtonUp,
    OESNESButtonDown,
    OESNESButtonLeft,
    OESNESButtonRight,
    OESNESButtonA,
    OESNESButtonB,
    OESNESButtonX,
    OESNESButtonY,
    OESNESButtonTriggerLeft,
    OESNESButtonTriggerRight,
    OESNESButtonStart,
    OESNESButtonSelect,
    OESNESButtonCount
};

static uint32_t snes_button_to_s9x(int button) {
    /* These match the order in SNESEmulatorKeys and OESNESButton */
    return button;
}

/* -----------------------------------------------------------------------
 * oe_core_interface implementation
 * ----------------------------------------------------------------------- */

static OECoreState *snes_create(void)
{
    OECoreState *state = (OECoreState *)calloc(1, sizeof(OECoreState));
    if (!state) return NULL;

    state->rgba_buffer  = (uint32_t *)malloc(MAX_SNES_WIDTH * MAX_SNES_HEIGHT * sizeof(uint32_t));
    state->sound_buffer = (int16_t  *)malloc(SIZESOUNDBUFFER * sizeof(int16_t));

    if (!state->rgba_buffer || !state->sound_buffer) {
        free(state->rgba_buffer);
        free(state->sound_buffer);
        free(state);
        return NULL;
    }

    memset(state->rgba_buffer, 0, MAX_SNES_WIDTH * MAX_SNES_HEIGHT * sizeof(uint32_t));
    state->render_width  = 256;
    state->render_height = 224;
    return state;
}

static void snes_destroy(OECoreState *state)
{
    if (!state) return;
    S9xSetSamplesAvailableCallback(NULL, NULL);
    S9xDeinitAPU();
    S9xGraphicsDeinit();
    Memory.Deinit();
    free(state->rgba_buffer);
    free(state->sound_buffer);
    free(state);
}

static char g_save_directory[512] = "/tmp/openemu_snes9x";

static void snes_set_save_directory(OECoreState *state, const char *path)
{
    if (path) {
        snprintf(g_save_directory, sizeof(g_save_directory), "%s", path);
        S9xSetSaveDirectory(g_save_directory);
    }
}

/*
 * SDL abstract buttons vs SNES physical positions:
 *   SDL A (bottom) = SNES B,  SDL B (right) = SNES A
 *   SDL X (left)   = SNES Y,  SDL Y (top)   = SNES X
 */
static const char *SNESEmulatorKeys[] = { "Up", "Down", "Left", "Right", "B", "A", "Y", "X", "L", "R", "Start", "Select", NULL };

static int snes_load_rom(OECoreState *state, const char *path)
{
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

    S9xUnmapAllControls();
    S9xSetController(0, CTL_JOYPAD, 0, 0, 0, 0);
    S9xSetController(1, CTL_JOYPAD, 1, 0, 0, 0);

    /* Map buttons for all 8 possible players */
    for (int player = 0; player < 8; player++) {
        uint32_t player_mask = (player << 16);
        char player_str[32];
        snprintf(player_str, sizeof(player_str), "Joypad%d ", player + 1);

        for (int i = 0; SNESEmulatorKeys[i]; i++) {
            char cmd_str[64];
            snprintf(cmd_str, sizeof(cmd_str), "%s%s", player_str, SNESEmulatorKeys[i]);
            s9xcommand_t cmd = S9xGetCommandT(cmd_str);
            S9xMapButton(player_mask | i, cmd, false);
        }
    }

    if (!Memory.Init() || !S9xInitAPU() || !S9xGraphicsInit()) return -1;
    /*
     * GFX.Screen must be set AFTER S9xGraphicsInit — that function allocates
     * its own internal ScreenBuffer and points GFX.Screen into it, overwriting
     * anything set before.  We leave GFX.Screen pointing at the internal buffer
     * and read pixels from it directly in snes_run_frame.
     */
    if (!S9xInitSound(100)) {}

    S9xSetSamplesAvailableCallback(samples_available_callback, state);

    if (!Memory.LoadROM(path)) return -2;

    return 0;
}

static void snes_run_frame(OECoreState *state)
{
    IPPU.RenderThisFrame = TRUE;
    S9xMainLoop();
    state->render_width  = IPPU.RenderedScreenWidth;
    state->render_height = IPPU.RenderedScreenHeight;
    if (state->render_width  == 0) state->render_width  = 256;
    if (state->render_height == 0) state->render_height = 224;

    /*
     * Read directly from GFX.Screen (SNES9x's internal buffer).
     * GFX.RealPPL is the true row stride in uint16 units (= MAX_SNES_WIDTH = 512),
     * regardless of the current render width.
     */
    const int stride = (int)GFX.RealPPL;
    for (int y = 0; y < state->render_height; ++y) {
        for (int x = 0; x < state->render_width; ++x) {
            const uint16_t pixel = GFX.Screen[y * stride + x];
            const uint8_t r = ((pixel >> 11) & 0x1F) * 255 / 31;
            const uint8_t g = ((pixel >> 5)  & 0x3F) * 255 / 63;
            const uint8_t b = ( pixel        & 0x1F) * 255 / 31;
            state->rgba_buffer[y * state->render_width + x] =
                (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | 0xFF000000u;
        }
    }
}

static void snes_reset(OECoreState *state)
{
    (void)state;
    S9xSoftReset();
}

static int snes_save_state(OECoreState *state, const char *path)
{
    (void)state;
    return S9xFreezeGame(path) ? 0 : -1;
}

static int snes_load_state(OECoreState *state, const char *path)
{
    (void)state;
    return S9xUnfreezeGame(path) ? 0 : -1;
}

static void snes_set_button(OECoreState *state, int player, int button, int pressed)
{
    (void)state;
    if (player >= 0 && player < 8 && button >= 0 && button < OESNESButtonCount) {
        S9xReportButton((player << 16) | button, pressed != 0);
    }
}

static void snes_set_axis(OECoreState *state, int player, int axis, int value)
{
    (void)state; (void)player; (void)axis; (void)value;
}

static void snes_get_video_size(OECoreState *state, int *width, int *height)
{
    *width  = state->render_width;
    *height = state->render_height;
}

static OEPixelFormat snes_get_pixel_format(OECoreState *state)
{
    (void)state;
    return OE_PIXEL_RGBA8888;
}

static const void *snes_get_video_buffer(OECoreState *state)
{
    return state->rgba_buffer;
}

static int snes_get_audio_sample_rate(OECoreState *state)
{
    (void)state;
    return SAMPLERATE;
}

static size_t snes_read_audio(OECoreState *state, int16_t *buffer, size_t max_samples)
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

static const OECoreInterface snes_interface = {
    snes_create,
    snes_destroy,
    snes_set_save_directory,
    snes_load_rom,
    snes_run_frame,
    snes_reset,
    snes_save_state,
    snes_load_state,
    snes_set_button,
    snes_set_axis,
    snes_get_video_size,
    snes_get_pixel_format,
    snes_get_video_buffer,
    snes_get_audio_sample_rate,
    snes_read_audio
};

extern "C" const OECoreInterface *oe_get_core_interface(void)
{
    return &snes_interface;
}

/* -----------------------------------------------------------------------
 * Phase 0 Compatibility Exports
 * ----------------------------------------------------------------------- */
extern "C" OECoreState *oe_core_create(void) { return snes_create(); }
extern "C" int oe_core_load_rom(OECoreState *s, const char *p) { return snes_load_rom(s, p); }
extern "C" void oe_core_run_frame(OECoreState *s) { snes_run_frame(s); }
extern "C" const void *oe_core_video_buffer(OECoreState *s) { return snes_get_video_buffer(s); }
extern "C" void oe_core_video_size(OECoreState *s, int *w, int *h) { snes_get_video_size(s, w, h); }
extern "C" OEPixelFormat oe_core_pixel_format(OECoreState *s) { return snes_get_pixel_format(s); }
extern "C" void oe_core_reset(OECoreState *s) { snes_reset(s); }
extern "C" void oe_core_destroy(OECoreState *s) { snes_destroy(s); }
