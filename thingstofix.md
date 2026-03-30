# Things To Fix

## Fixed This Session
- **SNES video stride bug** — was reading video_buffer sequentially but SNES9x uses stride=MAX_SNES_WIDTH=512 pixels/row regardless of render width. Now reads from GFX.Screen directly (after S9xGraphicsInit) with correct Y*stride+X indexing. Removed external video_buffer entirely.
- **SNES audio slow motion** — S9xMixSamples fills zeros when asked for more samples than the resampler holds. Fixed by calling S9xGetSampleCount() first and only requesting what's available.
- **SNES A/B buttons swapped** — SDL abstract A (bottom) ≠ SNES A (right). Fixed by reordering SNESEmulatorKeys to "B","A","Y","X".
- **NES audio distortion** — FCEUI_SetSoundVolume(150) caused int32 samples to overflow int16_t. Reduced to 100 and added clamping. Palette byte order also corrected in fceu_driver.cpp.
- **Audio queue latency** — SDL_QueueAudio was unbounded causing latency buildup. Capped at ~50ms (sampleRate/20 * channels * 2 bytes).
- **Controller mapping UI** — Preferences → Input → "Configure Button Mapping..." opens a live-capture remapping dialog. Click Set next to any button, press a key or controller button. Saved to QSettings, applied on next game launch.
- **Library UI visual overhaul** — Switched from navy to dark charcoal theme (closer to original OpenEMU aesthetic). Rounded cover art tiles with sheen highlight. Sidebar now has LIBRARY/CONSOLES section headers. Double-click to launch games. Accent color changed to macOS blue (#0a84ff).

## Still Open
- **SNES**: Live test needed to confirm stride fix and button mapping work end-to-end with a real cartridge dump.
- **GBA**: PS4 L1/R1 mapping — use the Controller Mapping dialog to remap L/R shoulder if the SDL controller DB entry is wrong for your pad.
- **NES**: Controls "not perfect" — further investigation needed. Likely a button-layout expectation (NES only has A/B; X/Y/L/R silently no-op which is correct behavior).
- **Library view modes**: Add configurable grid/list/large-icon view modes similar to original OpenEMU Finder-style layout.
- **Phase 7**: Multi-process core isolation (each core runs in a child process for crash safety) — not started.
