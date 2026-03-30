/*
 * mgba_adapter.c — OpenEMU Linux port, Phase 0
 *
 * Implements oe_core_interface.h for the mGBA GBA emulator core.
 * Wraps the clean mGBA C function-pointer API directly,
 * bypassing the macOS ObjC wrapper (mGBA/src/platform/openemu/mGBAGameCore.m).
 */

#include "oe_core_interface.h"

#include <mgba-util/common.h>
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/gba/core.h>
#include <mgba-util/vfs.h>
#include <mgba/core/interface.h>
#include <mgba/core/blip_buf.h>
#include <mgba/internal/gba/input.h>

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

/* 4 bytes per pixel (RGBA8888) */
#define BYTES_PER_PIXEL 4
/* Sound buffer size: ~100ms at 32768 Hz, stereo int16_t samples */
#define SIZESOUNDBUFFER (32768 / 10 * 2)

struct OECoreState {
    struct mCore *core;
    void         *video_buffer;
    unsigned      width;
    unsigned      height;

    int16_t      *sound_buffer;
    size_t        sound_head;
    size_t        sound_tail;
    size_t        sound_count;
};

/* -----------------------------------------------------------------------
 * Button mapping
 * ----------------------------------------------------------------------- */
enum OEGBAButton
{
    OEGBAButtonUp,
    OEGBAButtonDown,
    OEGBAButtonLeft,
    OEGBAButtonRight,
    OEGBAButtonA,
    OEGBAButtonB,
    OEGBAButtonX,
    OEGBAButtonY,
    OEGBAButtonL,
    OEGBAButtonR,
    OEGBAButtonStart,
    OEGBAButtonSelect,
    OEGBAButtonCount
};

static int gba_button_to_mgba(int button) {
    switch (button) {
        case OEGBAButtonUp:     return GBA_KEY_UP;
        case OEGBAButtonDown:   return GBA_KEY_DOWN;
        case OEGBAButtonLeft:   return GBA_KEY_LEFT;
        case OEGBAButtonRight:  return GBA_KEY_RIGHT;
        case OEGBAButtonA:      return GBA_KEY_A;
        case OEGBAButtonB:      return GBA_KEY_B;
        case OEGBAButtonX:      return GBA_KEY_A;
        case OEGBAButtonY:      return GBA_KEY_B;
        case OEGBAButtonL:      return GBA_KEY_L;
        case OEGBAButtonR:      return GBA_KEY_R;
        case OEGBAButtonStart:  return GBA_KEY_START;
        case OEGBAButtonSelect: return GBA_KEY_SELECT;
        default: return GBA_KEY_NONE;
    }
}

/* Suppress mGBA log output */
static void _silent_log(struct mLogger* log, int category,
                         enum mLogLevel level, const char* format,
                         va_list args)
{
    (void)log; (void)category; (void)level; (void)format; (void)args;
}

static struct mLogger g_silent_logger = { .log = _silent_log };

/* -----------------------------------------------------------------------
 * oe_core_interface implementation
 * ----------------------------------------------------------------------- */

static OECoreState *mgba_create(void)
{
    mLogSetDefaultLogger(&g_silent_logger);

    struct mCore *core = GBACoreCreate();
    if (!core) return NULL;

    mCoreInitConfig(core, NULL);

    struct mCoreOptions opts = { .useBios = false };
    mCoreConfigLoadDefaults(&core->config, &opts);

    if (!core->init(core)) {
        core->deinit(core);
        return NULL;
    }

    unsigned width = 0, height = 0;
    core->desiredVideoDimensions(core, &width, &height);

    void *vbuf = malloc(width * height * BYTES_PER_PIXEL);
    int16_t *abuf = malloc(SIZESOUNDBUFFER * sizeof(int16_t));
    if (!vbuf || !abuf) {
        free(vbuf); free(abuf);
        core->deinit(core);
        return NULL;
    }
    memset(vbuf, 0, width * height * BYTES_PER_PIXEL);
    core->setVideoBuffer(core, (color_t *)vbuf, width);
    core->setAudioBufferSize(core, 1024);

    OECoreState *state = (OECoreState *)calloc(1, sizeof(OECoreState));
    if (!state) {
        free(vbuf); free(abuf);
        core->deinit(core);
        return NULL;
    }
    state->core         = core;
    state->video_buffer = vbuf;
    state->width        = width;
    state->height       = height;
    state->sound_buffer = abuf;

    return state;
}

static void mgba_destroy(OECoreState *state)
{
    if (!state) return;
    mCoreConfigDeinit(&state->core->config);
    state->core->deinit(state->core);
    free(state->video_buffer);
    free(state->sound_buffer);
    free(state);
}

