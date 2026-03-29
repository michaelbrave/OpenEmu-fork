/* Manually configured flags.h for the OpenEMU Linux port.
 * Only GBA emulation, software renderer, no threading, no optional features. */
#ifndef FLAGS_H
#define FLAGS_H

/* We only need GBA support */
#define M_CORE_GBA

/* Use software renderer (no OpenGL in Phase 0) */
/* BUILD_GL is NOT defined */

/* Disable threading for simplicity in Phase 0 */
#define DISABLE_THREADING

/* RGBA8888 pixel format (4 bytes, 32-bit) */
/* COLOR_16_BIT is NOT defined */

/* pthreads available on Linux (but threading disabled above) */
#define USE_PTHREADS

#endif /* FLAGS_H */
