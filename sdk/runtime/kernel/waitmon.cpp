// Shadow-nx: the wait monitor (waitmon.h). Written for the stuck stage load of switch-test3 to 3c (2026-11-16), where the
// sampling profiler could only show the busy thread: the nine loader threads were blocked, and nothing said on what.
#include "kernel/waitmon.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "core/log.h"

namespace kernel::waitmon {
namespace {

using Clock = std::chrono::steady_clock;
int64_t NowNs() { return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count(); }
const int64_t g_t0 = NowNs();
double Secs(int64_t ns) { return double(ns - g_t0) / 1e9; }

constexpr int kChain = 8;
constexpr int kMaxObjs = 4;

struct Info {
    uint32_t id = 0, ethread = 0;
    X86Context* ctx = nullptr;
    std::mutex lock;  // guards the wait description below
    int state = 0;    // 0 running, 1 waiting, 2 delaying, 3 exited
    uint32_t exit_code = 0;
    int64_t exited_at = 0;
    int64_t since = 0;
    const char* kind = "wait";
    uint32_t detail = 0;
    std::weak_ptr<Object> objs[kMaxObjs];
    int nobjs = 0;
    bool all = false;
    int64_t timeout = 0;  // 100 ns, INT64_MAX infinite
    uint32_t chain[kChain] = {};
    int nchain = 0;
    std::atomic<uint64_t> yields{0}, waits{0}, delays{0};
    uint64_t last_yields = 0, last_waits = 0, last_delays = 0;
    const char* hint = nullptr;
    uint32_t hint_detail = 0;
};

std::mutex g_lock;
std::vector<std::unique_ptr<Info>> g_infos;
thread_local Info* t_info = nullptr;

// the image's executable sections, read from the XBE header the loader placed at 0x10000
std::vector<std::pair<uint32_t, uint32_t>> g_code;
std::once_flag g_code_once;
void FindCode() {
    std::call_once(g_code_once, [] {
        constexpr uint32_t kBase = 0x10000;
        if (R32(kBase) != 0x48454258u) return;  // "XBEH"
        uint32_t n = R32(kBase + 0x11C), at = R32(kBase + 0x120);
        for (uint32_t i = 0; i < n && i < 64; i++) {
            uint32_t s = at + i * 0x38;
            if (R32(s) & 4) g_code.push_back({R32(s + 4), R32(s + 4) + R32(s + 8)});
        }
    });
}
bool IsCode(uint32_t va) {
    for (auto& [lo, hi] : g_code)
        if (va >= lo && va < hi) return true;
    return false;
}
// a return address: code right after a call (E8 rel32, or FF /2 through a register or memory)
bool AfterCall(uint32_t ra) {
    if (!IsCode(ra) || !IsCode(ra - 7)) return false;
    if (G<uint8_t>(ra - 5) == 0xE8) return true;
    for (int len = 2; len <= 7; len++)
        if (G<uint8_t>(ra - len) == 0xFF && ((G<uint8_t>(ra - len + 1) >> 3) & 7) == 2) return true;
    return false;
}
// stack_top: the thread's StackBase; the scan stops there. Shadow-nx, switch-test5: the PC crashed reading past the
// top of a fresh thread's stack (thread 10 creating a thread 0x10C bytes below its top, the scan going 2 KB up)
int Chain(const X86Context* ctx, uint32_t* out, uint32_t stack_top = 0) {
    if (!ctx) return 0;
    FindCode();
    int n = 0;
    uint32_t esp = ctx->esp;
    for (uint32_t i = 0; i < 512 && n < kChain; i++) {
        if (stack_top && esp + i * 4 + 4 > stack_top) break;
        uint32_t v = R32(esp + i * 4);
        if (AfterCall(v)) out[n++] = v;
    }
    return n;
}

Info* Me() {
    if (t_info) return t_info;
    Thread* t = Current();
    auto info = std::make_unique<Info>();
    if (t) {
        info->id = t->id;
        info->ethread = t->ethread;
        info->ctx = t->ctx;
    }
    t_info = info.get();
    std::lock_guard<std::mutex> lock(g_lock);
    g_infos.push_back(std::move(info));
    return t_info;
}

const char* TypeName(ObjType t) {
    switch (t) {
        case ObjType::Event: return "event";
        case ObjType::Semaphore: return "semaphore";
        case ObjType::Mutant: return "mutant";
        case ObjType::Timer: return "timer";
        case ObjType::Thread: return "thread";
        case ObjType::File: return "file";
        case ObjType::Queue: return "queue";
        default: return "object";
    }
}

std::string ChainText(const uint32_t* c, int n) {
    std::string s;
    char b[16];
    for (int i = 0; i < n; i++) {
        std::snprintf(b, sizeof b, "%s%06X", i ? " < " : "", c[i]);
        s += b;
    }
    return s.empty() ? "?" : s;
}

uint32_t IdOfEthread(uint32_t ethread) {
    if (!ethread) return 0;
    for (auto& i : g_infos)
        if (i->ethread == ethread) return i->id;
    return 0;
}

void Dump(int number, int64_t now) {
    std::lock_guard<std::mutex> lock(g_lock);
    LOGR("waitmon: dump %d at %.1f s, %zu threads that ran guest code", number, Secs(now), g_infos.size());
    for (auto& ip : g_infos) {
        Info& i = *ip;
        uint64_t y = i.yields.load(), w = i.waits.load(), d = i.delays.load();
        std::lock_guard<std::mutex> il(i.lock);
        if (i.state == 3) {
            LOGR("waitmon:   thread %X exited with %08X at %.1f s", i.id, i.exit_code, Secs(i.exited_at));
        } else if (i.state == 0) {
            uint32_t c[kChain];
            int n = Chain(i.ctx, c, i.ethread ? R32(i.ethread + 0x1C) : 0);  // live: approximate
            LOGR("waitmon:   thread %X running; since the last dump %llu yields, %llu waits, %llu delays; stack %s", i.id,
                 (unsigned long long)(y - i.last_yields), (unsigned long long)(w - i.last_waits),
                 (unsigned long long)(d - i.last_delays), ChainText(c, n).c_str());
        } else if (i.state == 2) {
            LOGR("waitmon:   thread %X delaying %.1f s (interval %.1f ms), from %s", i.id, double(now - i.since) / 1e9,
                 double(-i.timeout) / 1e4, ChainText(i.chain, i.nchain).c_str());
        } else {
            std::string what;
            for (int k = 0; k < i.nobjs; k++) {
                auto o = i.objs[k].lock();
                char b[256];
                if (!o) {
                    std::snprintf(b, sizeof b, "%s[gone]", k ? ", " : "");
                } else {
                    uint32_t guest_state = o->guest ? R32(o->guest + 4) : 0xFFFFFFFFu;
                    int64_t ls = o->last_signal_ns.load();
                    char sig[96];
                    if (ls)
                        std::snprintf(sig, sizeof sig, "last signalled by thread %X at %.2f s (%s), %u times", o->last_signal_thread.load(),
                                      Secs(ls), o->last_signal_how ? o->last_signal_how : "?", o->signal_count.load());
                    else
                        std::snprintf(sig, sizeof sig, "never signalled by the kernel");
                    std::snprintf(b, sizeof b, "%s%s %08X%s%s host %d guest %d owner %X; %s", k ? ", " : "", TypeName(o->type),
                                  o->guest, o->notification ? " (manual)" : "", o->name.empty() ? "" : (" " + o->name).c_str(),
                                  o->signal, int32_t(guest_state), IdOfEthread(o->owner), sig);
                }
                what += b;
            }
            LOGR("waitmon:   thread %X waiting %.1f s in %s%s%s on %s; timeout %s; from %s", i.id, double(now - i.since) / 1e9,
                 i.kind, i.detail ? " " : "", i.detail ? (std::to_string(i.detail)).c_str() : "", what.c_str(),
                 i.timeout == INT64_MAX ? "none" : (std::to_string(double(i.timeout < 0 ? -i.timeout : i.timeout) / 1e4) + " ms").c_str(),
                 ChainText(i.chain, i.nchain).c_str());
            if (!std::strcmp(i.kind, "critical section"))
                LOGR("waitmon:     critical section %08X: lock count %d, owner thread %X, recursion %u", i.detail,
                     int32_t(R32(i.detail + 0x10)), IdOfEthread(R32(i.detail + 0x18)), R32(i.detail + 0x14));
        }
        i.last_yields = y;
        i.last_waits = w;
        i.last_delays = d;
    }
}

void Watchdog() {
    constexpr int64_t kLong = 5'000'000'000, kEvery = 15'000'000'000, kSummary = 60'000'000'000;
    constexpr int kMaxDumps = 40;
    int dumps = 0;
    int64_t last_dump = 0, last_summary = NowNs();
    for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        int64_t now = NowNs();
        bool stall = false;
        {
            std::lock_guard<std::mutex> lock(g_lock);
            for (auto& ip : g_infos) {
                std::lock_guard<std::mutex> il(ip->lock);
                if (ip->state == 1 && now - ip->since > kLong && ip->timeout == INT64_MAX) stall = true;
            }
        }
        // a long infinite wait, at most every 15 s; else one picture a minute to compare with
        if (dumps < kMaxDumps && ((stall && now - last_dump >= kEvery) || now - last_summary >= kSummary)) {
            Dump(++dumps, now);
            last_dump = now;
            last_summary = now;
        }
    }
}

}  // namespace

