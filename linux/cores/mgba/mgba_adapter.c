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

#include <stdlib.h>
#include <string.h>

/* 4 bytes per pixel (RGBA8888) */
#define BYTES_PER_PIXEL 4

struct OECoreState {
    struct mCore *core;
    void         *video_buffer;
    unsigned      width;
    unsigned      height;
};

/* Suppress mGBA log output in Phase 0 */
static void _silent_log(struct mLogger *log, int category,
                         enum mLogLevel level, const char *format,
                         va_list args)
{
    (void)log; (void)category; (void)level; (void)format; (void)args;
}

static struct mLogger g_silent_logger = { .log = _silent_log };

OECoreState *oe_core_create(void)
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

    void *buf = malloc(width * height * BYTES_PER_PIXEL);
    if (!buf) {
        core->deinit(core);
        return NULL;
    }
    memset(buf, 0, width * height * BYTES_PER_PIXEL);
    core->setVideoBuffer(core, buf, width);
    core->setAudioBufferSize(core, 1024);

    OECoreState *state = malloc(sizeof(OECoreState));
    if (!state) {
        free(buf);
        core->deinit(core);
        return NULL;
    }
    state->core         = core;
    state->video_buffer = buf;
    state->width        = width;
    state->height       = height;
    return state;
}

int oe_core_load_rom(OECoreState *state, const char *path)
{
    if (!mCoreLoadFile(state->core, path)) return -1;
    state->core->reset(state->core);
    return 0;
}

void oe_core_run_frame(OECoreState *state)
{
    state->core->runFrame(state->core);
}

const void *oe_core_video_buffer(OECoreState *state)
{
    return state->video_buffer;
}

void oe_core_video_size(OECoreState *state, int *width, int *height)
{
    *width  = (int)state->width;
    *height = (int)state->height;
}

OEPixelFormat oe_core_pixel_format(OECoreState *state)
{
    (void)state;
    return OE_PIXEL_RGBA8888;
}

void oe_core_reset(OECoreState *state)
{
    state->core->reset(state->core);
}

void oe_core_destroy(OECoreState *state)
{
    if (!state) return;
    mCoreConfigDeinit(&state->core->config);
    state->core->deinit(state->core);
    free(state->video_buffer);
    free(state);
}
