# Shadow-nx

**Shadow the Hedgehog** (original Xbox, USA, 2005) as a native Nintendo Switch homebrew program, statically
recompiled from the original Xbox executable. Not an emulator: the game's x86 code is translated to C++ and compiled
for the Switch, and Direct3D 8 and DirectSound are replaced by the Switch's own graphics (deko3d) and sound. Built on
sheroes-nx, the Sonic Heroes port, since both games share an engine.

You need your own copy of the game. No game files are in this repository or its releases.

## Status: 0.5.0

Feature ready, not yet played through to the end. Tested through the first three stages with their cutscenes and
loading screens, on an unpatched V1 Switch at stock clocks.

- Gameplay runs at about 50 to 60 fps at stock clocks, with dips into the mid 40s in the busiest spots.
- In-engine cutscenes run at 30 fps.
- The opening movie and other pre-rendered movies can be skipped with Start. In-engine cutscenes cannot be skipped yet.

## Installing

1. Copy the `switch/shadow` folder from the release to the root of your SD card, so you have
   `sdmc:/switch/shadow/shadow.nro`.
2. Put your game in `sdmc:/switch/shadow/game/`: either a disc image (`.iso`, a trimmed XISO works) or the files of
   the disc (`default.xbe` with all its folders).
3. Launch it through title takeover: hold R while opening any game, then pick Shadow-nx in the Homebrew Menu. The
   Album applet does not have enough memory for it.

The supported `default.xbe` (USA, title 53450086) has this SHA-256:
`5b16d285d60542d70f9a1e7dc1590aed4d81a4cb87f920e17a5f10b67c07b42c`

The release includes a `shadercache` folder with every shader seen in testing already compiled for the Switch, so
effects do not stutter the first time they appear. A shader the game uses that is not in it is compiled the first
time it is drawn (a short stutter, once) and kept for later.

## Controls and settings

Press + and - together for the port menu: button layout, camera inversion, steering response, resolution, sharp 3D
scene, texture filtering, smoothing (SMAA), the FPS counter, music and effects volume, and a CPU boost. It saves to
`sdmc:/switch/shadow/shadow-settings.toml`, which also explains each setting.

Logs go to `sdmc:/switch/shadow/logs/`. The first line of `shadow.log` names the build and its commit.

## Known issues

- Not 60 fps everywhere at stock clocks yet.
- In-engine cutscenes cannot be skipped.
- Only the first three stages have been tested.

## Credits and license

GPL-3.0 (`LICENSE`); the components and their licenses are listed in `LICENSES.txt` and `THIRD_PARTY.txt`.

Built on TGP482's XboxXBE-Recomp, by way of xsaja's brextreme-nx and the street-nx and mvp-nx ports, whose Switch layer
comes from gerisson1996's BLACK-NX-PORT; and on sheroes-nx (Sonic Heroes). Session notes:
[docs/progress-log.md](docs/progress-log.md).

Shadow the Hedgehog is (c) SEGA; this project has no relation to SEGA, Sonic Team, Microsoft or Nintendo.
