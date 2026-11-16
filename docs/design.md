# Shadow-nx design (2026-11-16, for review with Mark)

## Goal

Shadow the Hedgehog (original Xbox, USA) as a native Switch homebrew program at 60 fps at stock clocks (CPU
1020 MHz), with the port menu sheroes-nx has. Native means the game's x86 functions are compiled Switch functions,
game memory is plain Switch memory, and Direct3D 8 and DirectSound are replaced by our own code at the API level.
Nothing emulates the NV2A GPU or the MCPX sound chip.

## Why sheroes-nx 0.5.0 is the base

Checked today against the disc:

| | Sonic Heroes (sheroes-nx) | Shadow the Hedgehog |
|---|---|---|
| XDK build | 5659 (layout made by hand for this port) | 5849, the SDK's own build (Splinter Cell DA, BLACK) |
| Engine and files | RenderWare, .one, .txd, .dff, CRI ADX, Sofdec .sfd | the same (1,194 .one, 702 .txd, 224 .adx, 9 .sfd) |
| Disc | ~3,900 files | 3,510 files, 2.54 GB trimmed: fits a FAT32 card as one .iso |
| XDK functions found by signature | 134 of 238 | 134 of 238 |
| Called by the game but needing a name by hand | 7 | the same 7, all found at Shadow's addresses (below) |
| Code | about 3.2 MB of .text | 3.9 MB of .text, 39,925 functions |
| Game hooks in game/src | none | none needed so far |

The seven named by hand (game.toml, each with its evidence): D3DDevice_BeginPush 0x3D4740, EndPush 0x3D4760,
SetPalette 0x3D0E80, LoadVertexShaderProgram 0x3D1F00 (RenderWare's vertex program path), and DirectSound's
IDirectSound_SetAllParameters 0x3E577A, IDirectSoundBuffer_SetPitch 0x3E4148, SetConeAngles 0x3E4932. In Heroes the
DirectSound one crashed the first boot until it was named. Each sits in the same place among its neighbours as in the
XDK, and their code matches sheroes-nx's 5659 versions instruction for instruction.

The other 31 library functions the game calls without a name are the same kinds Heroes left alone: resource and
lock helpers (one vertex buffer lock writes a cache flush into the SDK's scratch push buffer, which is harmless),
XGraphics swizzle and D3DX helpers, three XPP helpers. None touches hardware registers.

## The pieces

| Piece | Plan | Comes from |
|---|---|---|
| Function list | xbe_recomp's own discovery plus 446 seeds from a sweep done today (pointers in .rdata/.data and code immediates into .text, and four ret-only vtable functions). Heroes' first boot trapped on a function started at its padding; the sweep covers that whole class up front | sheroes-nx method |
| Code generator | XboxXBE-Recomp's xbe_recomp, unchanged | TGP482 |
| Kernel, files, memory | SDK runtime as in sheroes-nx 0.5.0: busy ranges backed up front on the Switch, the disc image read as one file, lazy opens for an extracted folder | sheroes-nx |
| Direct3D 8 | the SDK's API-level D3D8 with sheroes-nx's RenderWare fixes (vertex program slots, vertex blending, the 0xFC pixel shader object, gamma ramp mirrored to device memory) | sheroes-nx, brextreme-nx |
| Switch renderer | deko3d by default, Vulkan (NVK) in the same NRO as a menu choice; draw_copy on | sheroes-nx |
| Sound | the SDK's DirectSound with sheroes-nx's rolloff, pan and volume fixes; music and effects volume in the menu | sheroes-nx |
| Movies | CRI Sofdec is game code (PSFD00 section), recompiled like the rest, as in Heroes | game |
| Controls and menu | sheroes-nx's port menu (+ and - together): remapping, invert camera, steering response, resolution, filtering, SMAA, FPS counter, volumes, CPU boost | sheroes-nx |
| Saves | the SDK's hard disk folder, `sdmc:/switch/shadow/hdd/` | SDK |

## Phases

0. Setup (done 2026-11-16, no build yet): the tree copied from sheroes-nx 0.5.0, renamed (`shadow.nro`,
   `sdmc:/switch/shadow/`, title 53450086), game.toml written by xbe_symbols plus the seven hand names and the seed
   sweep. The recompiler runs clean on it in the cloud (3.7 s).
1. PC boot: Mark builds `SHADOW-vk.exe` with `tools\win\build_pc.ps1` and runs it with the .iso in `assets\`. Fix
   until the first stage plays.
2. Switch: the NRO built in the cloud, the .iso in `sdmc:/switch/shadow/game/`. Fix what the PC does not show.
3. Speed at stock clocks, if needed (the sheroes-nx checklist: diagnostics off, draw_copy, sampler table, then the
   profiler).
4. Coverage: every mode from the manual (story paths, Expert mode, 2P battle, library), then 0.5.

## Expected risks (theories, not findings)

- Shadow draws more at once than Heroes (gunfire, explosions, more enemies and particles). Heroes' render thread was
  11 to 14 ms a frame on the Switch; Shadow may need more of the speed work in phase 3.
- Anything Heroes never used: a Direct3D or DirectSound path only Shadow takes would show as an "unsupported" log
  line or a trap on the first PC boot.
- XPP_MemoryUnitType was not found. The SDK treats that as a game without memory unit support; Heroes had it. To
  check on the first boot (saving and the save select screen).

## Open decisions for Mark

1. Bring-up: PC first on your Legion Go (as with Heroes: your runs are real time and the logs come back through the
   bridge), then the Switch. Or straight to Switch builds made in the cloud. Recommended: PC first.
2. License: GPL-3, inherited from sheroes-nx and brextreme-nx (no choice really, since we build on them).
3. Version numbering: start at 0.1, 0.5 when feature ready as usual.
4. Widescreen: sources disagree on whether the Xbox version offers 16:9 (one game database says 480p only, a Sonic
   wiki says the original Xbox has the setting). The SDK answers the dashboard's widescreen query from a setting;
   if your Xbox shows a widescreen option in Shadow, we keep it as a menu choice. Default 4:3 like Heroes.
5. Fixes in shared code (sdk/, switch/) that also help Heroes: note them here and in sheroes-nx's log for a later
   sheroes-nx update, rather than keeping the two trees in sync on every change.
