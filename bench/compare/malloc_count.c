/* LD_PRELOAD library that counts the heap of a program: every malloc, calloc, realloc, memalign and free that goes through
   the C library (operator new too: libstdc++ calls malloc). Two sets of counters: the whole process (atomics) and the calling
   thread (the GUI thread, so that the threads of a software OpenGL do not count in the numbers of a frame). The program asks
   for them with dlsym(RTLD_DEFAULT, "mc_global") and dlsym(RTLD_DEFAULT, "mc_thread"). bench/compare is outside the no-std rule
   (see docs/COMPARACAO.md); this one is C and uses no library besides glibc. */
#define _GNU_SOURCE
#include <malloc.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <execinfo.h>
#include <stdlib.h>
#include <unistd.h>

extern void* __libc_malloc(size_t);
extern void* __libc_calloc(size_t, size_t);
extern void* __libc_realloc(void*, size_t);
extern void __libc_free(void*);
extern void* __libc_memalign(size_t, size_t);

struct mc_stats
{
    uint64_t allocs; /* calls that returned memory */
    uint64_t bytes;  /* usable bytes handed out, summed over all calls */
    uint64_t frees;
    int64_t live;    /* usable bytes handed out minus freed, now */
};

static struct mc_stats g_global;
static __thread struct mc_stats g_thread;

void mc_global(struct mc_stats* out) { out->allocs = __atomic_load_n(&g_global.allocs, __ATOMIC_RELAXED); out->bytes = __atomic_load_n(&g_global.bytes, __ATOMIC_RELAXED); out->frees = __atomic_load_n(&g_global.frees, __ATOMIC_RELAXED); out->live = __atomic_load_n(&g_global.live, __ATOMIC_RELAXED); }
void mc_thread(struct mc_stats* out) { *out = g_thread; }

/* MC_BIG=<bytes>: every allocation of at least that many bytes writes its size and its call stack to stderr (where the memory goes) */
static size_t big_limit(void)
{
    static size_t v = (size_t)-2;
    if (v == (size_t)-2) { const char* e = getenv("MC_BIG"); v = e ? (size_t)atol(e) : 0; }
    return v;
}
static __thread int in_trace;
static __thread int g_trace_all; /* mc_trace(1): every allocation of this thread writes its stack, whatever its size */
void mc_trace(int on) { g_trace_all = on; }
static void trace_big(size_t u)
{
    size_t lim = big_limit();
    if (in_trace || !(g_trace_all || (lim && u >= lim))) return;
    in_trace = 1;
    void* fr[16];
    int n = backtrace(fr, 16);
    char head[48];
    int k = 0;
    head[k++] = 'B'; head[k++] = 'I'; head[k++] = 'G'; head[k++] = ' ';
    unsigned long long x = u; char d[24]; int m = 0;
    do { d[m++] = (char)('0' + x % 10); x /= 10; } while (x);
    while (m) head[k++] = d[--m];
    head[k++] = '\n';
    if (write(2, head, (size_t)k) < 0) {}
    backtrace_symbols_fd(fr + 2, n - 2, 2);
    in_trace = 0;
}

static inline void add(size_t u)
{
    trace_big(u);
    __atomic_fetch_add(&g_global.allocs, 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_global.bytes, u, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_global.live, (int64_t)u, __ATOMIC_RELAXED);
    g_thread.allocs++;
    g_thread.bytes += u;
    g_thread.live += (int64_t)u;
}
static inline void sub(size_t u)
{
    __atomic_fetch_add(&g_global.frees, 1, __ATOMIC_RELAXED);
    __atomic_fetch_sub(&g_global.live, (int64_t)u, __ATOMIC_RELAXED);
    g_thread.frees++;
    g_thread.live -= (int64_t)u;
}

void* malloc(size_t n)
{
    void* p = __libc_malloc(n);
    if (p) add(malloc_usable_size(p));
    return p;
}
void* calloc(size_t a, size_t b)
{
    void* p = __libc_calloc(a, b);
    if (p) add(malloc_usable_size(p));
    return p;
}
void* realloc(void* old, size_t n)
{
    size_t before = old ? malloc_usable_size(old) : 0;
    void* p = __libc_realloc(old, n);
    if (p)
    {
        if (old) sub(before);
        add(malloc_usable_size(p));
    }
    return p;
}
void free(void* p)
{
    if (!p) return;
    sub(malloc_usable_size(p));
    __libc_free(p);
}
void* memalign(size_t al, size_t n)
{
    void* p = __libc_memalign(al, n);
    if (p) add(malloc_usable_size(p));
    return p;
}
int posix_memalign(void** out, size_t al, size_t n)
{
    if (al < sizeof(void*) || (al & (al - 1))) return EINVAL;
    void* p = __libc_memalign(al, n);
    if (!p) return ENOMEM;
    add(malloc_usable_size(p));
    *out = p;
    return 0;
}
void* aligned_alloc(size_t al, size_t n) { return memalign(al, n); }
