/*
 * fceu_driver.cpp — Stub FCEUD_* platform driver implementations for headless
 * operation in the OpenEMU Linux Phase 0 test harness.
 *
 * FCEUX separates the emulator core (FCEUI_*) from the driver/platform layer
 * (FCEUD_*). All functions declared in FCEU/src/driver.h with a FCEUD_ prefix
 * must be provided by the driver. This file provides minimal no-op stubs.
 */

#include "src/driver.h"
#include "src/types.h"
#include "src/fceu.h"
#include "src/input.h"
#include "src/emufile.h"
#include "src/file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

/* Globals expected by core paths not included in the Linux port glue. */
int dendy = 0;
int KillFCEUXonFrame = 0;
bool swapDuty = false;
bool turbo = false;
int eoptions = 0;
int pal_emulation = 0;
int closeFinishedMovie = 0;

/* -----------------------------------------------------------------------
 * Palette — filled by FCEUD_SetPalette, read by adapter for RGB conversion
 * ----------------------------------------------------------------------- */
static uint32_t g_palette[256];

void FCEUD_SetPalette(uint8 index, uint8 r, uint8 g, uint8 b)
{
    /* Store as RGBA8888 for OpenGL texture upload: R=byte0, G=byte1, B=byte2, A=byte3 */
    g_palette[index] = ((uint32_t)r << 0) | ((uint32_t)g << 8) |
                       ((uint32_t)b << 16) | ((uint32_t)0xFF << 24);
}

void FCEUD_GetPalette(uint8 i, uint8 *r, uint8 *g, uint8 *b)
{
    /* g_palette is stored as R=byte0, G=byte1, B=byte2, A=byte3 (RGBA8888) */
    uint32_t c = g_palette[i];
    *r = (c >>  0) & 0xFF;
    *g = (c >>  8) & 0xFF;
    *b = (c >> 16) & 0xFF;
}

uint32_t *fceu_get_palette(void)
{
    return g_palette;
}

/* -----------------------------------------------------------------------
 * Error/message output
 * ----------------------------------------------------------------------- */
void FCEUD_PrintError(const char *s)
{
    fprintf(stderr, "[fceu] error: %s\n", s);
}

void FCEUD_Message(const char *s)
{
    /* Suppress informational messages in headless mode */
    (void)s;
}

uint64 FCEUD_GetTime(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64)tv.tv_sec * 1000000ULL + (uint64)tv.tv_usec;
}

uint64 FCEUD_GetTimeFreq(void)
{
    return 1000000ULL;
}

void RefreshThrottleFPS(void)
{
}

/* -----------------------------------------------------------------------
 * File I/O
 * ----------------------------------------------------------------------- */
FILE *FCEUD_UTF8fopen(const char *fn, const char *mode)
{
    return fopen(fn, mode);
}

EMUFILE_FILE *FCEUD_UTF8_fstream(const char *n, const char *m)
{
    /* EMUFILE_FILE has no FILE* constructor; open by name */
    return new EMUFILE_FILE(n, m);
}

/* -----------------------------------------------------------------------
 * Archive support — stubs; Phase 0 only handles plain ROM files
 * ----------------------------------------------------------------------- */
ArchiveScanRecord FCEUD_ScanArchive(std::string fname)
{
    return ArchiveScanRecord();
}

FCEUFILE *FCEUD_OpenArchiveIndex(ArchiveScanRecord &asr, std::string &fname,
                                  int innerIndex)
{
    (void)asr; (void)fname; (void)innerIndex;
    return NULL;
}

FCEUFILE *FCEUD_OpenArchiveIndex(ArchiveScanRecord &asr, std::string &fname,
                                  int innerIndex, int *userCancel)
{
    (void)asr; (void)fname; (void)innerIndex;
    if (userCancel) *userCancel = 0;
    return NULL;
}

FCEUFILE *FCEUD_OpenArchive(ArchiveScanRecord &asr, std::string &fname,
                             std::string *innerFilename)
{
    (void)asr; (void)fname; (void)innerFilename;
    return NULL;
}

