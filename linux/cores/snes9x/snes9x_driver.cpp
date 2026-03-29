/*
 * snes9x_driver.cpp — Stub implementations of the SNES9x platform driver interface.
 *
 * SNES9x requires a "port" layer to implement display, sound, input, and
 * filesystem callbacks. This file provides no-op / minimal implementations
 * sufficient for headless frame rendering in the OpenEMU Linux Phase 0 test.
 *
 * Based on the display.h interface declarations in SNES9x/src/display.h.
 */

#include "snes9x.h"
#include "display.h"
#include "snapshot.h"
#include "gfx.h"
#include "apu/apu.h"
#include "fscompat.h"

#include <string.h>
#include <string>

/* -----------------------------------------------------------------------
 * Mandatory display callbacks
 * ----------------------------------------------------------------------- */

void S9xMessage(int type, int number, const char *message)
{
    /* Suppress all messages in Phase 0 headless mode */
    (void)type; (void)number; (void)message;
}

void S9xPutImage(int width, int height)
{
    /*
     * The video frame is already in GFX.Screen — nothing to do here.
     * Our adapter reads GFX.Screen directly after S9xMainLoop().
     */
    (void)width; (void)height;
}

bool8 S9xInitUpdate(void)
{
    return true;
}

bool8 S9xDeinitUpdate(int width, int height)
{
    (void)width; (void)height;
    return true;
}

bool8 S9xContinueUpdate(int width, int height)
{
    (void)width; (void)height;
    return true;
}

void S9xSyncSpeed(void)
{
}

void S9xInitDisplay(int argc, char **argv)
{
    (void)argc; (void)argv;
}

void S9xDeinitDisplay(void)
{
}

void S9xTextMode(void)
{
}

void S9xGraphicsMode(void)
{
}

void S9xToggleSoundChannel(int c)
{
    (void)c;
}

bool8 S9xOpenSoundDevice(void)
{
    /* Headless Phase 0: audio device is not needed for frame smoke tests. */
    return true;
}

std::string S9xGetDirectory(enum s9x_getdirtype dirtype)
{
    (void)dirtype;
    return ".";
}

std::string S9xGetFilenameInc(std::string ext, enum s9x_getdirtype dirtype)
{
    (void)dirtype;
    if (!ext.empty() && ext[0] != '.')
        ext = "." + ext;
    return std::string("snes9x_output") + ext;
}

bool8 S9xOpenSnapshotFile(const char *filepath, bool8 read_only, STREAM *file)
{
    /* Save state support via SNES9x stream abstraction */
    const char *mode = read_only ? "rb" : "wb";
    *file = OPEN_STREAM(filepath, mode);
    return (*file != NULL) ? true : false;
}

void S9xCloseSnapshotFile(STREAM file)
{
    if (file) CLOSE_STREAM(file);
}

const char *S9xStringInput(const char *message)
{
    (void)message;
    return NULL;
}

/* -----------------------------------------------------------------------
 * Optional display callbacks — no-ops
 * ----------------------------------------------------------------------- */

void S9xExtraUsage(void)
{
}

void S9xParseArg(char **argv, int &i, int argc)
{
    (void)argv; (void)i; (void)argc;
}

void S9xExtraDisplayUsage(void)
{
}

void S9xParseDisplayArg(char **argv, int &i, int argc)
{
    (void)argv; (void)i; (void)argc;
}

void S9xSetTitle(const char *title)
{
    (void)title;
}

void S9xInitInputDevices(void)
{
}

void S9xProcessEvents(bool8 block)
{
    (void)block;
}

const char *S9xSelectFilename(const char *def, const char *dir1,
                               const char *dir2, const char *title)
{
    (void)def; (void)dir1; (void)dir2; (void)title;
    return NULL;
}
