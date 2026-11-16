// Where Horizon runs each thread (core_layout = true in the settings), as sa2-nx's core layout does: the title thread
// alone on core 0, the render thread on core 1, every other thread (guest threads, audio, the shader compiler) on cores
// 1 and 2, preferring 2. Without it every thread has mask 7 at the same priority, and the title thread, the frame's
// bottleneck, shares its core with whatever woke up. Each thread sets its own mask: setting another running thread's
// can wait for it to leave its core, and with busy loader threads hang (sa2-nx's emulator runs).
#include <switch.h>

#include "core/log.h"
#include "host.h"

// role: 0 the title thread (the one that presents), 1 the render thread, 2 any other host thread, 3 a thread that
// runs guest code (kernel/threads.cpp)
//
// Shadow-nx: guest_one_core (on by default) puts every thread that runs guest code on core 0 alone, as the Xbox ran
// them all on its one CPU, and keeps the render thread on core 1 and the other host threads on core 2. The title's
// code assumes one x86 CPU: its threads hand data to each other with plain stores and loads, which an x86 keeps in
// order but the Switch's ARM cores may see out of order when the threads run on different cores. switch-test2
// (2026-11-16) crashed in a RenderWare vector (0x559E90, grown at 0x2B4010) whose end lay before its insert point, after
// streaming Westopolis' parts, every one of which had arrived intact from the disc; switch-test1 lost the ground and
// surfaces past the first ramp. One core gives the guest threads the console's view of memory.
extern "C" void shadow_place_thread(int role) {
    if (role < 0 || role > 3) return;
    const bool one = host::GuestOneCore();
    if (!one && !host::CoreLayout()) return;
    struct Place {
        s32 core;
        u32 mask;
        const char* name;
    };
    static constexpr Place kLayout[] = {{0, 0b001, "title"}, {1, 0b010, "render"}, {2, 0b110, "other"}, {2, 0b110, "guest"}};
    static constexpr Place kOneCore[] = {{0, 0b001, "title"}, {1, 0b010, "render"}, {2, 0b110, "other"}, {0, 0b001, "guest"}};
    const Place& p = one ? kOneCore[role] : kLayout[role];
    Result rc = svcSetThreadCoreMask(CUR_THREAD_HANDLE, p.core, p.mask);
    if (role != 2 || R_FAILED(rc))
        LOGR("threads: %s thread to core %d (mask %X)%s", p.name, int(p.core), unsigned(p.mask), R_FAILED(rc) ? " refused" : "");
}
