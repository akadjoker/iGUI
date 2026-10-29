#pragma once

#include <chrono>

#if defined(CLOCK_PROCESS_CPUTIME_ID)
#include <time.h>
#endif

namespace bench
{

// Timestamps for the benchmark. CPU time, not wall time: on a shared machine the
// process is preempted, and wall time then measures how busy the machine is
// rather than what a frame costs. The scheduler's CPU accounting is per-process
// and unaffected by anything else running alongside.
//
// The absolute value is meaningless (it is CPU time since process start); only
// differences are used.
inline double cpuNowUs()
{
#if defined(CLOCK_PROCESS_CPUTIME_ID)
    struct timespec stamp;
    if (::clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &stamp) == 0)
        return static_cast<double>(stamp.tv_sec) * 1000000.0 +
               static_cast<double>(stamp.tv_nsec) * 0.001;
#endif
    using namespace std::chrono;
    return static_cast<double>(
            duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}

} // namespace bench
