# Shadow-nx

**Shadow the Hedgehog** (original Xbox, USA, 2005) as a native Nintendo Switch homebrew program, statically
recompiled from the original Xbox executable. Not an emulator: the game's x86 code is translated to C++ and compiled
for the Switch, and Direct3D 8 and DirectSound are replaced by the Switch's own graphics (deko3d) and sound. Built on
sheroes-nx, the Sonic Heroes port, since both games share an engine.

You need your own copy of the game. No game files are in this repository or its releases.

## Status

Started 2026-11-16. Not playable yet: no release. Plan: [docs/design.md](docs/design.md). Session notes:
[docs/progress-log.md](docs/progress-log.md).

The supported `default.xbe` (USA, title 53450086) has this SHA-256:
`5b16d285d60542d70f9a1e7dc1590aed4d81a4cb87f920e17a5f10b67c07b42c`

## Credits and license

GPL-3.0 (`LICENSE`); the components and their licenses are listed in `LICENSES.txt` and `THIRD_PARTY.txt`.
Shadow the Hedgehog is (c) SEGA; this project has no relation to SEGA, Sonic Team, Microsoft or Nintendo.