static int mgba_load_rom(OECoreState *state, const char *path)
{
    if (!mCoreLoadFile(state->core, path)) return -1;
    
    blip_set_rates(state->core->getAudioChannel(state->core, 0), state->core->frequency(state->core), 32768);
    blip_set_rates(state->core->getAudioChannel(state->core, 1), state->core->frequency(state->core), 32768);

    state->core->reset(state->core);
    return 0;
}

static void mgba_run_frame(OECoreState *state)
{
    state->core->runFrame(state->core);

    /* Extract audio samples */
    struct blip_t *left = state->core->getAudioChannel(state->core, 0);
    struct blip_t *right = state->core->getAudioChannel(state->core, 1);
    size_t available = blip_samples_avail(left);
    
    if (available > 0) {
        int16_t samples[2048 * 2];
        if (available > 2048) available = 2048;
        
        blip_read_samples(left, samples, available, true);
        blip_read_samples(right, samples + 1, available, true);

        for (size_t i = 0; i < available * 2; i++) {
            if (state->sound_count < SIZESOUNDBUFFER) {
                state->sound_buffer[state->sound_head] = samples[i];
                state->sound_head = (state->sound_head + 1) % SIZESOUNDBUFFER;
                state->sound_count++;
            }
        }
    }
}

static void mgba_reset(OECoreState *state)
{
    state->core->reset(state->core);
}

static int mgba_save_state(OECoreState *state, const char *path)
{
    struct VFile *vf = VFileOpen(path, O_CREAT | O_TRUNC | O_WRONLY);
    if (!vf) return -1;
    bool ok = mCoreSaveStateNamed(state->core, vf, 0);
    vf->close(vf);
    return ok ? 0 : -2;
}

static int mgba_load_state(OECoreState *state, const char *path)
{
    struct VFile *vf = VFileOpen(path, O_RDONLY);
    if (!vf) return -1;
    bool ok = mCoreLoadStateNamed(state->core, vf, 0);
    vf->close(vf);
    return ok ? 0 : -2;
}

static void mgba_set_button(OECoreState *state, int player, int button, int pressed)
{
    if (player != 0) return;
    int key = gba_button_to_mgba(button);
    if (key != GBA_KEY_NONE) {
        if (pressed)
            state->core->addKeys(state->core, 1 << key);
        else
            state->core->clearKeys(state->core, 1 << key);
    }
}

static void mgba_set_axis(OECoreState *state, int player, int axis, int value)
{
    (void)state; (void)player; (void)axis; (void)value;
}

static void mgba_get_video_size(OECoreState *state, int *width, int *height)
{
    *width  = (int)state->width;
    *height = (int)state->height;
}

static OEPixelFormat mgba_get_pixel_format(OECoreState *state)
{
    (void)state;
    return OE_PIXEL_RGBA8888;
}

static const void *mgba_get_video_buffer(OECoreState *state)
{
    return state->video_buffer;
}

static int mgba_get_audio_sample_rate(OECoreState *state)
{
    (void)state;
    return 32768;
}

static size_t mgba_read_audio(OECoreState *state, int16_t *buffer, size_t max_samples)
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

static const OECoreInterface mgba_interface = {
    mgba_create,
    mgba_destroy,
    mgba_load_rom,
    mgba_run_frame,
    mgba_reset,
    mgba_save_state,
    mgba_load_state,
    mgba_set_button,
    mgba_set_axis,
    mgba_get_video_size,
    mgba_get_pixel_format,
    mgba_get_video_buffer,
    mgba_get_audio_sample_rate,
    mgba_read_audio
};

const OECoreInterface *oe_get_core_interface(void)
{
    return &mgba_interface;
}

/* -----------------------------------------------------------------------
 * Phase 0 Compatibility Exports
 * ----------------------------------------------------------------------- */
OECoreState *oe_core_create(void) { return mgba_create(); }
int oe_core_load_rom(OECoreState *s, const char *p) { return mgba_load_rom(s, p); }
void oe_core_run_frame(OECoreState *s) { mgba_run_frame(s); }
const void *oe_core_video_buffer(OECoreState *s) { return mgba_get_video_buffer(s); }
void oe_core_video_size(OECoreState *s, int *w, int *h) { mgba_get_video_size(s, w, h); }
OEPixelFormat oe_core_pixel_format(OECoreState *s) { return mgba_get_pixel_format(s); }
void oe_core_reset(OECoreState *s) { mgba_reset(s); }
void oe_core_destroy(OECoreState *s) { mgba_destroy(s); }
