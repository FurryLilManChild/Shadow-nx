// Hooks for default.xbe. A strong definition replaces the recompiled function at that address, for example:
//
// #include "xdk/hle.h"
//
// X86_EXTERN_FUNC(__imp__sub_00012345);
// X86_FUNC(sub_00012345) {
//     __imp__sub_00012345(ctx, base);  // the original
// }
//
// (street-nx's EA sound guard fix lived here; sheroes-nx had none.)
#include <atomic>
#include <chrono>

#include "core/input.h"
#include "core/log.h"
#include "gpu/backend.h"
#include "kernel/kernel.h"

// Shadow-nx: Start (Switch +) skips a movie. Shadow plays its story movies (E1001.sfd, the Westopolis opening, and
// the rest) through CRI Sofdec, and its movie manager ends a movie when mwPlyGetStat (0x340CC0) answers PLAYEND (3)
// or ERROR (4) (0x255A4, 0x24DFF, 0x24EB7, 0x24F17). With a skip asked for, a playing movie (2) answers PLAYEND, and
// the game ends it its own way. While a movie plays, Start goes to the skip and not to the game.
namespace shadow {
namespace {
using Clock = std::chrono::steady_clock;
std::atomic<int64_t> g_movie_seen{0};  // ns, the last time mwPlyGetStat answered PLAYING
std::atomic<bool> g_skip{false};
bool g_start_held = false;

int64_t Now() { return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count(); }
bool MoviePlaying() { return Now() - g_movie_seen.load() < 500'000'000; }
}  // namespace

// from ScriptPad (pad_script.cpp), every poll of player 1
void MovieSkipPad(input::Pad& pad) {
    // Shadow-nx, switch-test4: a click of the right stick (R3) saves a screenshot to screenshots/ beside the NRO and
    // does not reach the game
    static bool r3_held = false;
    bool r3 = pad.buttons & 0x80;
    if (r3 && !r3_held) {
        gpu::g_screenshot = true;
        LOGR("screenshot asked for (right stick click)");
    }
    r3_held = r3;
    pad.buttons &= uint16_t(~0x80u);
    bool start = pad.buttons & 16;
    if (MoviePlaying()) {
        if (start && !g_start_held && !g_skip.exchange(true)) LOGR("movie: skipped (Start)");
        pad.buttons &= uint16_t(~16u);
    } else {
        g_skip.store(false);  // no movie for half a second: the next one plays
    }
    g_start_held = start;
}
}  // namespace shadow

X86_EXTERN_FUNC(__imp__sub_00340CC0);
X86_FUNC(sub_00340CC0) {  // mwPlyGetStat(handle)
    __imp__sub_00340CC0(ctx, base);
    if (ctx.eax == 2) {
        shadow::g_movie_seen.store(shadow::Now());
        if (shadow::g_skip.load()) ctx.eax = 3;
    } else if (shadow::g_skip.load() && ctx.eax != 3) {
        shadow::g_skip.store(false);  // stopped: the next movie plays
    }
}

// Shadow-nx, switch-test5: the game's thread class (constructor 0x4F4F0: vtable at +0, "keep running" at +8, done
// at +A; CreateThread(0x4F440, this, suspended)) runs this loop: call vtbl[1] (one step) and yield while it answers
// true and +8 stays set, then vtbl[2] (finish) and mark done. Test 4: in the hung loads thread 34 never left it,
// in the good ones it did. Which class it is (its vtable and step function), and when and how it ends.
X86_EXTERN_FUNC(__imp__sub_0004F440);
X86_FUNC(sub_0004F440) {
    using namespace kernel;
    const uint32_t obj = R32(ctx.esp + 4);
    const uint32_t vt = obj ? R32(obj) : 0;
    Thread* t = Current();
    const uint32_t id = t ? t->id : 0;
    TRACE("trace: thread %X task loop: object %08X, vtable %08X, step %08X, finish %08X, flags %02X %02X %02X", id, obj,
         vt, vt ? R32(vt + 4) : 0, vt ? R32(vt + 8) : 0, obj ? G<uint8_t>(obj + 8) : 0, obj ? G<uint8_t>(obj + 9) : 0,
         obj ? G<uint8_t>(obj + 10) : 0);
    const int64_t t0 = shadow::Now();
    __imp__sub_0004F440(ctx, base);
    TRACE("trace: thread %X task loop ends after %.2f s: object %08X, vtable now %08X, flags %02X %02X %02X", id,
         double(shadow::Now() - t0) / 1e9, obj, obj ? R32(obj) : 0, obj ? G<uint8_t>(obj + 8) : 0,
         obj ? G<uint8_t>(obj + 9) : 0, obj ? G<uint8_t>(obj + 10) : 0);
}
