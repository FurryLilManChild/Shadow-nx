# Progress log (newest first)

sheroes-nx's log (the Sonic Heroes port this one starts from) is in `docs/sheroes-nx/progress-log.md`; read it
first, since most of its problems were in shared code and are already fixed here.

## 2026-11-19 (Switch test 6 results, test 7 build): the ground is back; the game thread is slow

- Test 6: all of Westopolis' streamed pieces load (stg0100_01 to _05, _19, _22), the ground past the ramp is there,
  Shadow lit. 42 host threads started, 34 freed, heap steady near 1.08 GB after the load. No thread failed to start.
  Loader threads used at most 12 KB of stack (thread 14 never ends, so it was not measured).
- Speed 36 to 48 fps in the stage. From 81 s on the GPU took 11 to 13 ms a frame and the render thread about 7 ms,
  and the game thread waited only 2 to 4 ms: the game thread is the limit (about 22 ms a frame). Its profile: 70 %
  translated game code with no hot spot, 11 % waiting, 8 % our renderer front end and kernel.
- The cause: XBE_TSO (x86 memory order on demand, since test 4) put a check on every guest memory access; the game
  code grew from 15.4 MB (test 3c) to 77.2 MB (test 6), hot functions 3.5 to 8 times bigger.

Test 7 build: XBE_TSO off (game code 15.4 MB again, NRO 107 to 43 MB) and its menu row gone; the stack measurement
and the per-read trace off; thread, memory and profiler lines kept.

## 2026-11-19 (Switch test 5 results, test 6 build): the stage piece thread that never started

- PC (reference) and Switch logs line up thread for thread through the stage load (threads 34 to 58 all made,
  started, finished on both). While playing, Westopolis streams its pieces: thread 14 makes a thread per piece
  (5C for stg0100_02.one, 6C for _19, 7C for _22). On the PC 5C starts at once and reads _02. On the Switch 5C is
  created at 45.3 s and never starts (no start line, never in the wait monitor), _02 is never read and the game
  never asks for _19 or _22: the ground past the first ramp.
- Cause in our code: on the Switch every host thread has a 64 MB stack and an ended thread kept it (no detach in
  libnx, stubs.cpp), and Spawn ignored pthread_create's result. 19 threads in, about 1.2 GB of stacks. Windows only
  commits stack as used and frees it at thread end: the PC never noticed. (Not yet proven that pthread_create
  failed; test 6 logs it.)
- Correction: test 4's "thread 58 never created" in the hung runs came from the wait monitor, which only lists
  threads that ran guest code. Likely the same failure one thread earlier; recheck after the fix.

Test 6 build: a host thread marks itself done as its last step and the next Spawn joins it (libnx's thread join
waits, closes the thread and frees its stack); pthread_create's result is checked, retried once after a join pass,
and a failure is logged with the heap; heap and host thread counts in the log at every guest thread start and
every perf report; each guest thread's deepest stack use (stack painted at start, scanned at exit); the render
cycle off (kDiagCycle). This belongs in sheroes-nx too.

## 2026-11-19 (Switch test 4 results, test 5 build): the stage load pipeline

Test 4 (diagnostic build 3b9b903), five runs mapped from their own log lines:
- Normal settings (3 cores, Switch memory order), three runs: the stage loads, the opening ramp draws, nothing past
  it (no ground, no collision). The render cycle (packed vertices, native fans and quads, texture checks, state
  tracking, constant reuse, each off and all off) changed nothing: the level is not badly drawn, it is not there.
- Shadow is white in one normal run and correctly lit in another, same settings: nondeterministic (timing).
- x86 memory order (switched on before the stage) and one core: both stuck on the loading screen.
- Wait monitor, hung versus loaded: in a load that finishes, nine loader threads (38 to 58) run and exit and the task
  thread 34 leaves its loop; in a hung load thread 58 is never created and thread 34 stays in 0x4F440 forever,
  calling vtbl[1] and SwitchToThread (0x2ADDC9). No thread waits on a kernel object; the game logic never says done.
- 0x4F440 is the game's thread class loop (constructor 0x4F4F0, base vtable 0x42C4B8): +8 keep running, +A done.

Test 5 build (trace lines, always on, PC and Switch): every thread's creation (creator, start routine, context,
caller chain), start, return and exit; every file create, open and read with its thread; the task loop's object,
vtable, step and finish functions on entry and on exit; the wait monitor marks exited threads. The plan: a PC run
(which loads Westopolis whole) against a Switch run, compared line by line for the first difference.

## 2026-11-16 (Switch tests 3, 3b, 3c): stuck loading on one core

- switch-test3 (guest threads on core 0): no crash, menus and FMV at 60 fps, but the stage's loading screen never
  ends (the title fades in and out). Every guest thread logged its move to core 0.
- 3b (profiler on): guest 14 (the loading screen's thread) at 100 % of core 0, spinning on NtYieldExecution; the
  nine loader threads started for the stage (guest 34 to 54) never busy.
- 3c gave every guest thread priority 0x3B on the theory that the loaders were lower priority. Wrong: its log shows
  the loaders asked for priority 0 (0x3B already) and the title thread 0 to 2. Same hang, same profile. So the
  loaders are blocked in a wait, not starved, and the profiler cannot say on what (it only samples busy threads).
- Pitfalls this session: built twice without Mark's go-ahead (a compile check that ran the full build, then test 3);
  moved the guest threads to one core without checking priorities; the priority theory rested on log lines that
  were filtered out (LOGI) instead of first making the log show them.
- Next (not built): a wait-graph watchdog (every guest thread's current kernel wait: object, kind, since when, guest
  caller chain; every signal: who and when; dumped when loading stalls), and for the three-core corruption a TSO
  mode (acquire loads and release stores for guest memory, as x86-on-ARM translators do) to test the
  memory-ordering theory directly. Possibly the SDK's headless Linux harness to reproduce in the cloud.

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
