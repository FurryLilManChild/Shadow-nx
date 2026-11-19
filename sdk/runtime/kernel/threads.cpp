#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "kernel/kernel.h"
#include "kernel/waitmon.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif
#ifdef __SWITCH__
#include <malloc.h>
#include <unistd.h>
#include <switch.h>

#include <cstring>
#include <thread>
extern "C" char* fake_heap_end;  // libnx: the end of newlib's heap
#endif

namespace kernel {

namespace {

#ifdef __SWITCH__
// Shadow-nx, switch-test6: host threads give their stacks back. libnx has no detach, so a thread that ended kept its
// 64 MB stack until the process quit (stubs.cpp). Shadow makes a thread for every stage piece it streams in;
// switch-test5 created thread 5C (stg0100_02.one, the ground past Westopolis' first ramp) 19 threads in, and it never
// started: no memory left for its stack. A thread that ends now says so as its very last step, and the next Spawn
// joins it, which frees its stack and handle.
// Shadow-nx, switch-test10: the stacks themselves are pooled. Freed to the heap, a 64 MB stack was soon split by
// small allocations and the next thread took a fresh 64 MB from the top: switch-test9 had 2 GB free in pieces and
// 14 MB unclaimed at 597 s, and threads could no longer start (error 12). Threads are made with libnx directly on a
// stack from this pool; a joined thread's stack goes back to the pool, never to the heap. The pool grows to the
// most threads alive at once.
constexpr size_t kHostStack = 64u << 20;
struct HostThread {
    ::Thread t{};
    void* mem = nullptr;
    std::function<void()>* fn = nullptr;
};
std::mutex g_done_lock;
std::vector<HostThread*> g_done;
std::vector<void*> g_free_stacks;  // under g_done_lock
std::atomic<uint32_t> g_live{0}, g_spawned{0}, g_reaped{0}, g_pool{0};
std::atomic<size_t> g_stack_max{0};
thread_local HostThread* t_host = nullptr;

void HostThreadDone() {
    if (!t_host) return;
    std::lock_guard<std::mutex> lock(g_done_lock);
    g_done.push_back(t_host);
}

void Reap() {
    std::vector<HostThread*> done;
    {
        std::lock_guard<std::mutex> lock(g_done_lock);
        done.swap(g_done);
    }
    for (HostThread* h : done) {
        threadWaitForExit(&h->t);
        threadClose(&h->t);
        {
            std::lock_guard<std::mutex> lock(g_done_lock);
            g_free_stacks.push_back(h->mem);
        }
        delete h;
        g_reaped++;
        g_live--;
    }
}

void* TakeStack() {
    {
        std::lock_guard<std::mutex> lock(g_done_lock);
        if (!g_free_stacks.empty()) {
            void* m = g_free_stacks.back();
            g_free_stacks.pop_back();
            return m;
        }
    }
    void* m = memalign(0x1000, kHostStack);
    if (m) g_pool++;
    return m;
}

void GiveBackStack(void* m) {
    std::lock_guard<std::mutex> lock(g_done_lock);
    g_free_stacks.push_back(m);
}

// Shadow-nx, switch-test6: how deep each guest thread's stack really goes, to size stacks from data later. The
// stack is filled with a pattern when the thread starts; at its end the first changed word from the bottom up is
// the deepest point it reached.
constexpr uint64_t kPaint = 0xCDCDCDCDCDCDCDCDull;
// on for switch-test6 only: the loader threads used 12 KB at most, and the paint and scan cost up to 45 ms a thread
constexpr bool kMeasureStacks = false;
thread_local uint8_t* t_stack_lo = nullptr;
thread_local size_t t_stack_sz = 0;

void PaintStack() {
    if (!kMeasureStacks) return;
    ::Thread* self = threadGetSelf();
    if (!self || !self->stack_mirror || !self->stack_sz) return;
    uint8_t* lo = static_cast<uint8_t*>(self->stack_mirror);
    uint8_t* sp = static_cast<uint8_t*>(__builtin_frame_address(0));
    if (sp < lo + 0x10000 || sp > lo + self->stack_sz) return;  // not on the stack libnx reports: measure nothing
    std::memset(lo, 0xCD, size_t(sp - 0x2000 - lo));
    t_stack_lo = lo;
    t_stack_sz = self->stack_sz;
}

size_t StackUsed() {
    if (!t_stack_lo) return 0;
    const uint64_t* p = reinterpret_cast<const uint64_t*>(t_stack_lo);
    size_t n = t_stack_sz / 8, i = 0;
    while (i < n && p[i] == kPaint) i++;
    size_t used = t_stack_sz - i * 8;
    size_t prev = g_stack_max.load();
    while (used > prev && !g_stack_max.compare_exchange_weak(prev, used)) {
    }
    return used;
}
#else
size_t StackUsed() { return 0; }
#endif

thread_local ThreadRef t_current;
std::mutex g_threads_lock;
std::vector<ThreadRef> g_threads;
std::atomic<uint32_t> g_next_id{0x10};

// guest structures per thread
constexpr uint32_t kKpcrSize = 0x400;
constexpr uint32_t kEthreadSize = 0x200;

void SetupGuestBlocks(Thread& t, uint32_t stack_size, uint32_t tls_size) {
    stack_size = std::max<uint32_t>(stack_size, 0x10000);
    tls_size = (tls_size + 3) & ~3u;
    t.stack_top = mem::AllocStack(stack_size + tls_size + 0x1000);
    t.stack_size = stack_size;
    t.kpcr = mem::AllocSystem(kKpcrSize, 64);
    t.ethread = mem::AllocSystem(kEthreadSize, 64);
    // TLS sits at the top of the stack: StackBase + tls_index * 4 must equal TlsData
    t.tls = t.stack_top - tls_size;
    uint32_t k = t.kpcr;
    W32(k + 0x00, 0xFFFFFFFF);                  // NtTib.ExceptionList
    W32(k + 0x04, t.stack_top);                 // NtTib.StackBase
    W32(k + 0x08, t.stack_top - stack_size);    // NtTib.StackLimit
    W32(k + 0x18, k);                           // NtTib.Self
    W32(k + 0x1C, k);                           // SelfPcr
    W32(k + 0x20, k + 0x28);                    // Prcb
    G<uint8_t>(k + 0x24) = 0;                   // Irql
    W32(k + 0x28, t.ethread);                   // PrcbData.CurrentThread
    uint32_t e = t.ethread;
    G<uint8_t>(e + 0) = 6;                      // dispatcher type ThreadObject
    G<uint8_t>(e + 2) = uint8_t(kEthreadSize / 4);  // Header.Size in dwords
    W32(e + 0x1C, t.stack_top);                 // KTHREAD.StackBase
    W32(e + 0x20, t.stack_top - stack_size);    // KTHREAD.StackLimit
    W32(e + 0x28, t.tls);                       // KTHREAD.TlsData
    W32(e + 0x12C, t.id);                       // ETHREAD.UniqueThread
}

void InitContext(Thread& t, X86Context& ctx) {
    std::memset(&ctx, 0, sizeof(ctx));
    ctx.esp = (t.tls & ~15u) - 16;
    ctx.fs_base = t.kpcr;
    ctx.fcw = 0x27F;  // x87 53 bit precision, exceptions masked
    ctx.mxcsr = 0x1F80;  // SSE exceptions masked, round to nearest
    ctx.thread_handle = 0;
    t.ctx = &ctx;
}

void Register(const ThreadRef& t) {
    std::lock_guard<std::mutex> lock(g_threads_lock);
    g_threads.push_back(t);
}

void Unregister(Thread* t) {
    std::lock_guard<std::mutex> lock(g_threads_lock);
    g_threads.erase(std::remove_if(g_threads.begin(), g_threads.end(), [t](const ThreadRef& r) { return r.get() == t; }),
                    g_threads.end());
}

void MarkExited(Thread& t, uint32_t code) {
    t.exit_code = code;
    std::lock_guard<std::mutex> lock(DispatchLock());
    SetSignal(t, 1);
    SignalChanged();
}

#ifdef __SWITCH__
extern "C" void shadow_profile_thread(const char* name) __attribute__((weak));
extern "C" void shadow_place_thread(int role) __attribute__((weak));  // switch/cores.cpp
#endif

void NameHostThread(const char* name) {
#ifdef _WIN32
    wchar_t w[64];
    size_t i = 0;
    for (; name[i] && i < 63; i++) w[i] = wchar_t(name[i]);
    w[i] = 0;
    using Fn = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    static Fn set = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription")));
    if (set) set(GetCurrentThread(), w);
#elif defined(__SWITCH__)
    // newlib has no pthread_setname_np; the Switch layer's profiler keeps the name and handle (switch/profiler.cpp)
    if (shadow_profile_thread) shadow_profile_thread(name);
    // Shadow-nx: role 3, a thread running guest code (the title's threads, and the DPC, vertical blank and sound
    // callback threads): with guest_one_core they all share one core, as on the Xbox's single CPU
    if (shadow_place_thread) shadow_place_thread(3);
#else
    pthread_setname_np(pthread_self(), name);
#endif
}

// recompiled code nests host frames as deep as the guest's calls, give it room
void Spawn(std::function<void()> fn) {
    auto* f = new std::function<void()>(std::move(fn));
#ifdef _WIN32
    auto entry = [](void* p) -> DWORD {
        std::unique_ptr<std::function<void()>> g(static_cast<std::function<void()>*>(p));
        (*g)();
        return 0;
    };
    HANDLE h = ::CreateThread(nullptr, 64u << 20, entry, f, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    if (h) ::CloseHandle(h);
#elif defined(__SWITCH__)
    Reap();
    auto entry = [](void* p) {
        auto* h = static_cast<HostThread*>(p);
        t_host = h;
        PaintStack();
        {
            std::unique_ptr<std::function<void()>> g(h->fn);
            h->fn = nullptr;
            (*g)();
        }
        HostThreadDone();  // the very last step: the next Spawn joins this thread and pools its stack
    };  // returning ends the thread (libnx calls threadExit)
    Result rc = 0;
    for (int attempt = 0; attempt < 2; attempt++) {
        auto* h = new HostThread;
        h->fn = f;
        h->mem = TakeStack();
        rc = h->mem ? threadCreate(&h->t, entry, h, h->mem, kHostStack, 0x3B, -2)
                    : MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
        if (R_SUCCEEDED(rc)) {
            // as libnx's pthread_create: any core of the process's
            u64 mask = 0;
            if (R_SUCCEEDED(svcGetInfo(&mask, InfoType_CoreMask, CUR_PROCESS_HANDLE, 0)))
                svcSetThreadCoreMask(h->t.handle, -1, u32(mask));
            rc = threadStart(&h->t);
            if (R_FAILED(rc)) threadClose(&h->t);
        }
        if (R_SUCCEEDED(rc)) {
            g_live++;
            g_spawned++;
            return;
        }
        if (h->mem) GiveBackStack(h->mem);
        delete h;
        LOGR("threads: %s a host thread (result 0x%X)%s; %s", attempt ? "FAILED to start" : "could not start",
             unsigned(rc), attempt ? "" : ", retrying", HostMemoryText().c_str());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Reap();
    }
    delete f;
#else
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 64u << 20);
    pthread_t th;
    pthread_create(&th, &attr, [](void* p) -> void* {
        std::unique_ptr<std::function<void()>> g(static_cast<std::function<void()>*>(p));
        (*g)();
        return nullptr;
    }, f);
    pthread_detach(th);
    pthread_attr_destroy(&attr);
#endif
}

}  // namespace

void HostSelfTest() {
#ifdef __SWITCH__
    using Clock = std::chrono::steady_clock;
    auto unclaimed = [] { return size_t(fake_heap_end - static_cast<char*>(sbrk(0))); };
    LOGR("selftest: before: %s", HostMemoryText().c_str());
    const auto t0 = Clock::now();
    const uint32_t live0 = g_live.load(), pool0 = g_pool.load();
    const size_t free0 = unclaimed();
    std::atomic<int> ran{0};
    constexpr int kThreads = 2000, kStacks = 2000;
    for (int i = 0; i < kThreads; i++) {
        Spawn([&ran] { ran++; });
        for (int k = 0; k < 5000 && g_live.load() > live0 + 4; k++) {
            std::this_thread::sleep_for(std::chrono::microseconds(200));
            Reap();
        }
    }
    for (int k = 0; k < 5000 && g_live.load() > live0; k++) {
        std::this_thread::sleep_for(std::chrono::microseconds(200));
        Reap();
    }
    const uint32_t pool_grew = g_pool.load() - pool0;
    uint32_t stack_bytes0 = 0, stacks0 = mem::StacksInUse(&stack_bytes0);
    int stack_fails = 0;
    for (int i = 0; i < kStacks; i++) {
        uint32_t top = mem::AllocStack(0x10000 + 0x100 + 0x1000);
        if (!top) {
            stack_fails++;
            break;
        }
        mem::FreeStack(top);
    }
    uint32_t stack_bytes1 = 0, stacks1 = mem::StacksInUse(&stack_bytes1);
    const size_t free1 = unclaimed();
    const size_t drop = free0 > free1 ? free0 - free1 : 0;
    const bool ok = ran.load() == kThreads && g_live.load() == live0 && pool_grew <= 6 &&
                    drop <= size_t(pool_grew) * kHostStack + (16u << 20) && !stack_fails && stacks1 == stacks0;
    LOGR("selftest: %s in %.2f s: %d of %d host threads ran, %u still running (%u before), stack pool grew by %u, "
         "unclaimed heap down %zu MB; %d guest stacks taken and freed, %u in use after (%u before)%s",
         ok ? "PASSED" : "FAILED", std::chrono::duration<double>(Clock::now() - t0).count(), ran.load(), kThreads,
         g_live.load(), live0, pool_grew, drop >> 20, kStacks - stack_fails, stacks1, stacks0,
         stack_fails ? ", a guest stack could not be taken" : "");
    LOGR("selftest: after: %s", HostMemoryText().c_str());
#endif
}

std::string KernelCountsText() {
    char b[160];
    std::snprintf(b, sizeof b, "handles %zu, kernel objects %lld, system arena %u KB, backed on demand %u",
                  HandleCount(), (long long)Object::live.load(), mem::SystemInUse() >> 10, mem::BackedOnDemand());
    return b;
}

std::string HostMemoryText() {
#ifdef __SWITCH__
    struct mallinfo mi = mallinfo();
    uint32_t stack_bytes = 0, stacks = mem::StacksInUse(&stack_bytes);
    size_t unclaimed = size_t(fake_heap_end - static_cast<char*>(sbrk(0)));
    char b[512];
    std::snprintf(b, sizeof b,
                  "heap %zu MB used, %zu MB free inside, %zu MB unclaimed; host threads %u running, %u started, %u "
                  "freed, %u stacks pooled; guest stacks %u (%u KB); %s",
                  size_t(mi.uordblks) >> 20, size_t(mi.fordblks) >> 20, unclaimed >> 20, g_live.load(),
                  g_spawned.load(), g_reaped.load(), g_pool.load(), stacks, stack_bytes >> 10, KernelCountsText().c_str());
    return b;
#else
    return {};
#endif
}

Thread* Current() { return t_current.get(); }
ThreadRef CurrentRef() { return t_current; }
X86Context& CurrentContext() { return *t_current->ctx; }

std::vector<ThreadRef> Threads() {
    std::lock_guard<std::mutex> lock(g_threads_lock);
    return g_threads;
}

// STREET: the title's priorities (KeSetBasePriorityThread: an increment on normal, 16 for time critical) on the
// Switch's threads. With every thread at 0x3B the main thread could run ahead of FIFA's time critical audio thread,
// which on the Xbox always ran first: it set a sound channel's volume before the audio thread had made the channel's
// voice (0x220C48, a call through a null object) whenever a thread was held back (a long frame, the profiler).
#ifdef __SWITCH__
extern "C" int shadow_guest_one_core() __attribute__((weak));  // switch/cores.cpp
#endif

void ApplyPriority(Thread& t) {
#ifdef __SWITCH__
    uint32_t h = t.host;
    if (!h) return;
    int p = t.priority;
    u32 prio = p >= 16 ? 0x30 : p <= -16 ? 0x3F : u32(0x3B - std::clamp(p, -2, 2));
    // Shadow-nx: switch-test3c put every guest thread at 0x3B with guest_one_core, on the theory that the loaders were
    // starved; they were not (test 5: the stalls were the stack leak). switch-test8 keeps the title's own priorities on
    // one core, as on the Xbox's one CPU: test 7 crashed with the loading job (boosted +2) and the title thread inside
    // the same RenderWare frame list at once, on two cores. NtYieldExecution has a safety valve (xboxkrnl.cpp).
    const bool flat = false;
    Result rc = svcSetThreadPriority(Handle(h), prio);
    LOGR("threads: guest %X priority %d -> %X%s%s", t.id, p, unsigned(prio), flat ? " (one core: all 3B)" : "",
         R_FAILED(rc) ? " (refused)" : "");
#else
    (void)t;
#endif
}

struct StartInfo {
    uint32_t system_routine, start_routine, start_context;
};
static std::unordered_map<Thread*, StartInfo> g_start;
static std::mutex g_start_lock;

ThreadRef CreateThread(uint32_t stack_size, uint32_t tls_size, uint32_t system_routine, uint32_t start_routine,
                       uint32_t start_context, bool suspended) {
    auto t = std::make_shared<Thread>();
    t->id = g_next_id.fetch_add(4);
    SetupGuestBlocks(*t, stack_size, tls_size);
    Bind(t, t->ethread);
    if (suspended) t->suspend = 1;
    std::lock_guard<std::mutex> lock(g_start_lock);
    g_start[t.get()] = {system_routine, start_routine, start_context};
    return t;
}

void StartThread(ThreadRef t) {
    Register(t);
    Spawn([t] {
        t_current = t;
        char name[32];
        std::snprintf(name, sizeof(name), "guest %X", t->id);
        NameHostThread(name);
#ifdef __SWITCH__
        t->host = threadGetCurHandle();
        ApplyPriority(*t);  // a priority set while it was suspended
#endif
        X86Context ctx;
        InitContext(*t, ctx);
        {
            std::unique_lock<std::mutex> lock(t->apc_lock);
            t->resume_cv.wait(lock, [&] { return t->suspend.load() <= 0; });
        }
        StartInfo si;
        {
            std::lock_guard<std::mutex> lock(g_start_lock);
            si = g_start[t.get()];
            g_start.erase(t.get());
        }
#ifdef __SWITCH__
        LOGR("memory: %s", HostMemoryText().c_str());
#endif
        LOGR("trace: thread %X starts at %08X (%08X, %08X)", t->id, si.system_routine ? si.system_routine : si.start_routine,
             si.start_routine, si.start_context);
        uint32_t code;
        if (si.system_routine)
            code = CallGuest(ctx, si.system_routine, {si.start_routine, si.start_context});
        else
            code = CallGuest(ctx, si.start_routine, {si.start_context});
        LOGR("trace: thread %X returned %08X; stack used %zu KB", t->id, code, StackUsed() >> 10);
        waitmon::Exited(code);
        MarkExited(*t, code);
        Unregister(t.get());
        mem::FreeStack(t->stack_top);  // Shadow-nx, switch-test9 (see ExitThread)
        t_current.reset();
    });
}

void StartHostThread(const char* name, std::function<void(X86Context&)> fn) {
    auto t = std::make_shared<Thread>();
    t->id = g_next_id.fetch_add(4);
    SetupGuestBlocks(*t, 0x20000, 0x100);
    Bind(t, t->ethread);
    std::string n = name;
    Spawn([t, fn, n] {
        t_current = t;
        NameHostThread(n.c_str());
        X86Context ctx;
        InitContext(*t, ctx);
        fn(ctx);
        t_current.reset();
    });
}

void ExitThread(uint32_t code) {
    Thread* t = Current();
    if (t) {
        X86Context* c = t->ctx;
        LOGR("trace: thread %X exits with %08X; stack used %zu KB%s%s", t->id, code, StackUsed() >> 10,
             c ? "; from " : "", c ? waitmon::CallerChain(*c).c_str() : "");
        waitmon::Exited(code);
        MarkExited(*t, code);
        Unregister(t);
        // Shadow-nx, switch-test9: the guest stack goes back for the next thread. The stack arena is 32 MB (backed
        // up front, memory.cpp); switch-test8 made 200 threads by 386 s, a stage piece each, and the next stack
        // past the arena failed to back ("commit D2610000 failed", 0xCE01): the next stage's loader never ran.
        // No guest code runs on this thread again; the KPCR and ETHREAD stay (handles still name them).
        mem::FreeStack(t->stack_top);
    }
    t_current.reset();
#ifdef _WIN32
    ::ExitThread(code);
#else
#ifdef __SWITCH__
    HostThreadDone();  // the very last step: the next Spawn joins this thread and pools its stack
    threadExit();
#else
    pthread_exit(nullptr);
#endif
#endif
}

void Resume(Thread& t) {
    std::lock_guard<std::mutex> lock(t.apc_lock);
    if (t.suspend > 0) t.suspend--;
    t.resume_cv.notify_all();
}

uint32_t Suspend(Thread& t) {
    uint32_t prev = uint32_t(t.suspend++);
    if (&t == Current()) SuspendPoint();
    return prev;
}

void SuspendPoint() {
    Thread* t = Current();
    if (!t || t->suspend.load() <= 0) return;
    std::unique_lock<std::mutex> lock(t->apc_lock);
    t->resume_cv.wait(lock, [&] { return t->suspend.load() <= 0; });
}

void QueueApc(Thread& t, uint32_t routine, uint32_t a1, uint32_t a2, uint32_t a3) {
    std::lock_guard<std::mutex> lock(t.apc_lock);
    t.apcs.push_back({routine, a1, a2, a3});
    SignalChanged();
}

bool RunApcs(Thread& t) {
    bool ran = false;
    for (;;) {
        std::array<uint32_t, 4> a;
        {
            std::lock_guard<std::mutex> lock(t.apc_lock);
            if (t.apcs.empty()) break;
            a = t.apcs.front();
            t.apcs.pop_front();
        }
        CallGuest(*t.ctx, a[0], {a[1], a[2], a[3]});
        ran = true;
    }
    return ran;
}

}  // namespace kernel
