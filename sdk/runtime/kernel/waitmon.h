#pragma once
// Shadow-nx: the wait monitor. Every thread that runs guest code reports what it waits on (kernel waits, critical
// sections, delays) and how often it yields; every kernel signal is stamped with its time and thread. A watchdog
// thread writes the whole picture to the log when a wait has lasted long (a stall), so a hang names which thread
// waits on which object, since when, called from where in the game, and who last signalled that object.
#include <cstdint>
#include <string>
#include <vector>

#include "kernel/kernel.h"

namespace kernel::waitmon {

// what the next Wait of this thread is for ("critical section", "file"...); cleared by that Wait
void Hint(const char* kind, uint32_t detail = 0);
void BeginWait(const std::vector<ObjectRef>& objs, bool wait_all, int64_t timeout_100ns);
void EndWait(uint32_t result);
void BeginDelay(int64_t interval_100ns);
void EndDelay();
void Yielded();
// an object was signalled (SetEvent, ReleaseSemaphore, ReleaseMutant, PulseEvent, a timer)
void Signalled(Object& o, const char* how);
// Shadow-nx, switch-test5: the guest return addresses on a thread's stack ("2ADF04 < 4F526 < ..."), for trace lines
std::string CallerChain(const X86Context& ctx);
// this thread has finished its guest code (returned or ExitThread): the dumps say so instead of "running"
void Exited(uint32_t code);
// Shadow-nx, switch-test8: every thread's state right now, in the log (a guest trap calls it before stopping)
void DumpNow(const char* why);
// the watchdog thread (Switch and PC); on with the setting wait_monitor
void Start();
inline bool g_on = false;

}  // namespace kernel::waitmon