FCEUFILE *FCEUD_OpenArchive(ArchiveScanRecord &asr, std::string &fname,
                             std::string *innerFilename, int *userCancel)
{
    (void)asr; (void)fname; (void)innerFilename;
    if (userCancel) *userCancel = 0;
    return NULL;
}

/* -----------------------------------------------------------------------
 * Network stubs
 * ----------------------------------------------------------------------- */
int FCEUD_SendData(void *data, uint32 len)
{
    (void)data; (void)len;
    return 0;
}

int FCEUD_RecvData(void *data, uint32 len)
{
    (void)data; (void)len;
    return 0;
}

void FCEUD_NetplayText(uint8 *text)
{
    (void)text;
}

void FCEUD_NetworkClose(void)
{
}

/* -----------------------------------------------------------------------
 * Sound / emulation speed
 * ----------------------------------------------------------------------- */
void FCEUD_SoundToggle(void)
{
}

void FCEUD_SoundVolumeAdjust(int d)
{
    (void)d;
}

void FCEUD_SetEmulationSpeed(int cmd)
{
    (void)cmd;
}

void FCEUD_TurboOn(void)
{
}

void FCEUD_TurboOff(void)
{
}

void FCEUD_TurboToggle(void)
{
}

unsigned int *GetKeyboard(void)
{
    static unsigned int keys[256];
    return keys;
}

void GetMouseData(uint32 (&md)[3])
{
    md[0] = md[1] = md[2] = 0;
}

/* -----------------------------------------------------------------------
 * UI stubs
 * ----------------------------------------------------------------------- */
int FCEUD_ShowStatusIcon(void)
{
    return 0;
}

void FCEUD_ToggleStatusIcon(void)
{
}

void FCEUD_HideMenuToggle(void)
{
}

void FCEUD_CmdOpen(void)
{
}

void FCEUD_SaveStateAs(void)
{
}

void FCEUD_LoadStateFrom(void)
{
}

void FCEUD_MovieRecordTo(void)
{
}

void FCEUD_MovieReplayFrom(void)
{
}

void FCEUD_LuaRunFrom(void)
{
}

void FCEUD_AviRecordTo(void)
{
}

void FCEUD_AviStop(void)
{
}

bool FCEUD_ShouldDrawInputAids(void)
{
    return false;
}

void FCEUD_OnCloseGame(void)
{
}

void FCEUI_UseInputPreset(int preset)
{
    (void)preset;
}

bool FCEUI_AviIsRecording(void)
{
    return false;
}

void FCEUI_AviVideoUpdate(const unsigned char *buffer)
{
    (void)buffer;
}

bool FCEUI_AviEnableHUDrecording(void)
{
    return false;
}

bool FCEUI_AviDisableMovieMessages(void)
{
    return false;
}

/* -----------------------------------------------------------------------
 * Input configuration
 * ----------------------------------------------------------------------- */
void FCEUD_SetInput(bool fourscore, bool microphone, ESI port0, ESI port1,
                    ESIFC fcexp)
{
    /* In Phase 0 we configure input directly via FCEUI_SetInput() in the
     * adapter; this callback is just informational. */
    (void)fourscore; (void)microphone; (void)port0; (void)port1; (void)fcexp;
}

/* -----------------------------------------------------------------------
 * Debugger callbacks
 * ----------------------------------------------------------------------- */
void FCEUD_DebugBreakpoint(int bp_num)
{
    (void)bp_num;
}

void FCEUD_TraceInstruction(uint8 *opcode, int size)
{
    (void)opcode; (void)size;
}

void FCEUD_FlushTrace(void)
{
}

void FCEUD_UpdateNTView(int scanline, bool drawall)
{
    (void)scanline; (void)drawall;
}

void FCEUD_UpdatePPUView(int scanline, int drawall)
{
    (void)scanline; (void)drawall;
}

bool FCEUD_PauseAfterPlayback(void)
{
    return false;
}

void FCEUD_VideoChanged(void)
{
}

/* -----------------------------------------------------------------------
 * Compiler info
 * ----------------------------------------------------------------------- */
const char *FCEUD_GetCompilerString(void)
{
    return "gcc-linux";
}
