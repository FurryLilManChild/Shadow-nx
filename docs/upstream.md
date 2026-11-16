# Upstreams

| Project | Commit or version | License | What we take |
|---|---|---|---|
| FurryLilManChild/sheroes-nx, branch `xbe-recomp` | 5abfcf1 (0.5.0, 2026-11-14) | GPL-3 | the whole tree: SDK with its fixes, Switch host, port menu, build scripts |
| TGP482/XboxXBE-Recomp | as carried in sheroes-nx's `sdk/` | MIT (`SDK-LICENSE.txt`) | xbe_recomp, xbe_symbols, runtime, XDK 5849 signatures and layout |
| xsaja/brextreme-nx | 82d8054 (2026-11-03), via sheroes-nx | GPL-3 | Switch host (deko3d, Vulkan, audio, input, profiler, PGO); notes in `docs/brextreme/` |
| gerisson1996/BLACK-NX-PORT | via brextreme-nx | none published (Mark: fine to build on) | the Switch layer under brextreme-nx |
| third_party/ (zydis, tomlplusplus, imgui, SDL3, glslang, SPIRV-Cross, Vulkan-Headers, smaa, uam) | as vendored by sheroes-nx | their own (`THIRD_PARTY.txt`) | libraries |

Looked at, not used:

- m0ester/StHDecomp: a GameCube decomp of Shadow the Hedgehog (USA), 3 commits, no license published. Reading only,
  never copying.
- No Xbox recompilation or Switch port of Shadow the Hedgehog was found (2026-11-16).
