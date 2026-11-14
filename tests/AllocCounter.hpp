// Counts the heap allocations of a program: ct allocates with malloc, the rest with operator new, and
// both are counted. Included by one file of a program linked with
//   -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
// (the wrapped malloc is __wrap_malloc below; the real one is __real_malloc). No standard library.
// gTraceAllocs makes every allocation print its call stack (backtrace_symbols_fd, one block each).
#pragma once

#include <execinfo.h>
#include <stdlib.h>
#include <unistd.h>

extern "C" void* __real_malloc(size_t);
extern "C" void* __real_calloc(size_t, size_t);
extern "C" void* __real_realloc(void*, size_t);

static unsigned long long gAllocs = 0;
static bool gTraceAllocs = false; // --trace-allocs: where the allocations of one idle frame come from

static void traceAlloc()
{
    if (!gTraceAllocs) return;
    gTraceAllocs = false; // backtrace() itself may allocate
    void* frames[14];
    const int n = backtrace(frames, 14);
    backtrace_symbols_fd(frames + 2, n - 2, 2);
    if (write(2, "--\n", 3) < 0) return;
    gTraceAllocs = true;
}

extern "C" void* __wrap_malloc(size_t n)
{
    ++gAllocs;
    traceAlloc();
    return __real_malloc(n);
}
extern "C" void* __wrap_calloc(size_t a, size_t b)
{
    ++gAllocs;
    return __real_calloc(a, b);
}
extern "C" void* __wrap_realloc(void* p, size_t n)
{
    ++gAllocs;
    traceAlloc();
    return __real_realloc(p, n);
}
void* operator new(unsigned long n)
{
    ++gAllocs;
    traceAlloc();
    void* p = __real_malloc(n ? n : 1);
    if (!p) abort();
    return p;
}
void* operator new[](unsigned long n)
{
    ++gAllocs;
    void* p = __real_malloc(n ? n : 1);
    if (!p) abort();
    return p;
}
void operator delete(void* p) noexcept
{
    free(p);
}
void operator delete[](void* p) noexcept
{
    free(p);
}
void operator delete(void* p, unsigned long) noexcept
{
    free(p);
}
void operator delete[](void* p, unsigned long) noexcept
{
    free(p);
}

