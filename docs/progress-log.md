# Progress log (newest first)

sheroes-nx's log (the Sonic Heroes port this one starts from) is in `docs/sheroes-nx/progress-log.md`; read it
first, since most of its problems were in shared code and are already fixed here.

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
