# Shadow-nx: notes for Claude

Native PC and Switch port of Shadow the Hedgehog (original Xbox, NTSC-U, title 53450086, XDK 5849), started on
2026-11-16 from sheroes-nx's `xbe-recomp` branch at 0.5.0 (commit 5abfcf1, Sonic Heroes, 60 fps at stock clocks on
the Switch). That tree is TGP482's XboxXBE-Recomp (`sdk/`, MIT) by way of xsaja/brextreme-nx (GPL-3), on street-nx
and mvp-nx, whose Switch layer comes from gerisson1996/BLACK-NX-PORT (no license; Mark: fine to build on). The x86
code is translated to C++ (`xbe_recomp`), and Direct3D 8 and DirectSound are replaced at the API level
(`sdk/runtime/xdk`): a render thread replays recorded draws with deko3d or Vulkan.

Shadow runs on the same engine as Sonic Heroes (RenderWare, .one archives, .txd, CRI ADX and Sofdec), so
sheroes-nx is the main upstream this time: read `docs/sheroes-nx/progress-log.md` before chasing a problem, and note
fixes that belong in sheroes-nx too. Plan and open decisions: `docs/design.md`. Upstreams: `docs/upstream.md`.

License: GPL-3 (`LICENSE`), with the components listed in `LICENSES.txt`.

Comments marked `// STREET:`, `// BREXTREME:` and `sheroes-nx:` say which port made a change; keep them. Mark this
port's changes `Shadow-nx:`.

## Who is working on this

The owner (Mark) is very tech savvy but has almost no coding experience. Claude writes the code and scripts. Mark
runs builds on his PC, tests on the PC and his Switch, and sends logs. PowerShell commands go in one code block, one
command per line. Never use em dashes in anything written for him or in the docs. No decorative emoji.

His hardware: a Windows PC (Lenovo Legion Go), an unpatched V1 Switch on Atmosphere CFW (sys-clk), an Xbox 360 with
Aurora, an original Xbox. His disc image is `C:\Shadow-nx\assets\Shadow the Hedgehog (USA) (En,Ja,Fr,De,Es,It).xiso.iso`
(a trimmed XISO, 2.54 GB, 3,510 files). `C:\Shadow-nx\assets\xbe\default.xbe` is the XBE taken out of it (kept out
of `assets\` itself, where the PC runtime would take a lone default.xbe for an extracted disc ahead of the .iso).
A cloud session reaches the folder through the device bridge. He moves files to the Switch's SD card himself; he
drops Switch logs in `C:\Shadow-nx\out\sd\switch\switchlogs`.

Workflow: Mark builds and runs the PC exe, Claude reads the logs and pushes fixes; the Switch NRO may be built in
the cloud. Present findings and the next build's plan, and wait for his go-ahead before each test build. Do not
start cloud runs of the game on your own. Goal: 60 fps at stock clocks.

Git: commit as `FurryLilManChild <346934328+FurryLilManChild@users.noreply.github.com>`. History rewrites and force
pushes need his explicit consent.

## Hard rules

- Never commit game data or anything derived from it: the disc, `default.xbe`, `assets/`, `game/assets/`,
  `game/generated/`, `out/`, frame dumps, shader caches. Check `git status` before every commit.
- Every hand-named XDK function in `game/config/game.toml` carries its evidence (the 5849 address and instruction).
- Log each session in `docs/progress-log.md` (newest first).

## Layout

| Where | What |
|---|---|
| `sdk/` | XboxXBE-Recomp: `tools/recomp` (xbe_recomp), `tools/symbols` (xbe_symbols), `runtime/` (kernel, D3D8, DSound, render thread), `xdk/<build>/` (signatures and layouts; 5849 is the SDK's own build) |
| `game/config/game.toml` | the recompiler's input for Shadow: sections, seeds, hand-named functions, `[symbols]` |
| `game/src/` | game hooks (`X86_FUNC(sub_...)` replaces a recompiled function) and `game_info.cpp` |
| `pc/` | the Windows host (D3D12 and Vulkan exes) |
| `switch/` | the Switch host (deko3d and Vulkan renderers, audio, input, port menu, profiler, PGO) |
| `tools/win/build_pc.ps1` | the Windows build: tools, recompile into game/generated, PC exes |
| `switch/build.sh` | the Switch NRO |

## The game

- `default.xbe`: base 0x00010000, entry 0x002AFB53, built 2005-10-16. Sections .text (0x11000, 3.9 MB), D3D
  (0x3CFC40), DSOUND (0x3E3220), PSFD00 (CRI Sofdec), D3DX, XGRPH, XPP, .rdata, .data, DOLBY. Libraries XAPILIB, D3D8,
  DSOUND, XBOXKRNL, LIBCPMT, LIBCMT, D3DX8, XGRAPHC, all 1.0.5849. SHA-256
  5b16d285d60542d70f9a1e7dc1590aed4d81a4cb87f920e17a5f10b67c07b42c.
- The D3D device pointer is at 0x3E0A18.
- Switch paths: `sdmc:/switch/shadow/` (`shadow.nro`, `shadow-settings.toml`, `logs/`), the game (the .iso, or the
  extracted disc) in `sdmc:/switch/shadow/game/`.