void Hint(const char* kind, uint32_t detail) {
    if (!g_on) return;
    Info* i = Me();
    i->hint = kind;
    i->hint_detail = detail;
}

void BeginWait(const std::vector<ObjectRef>& objs, bool wait_all, int64_t timeout) {
    if (!g_on) return;
    Info* i = Me();
    uint32_t c[kChain];
    int n = Chain(i->ctx, c, Current() ? Current()->stack_top : 0);
    std::lock_guard<std::mutex> lock(i->lock);
    i->state = 1;
    i->since = NowNs();
    i->kind = i->hint ? i->hint : "wait";
    i->detail = i->hint ? i->hint_detail : 0;
    i->hint = nullptr;
    i->nobjs = int(std::min<size_t>(objs.size(), kMaxObjs));
    for (int k = 0; k < i->nobjs; k++) i->objs[k] = objs[size_t(k)];
    i->all = wait_all;
    i->timeout = timeout;
    std::copy(c, c + n, i->chain);
    i->nchain = n;
}

void EndWait(uint32_t) {
    if (!g_on) return;
    Info* i = Me();
    i->waits++;
    std::lock_guard<std::mutex> lock(i->lock);
    i->state = 0;
}

void BeginDelay(int64_t interval) {
    if (!g_on) return;
    Info* i = Me();
    uint32_t c[kChain];
    int n = Chain(i->ctx, c, Current() ? Current()->stack_top : 0);
    std::lock_guard<std::mutex> lock(i->lock);
    i->state = 2;
    i->since = NowNs();
    i->timeout = interval;
    std::copy(c, c + n, i->chain);
    i->nchain = n;
}

void EndDelay() {
    if (!g_on) return;
    Info* i = Me();
    i->delays++;
    std::lock_guard<std::mutex> lock(i->lock);
    i->state = 0;
}

std::string CallerChain(const X86Context& ctx) {
    uint32_t c[kChain];
    Thread* t = Current();
    int n = Chain(&ctx, c, t ? t->stack_top : 0);
    return ChainText(c, n);
}

void Exited(uint32_t code) {
    if (!g_on) return;
    Info* i = Me();
    std::lock_guard<std::mutex> il(i->lock);
    i->state = 3;
    i->exit_code = code;
    i->exited_at = NowNs();
}

void Yielded() {
    if (g_on) Me()->yields++;
}

void Signalled(Object& o, const char* how) {
    if (!g_on) return;
    Thread* t = Current();
    o.last_signal_ns = NowNs();
    o.last_signal_thread = t ? t->id : 0;
    o.last_signal_how = how;
    o.signal_count++;
}

void Start() {
    if (!g_on) return;
    LOGR("waitmon: on (dumps when a wait without timeout lasts 5 s, and once a minute)");
    std::thread(Watchdog).detach();
}

}  // namespace kernel::waitmon
