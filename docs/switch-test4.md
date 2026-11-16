# switch-test4: the diagnostic build (2026-11-16)

Built while Mark travels; not run yet. NRO sha256 198e4a1822d859a30b9133513de9d62c5c3b9288887e52d03822250ef16d80b5
(107.7 MB: every guest memory access is compiled both ways for the memory order setting). ELF kept as
shadow-switch-test4.elf.

## What we know and what we do not

Known from the logs: on three cores (test 1) the game reaches Westopolis with Shadow white, surfaces missing and no
ground past the first ramp; the PC draws and plays the same stage right; every byte the Switch read from the disc
matched the ISO (test 2). Test 2 crashed in a corrupted RenderWare vector while loading; with every guest thread on
one core (tests 3 to 3c) the stage never finishes loading: the loading screen's thread spins on NtYieldExecution
while the nine loader threads, at the same priority, stay blocked on something unknown.

Not known: why. Theories (each tested below, none assumed): memory order between cores (the Switch's ARM cores may
see another core's stores out of order, an x86 never does); a Switch-only render path (white Shadow and missing
surfaces could be drawing problems separate from the missing collision); for the one-core hang, a wait that is never
satisfied.

## What the build records

- Wait monitor (wait_monitor, on): every thread that runs game code reports its kernel waits (objects, kind, since
  when, the game's return addresses from its stack), delays and yields; every kernel signal is stamped (who, when,
  how, how often). The log gets the whole picture when a wait without timeout lasts 5 s, and once a minute anyway,
  including for each waited object the host signal state against the one in guest memory.
- Memory order (x86_memory_order, port menu "Memory order: Switch / x86", in force at once): every game load
  acquires and every store releases, the order an x86 keeps.
- Render path cycle in the first minute of each stage (up to 3 stages): 8 s each with one Switch-only render feature
  off (packed vertices, native fans and quads, texture checks every 8th frame, state tracking, constant reuse), then
  all five off, a screenshot of each step to screenshots/.
- Right stick click: a screenshot (logged; the click does not reach the game).
- Profiler on (profile = true): where busy threads spend their time.
- The last five runs' logs and profiles kept (shadow.1.log to shadow.5.log, prof.1.bin...).
- Game threads on one core: port menu row, from the next start (default off).

## The runs (one sitting, nothing to copy between runs)

1. Normal settings: story, skip the FMV, into Westopolis, play or fall for about a minute. Answers: is it a render
   setting (screenshots per step), and a baseline wait picture.
2. Port menu: Memory order x86 (before starting the story), then the same. Answers: does x86 memory order bring back
   the ground, surfaces and Shadow's colours.
3. Port menu: Memory order Switch, Game threads on one core On; quit, start again, story, wait 30 s on the stuck
   loading screen, quit. Answers: what each loader thread waits on, and who should have signalled it.

Then copy switch/shadow/logs and switch/shadow/screenshots into out/sd/switch/switchlogs.
