# Progress log (newest first)

sheroes-nx's log (the Sonic Heroes port this one starts from) is in `docs/sheroes-nx/progress-log.md`; read it
first, since most of its problems were in shared code and are already fixed here.

## 2026-11-16 (Switch tests 1 and 2)

- switch-test1: boots from the ISO, menus and FMV fine; in Westopolis Shadow white, surfaces missing and no ground
  past the first ramp (Shadow falls through). FMV at 20 fps (15 ms a frame uploading the movie texture).
- Movie skip: Start (Switch +) answers PLAYEND from mwPlyGetStat (0x340CC0); confirmed working in switch-test2.
- switch-test2 logged every disc read with an FNV hash: all 555 matched the ISO (tools: a checker on Mark's PC), so the
  data arrives intact. It froze on the stage's loading screen and crashed on guest thread 14 in a RenderWare vector
  insert (0x2B4010, vector at 0x559E90, memmove of 0x3FFFFFFF dwords: its end lay before the insert point).
- Theory (not proven): the guest threads share data the x86 way (plain stores and loads, one CPU on the Xbox), and on
  the Switch's three ARM cores another thread can see those stores out of order. The PC's x86 keeps them in order,
  which would explain why only the Switch breaks. switch-test3 runs every guest-code thread on core 0
  (guest_one_core, cores.cpp), the render thread on core 1, other host threads on 2.

## 2026-11-16 (later): first PC boot, black Shadow and blur

- First PC boot (Vulkan) reached Westopolis with no trap: 58 to 60 fps in the stage, GPU about 5 ms a frame. The
  dashboard's widescreen answer (1) gives a real 16:9 picture: the Xbox version supports widescreen.
- F3 capture (D3D12, out/capture/134315): Shadow draws its 3D scene into a 512x512 target (512x384 used, the game's
  own 3D resolution on the Xbox), adds a glow chain (512, 128, 64) and stretches the scene over the 640x480 back
  buffer, then draws the HUD.
- Blur: the stretch read the scene as a target "shrunk" into the back buffer because it is taller (512 > 480), so
  each tap became a 3 texel box (kTexWide) and the whole picture smeared. Shrinking now means smaller both ways
  (d3d12.cpp, vulkan.cpp, deko.cpp).
- Black Shadow: his skinned program lights oD0 = c95 * (c-91 ambient + light), and RenderWare (0x2C51C0) pushes
  c-91 and c95 as raw NV097_SET_TRANSFORM_CONSTANT_LOAD / SET_TRANSFORM_CONSTANT commands. Neither the push walker
  nor EndPush read them: the capture had c-91 = 0 0 0 1. Both now apply them (ApplyConstantMethod), and the
  SetVertexShaderConstant HLEs walk the push buffer first so the order matches the console. A scan of every
  immediate the game writes found no other raw state commands besides begin/end and inline vertices.
  Also for sheroes-nx: the same walker code is there.

## 2026-11-16: started from sheroes-nx 0.5.0

- Disc: Mark's trimmed XISO, base offset 0 (no video partition), 3,510 files, 2.54 GB. default.xbe taken out of it
  into `assets\xbe\` on his PC: title 53450086, entry 0x2AFB53, built 2005-10-16, every library 1.0.5849, SHA-256
  5b16d285d60542d70f9a1e7dc1590aed4d81a4cb87f920e17a5f10b67c07b42c. Files: .one, .txd, .dff, .adx, .afs, .sfd, the
  same engine as Sonic Heroes.
- Tree copied from sheroes-nx `xbe-recomp` 5abfcf1 and renamed: `shadow.nro`, `shadow-settings.toml`,
  `sdmc:/switch/shadow/`, `SHADOW_*` environment switches, title ID 53450086 in both game_files.cpp. Provenance
  comments (`sheroes-nx:`, `SHEROES (performance)`) kept. `docs/xboxrecomp-era/` left in sheroes-nx (only its
  feasibility study came along, in `docs/sheroes-nx/`); the unused Pages workflow dropped.
- game.toml from xbe_symbols with the SDK's 5849 signatures: 134 of 238 functions, 8 of 12 data (as Heroes). The
  seven functions Heroes named by hand are unnamed here too and were found at Shadow's addresses, each matching its
  5659 version and its XDK neighbours: BeginPush 0x3D4740, EndPush 0x3D4760, SetPalette 0x3D0E80,
  LoadVertexShaderProgram 0x3D1F00, IDirectSound_SetAllParameters 0x3E577A, IDirectSoundBuffer_SetPitch 0x3E4148,
  SetConeAngles 0x3E4932. 31 unnamed library functions remain, the kinds Heroes left (resource locks and setup,
  XGraphics, XPP); none touches hardware.
- Seed sweep before the first boot: 407 aligned pointers in .rdata/.data and 35 code immediates that begin a
  recompiled instruction (or follow padding after a function start) without being a function start, plus four
  ret-only vtable functions the recompiler never reached. 39,446 functions before, 39,925 after; overlapping
  decodes unchanged at 24. The recompiler runs clean in the cloud (3.7 s).
- `tools\win\build_pc.ps1` takes `assets\xbe\default.xbe`.
- Not built yet: the design (`docs/design.md`) is up for review with Mark first.
