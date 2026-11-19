// Shared by the four comparison programs (iGUI, Dear ImGui, FLTK, GTK): the scenes, the numbers and how they are taken.
// See docs/COMPARACAO.md. Plain C library calls only (no std::, no standard headers of C++): the iGUI program includes it too.
//
// Every program is one library, one scene and one mode:
//     cmp_<lib> <scene A..F> <mode> [--n=N] [--gl|--soft]
//     frames   startup, memory, heap, and the cost of a frame idle / with the mouse moving / scrolling / the scene's action
//     idlecpu  the library's own main loop for 10 s with nothing happening: CPU, wake-ups, frames
//     latency  an injected click until the changed widget is drawn and presented
// Output: key=value lines on stdout. With ICOUNT=1 in the environment the GUI side of each frame is single-stepped and counted
#pragma once

#include <dirent.h>
#include <dlfcn.h>
#include <signal.h>
#include <ucontext.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

namespace cmp
{

// ---- the scenes (the same numbers for every library) ----------------------------------------------------------------------
enum
{
    kW = 1000, // window size
    kH = 700,
    kFormCols = 8, // scene B: 200 widgets, 40 of each kind, in a grid of 8 x 25 cells
    kFormRows = 25,
    kCellW = 120,
    kCellH = 26,
    kFormX = 20,
    kFormY = 20,
    kRows = 100000,    // scene C
    kTreeGroups = 100, // scene D: 100 groups of 99 children = 10000 nodes
    kTreeKids = 99,
    kTextLines = 91000, // scene E: 91000 lines of 55 bytes = 5 005 000 bytes
    kWindows = 50,      // scene F
};

// The kind of the widget in cell i of scene B: 0 label, 1 button, 2 checkbox, 3 slider, 4 text field.
inline int formKind(int i) { return i % 5; }
inline void formText(int i, char* out, int n) { snprintf(out, n, "%s %d", formKind(i) == 0 ? "Label" : formKind(i) == 1 ? "Button" : formKind(i) == 2 ? "Check" : "Field", i); }
// The checkbox the latency test clicks: cell 2 (row 0, column 2).
inline int latencyX() { return kFormX + 2 * kCellW + 14; }
inline int latencyY() { return kFormY + kCellH / 2; }
inline void rowText(int i, char* out, int n) { snprintf(out, n, "Row %06d  item text", i); }
inline void groupText(int i, char* out, int n) { snprintf(out, n, "Group %03d", i); }
inline void kidText(int g, int k, char* out, int n) { snprintf(out, n, "Node %03d.%02d", g, k); }
// Scene F: window k, cascaded, 260 x 120, a title and a label, a button and a checkbox.
inline int winX(int k) { return 12 + (k % 25) * 22 + (k / 25) * 12; }
inline int winY(int k) { return 12 + (k % 25) * 22; }
enum { kWinW = 260, kWinH = 120 };
// The text of scene E, allocated (the caller frees it); length in *len.
inline char* makeText(size_t* len)
{
    char* t = static_cast<char*>(malloc(static_cast<size_t>(kTextLines) * 55 + 1));
    size_t o = 0;
    for (int i = 0; i < kTextLines; ++i) o += static_cast<size_t>(snprintf(t + o, 56, "line %06d: the quick brown fox jumps over a lazy dog\n", i));
    t[o] = 0;
    *len = o;
    return t;
}

// ---- time and the process ----------------------------------------------------------------------------------------------------
inline double wallMs()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec * 1e-6;
}
inline double threadCpuUs()
{
    timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return ts.tv_sec * 1e6 + ts.tv_nsec * 1e-3;
}
inline double processCpuMs()
{
    timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec * 1e-6;
}
// set by main() before anything else; the startup time counts from here
inline double& t0Wall() { static double v = wallMs(); return v; }
inline double& t0Cpu() { static double v = processCpuMs(); return v; }

inline void memKb(long& rss, long& pss)
{
    rss = pss = -1;
    FILE* f = fopen("/proc/self/smaps_rollup", "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f))
    {
        long v;
        if (sscanf(line, "Rss: %ld kB", &v) == 1) rss = v;
        else if (sscanf(line, "Pss: %ld kB", &v) == 1) pss = v;
    }
    fclose(f);
}
// CPU time of the X server (Xvfb) so far, in ns: the drawing a library asks the server for is done there, not in its own process.
inline long long xserverCpuNs()
{
    static int pid = 0;
    if (!pid)
    {
        DIR* d = opendir("/proc");
        while (d)
        {
            dirent* e = readdir(d);
            if (!e) break;
            const int p = atoi(e->d_name);
            if (p <= 0) continue;
            // the Xvfb of THIS run: its command line has our display (":99") as an argument (other X servers may be running)
            char path[64], cmd[512];
            snprintf(path, sizeof path, "/proc/%d/cmdline", p);
            FILE* f = fopen(path, "r");
            if (!f) continue;
            const size_t n = fread(cmd, 1, sizeof cmd - 1, f);
            fclose(f);
            cmd[n] = 0;
            const char* display = getenv("DISPLAY");
            if (!display || strstr(cmd, "Xvfb") == nullptr) continue;
            for (size_t i = 0; i < n; i += strlen(cmd + i) + 1)
                if (!strcmp(cmd + i, display)) pid = p;
            if (pid) break;
        }
        if (d) closedir(d);
        if (!pid) pid = -1;
    }
    if (pid < 0) return -1;
    char path[64];
    snprintf(path, sizeof path, "/proc/%d/schedstat", pid);
    FILE* f = fopen(path, "r");
    long long ns = -1;
    if (f)
    {
        if (fscanf(f, "%lld", &ns) != 1) ns = -1;
        fclose(f);
    }
    return ns;
}
inline int threadCount()
{
    FILE* f = fopen("/proc/self/status", "r");
    int n = -1;
    char line[256];
    while (f && fgets(line, sizeof line, f))
        if (sscanf(line, "Threads: %d", &n) == 1) break;
    if (f) fclose(f);
    return n;
}

// ---- the heap, through the LD_PRELOAD library (bench/compare/malloc_count.c) ---------------------------------------------------
struct McStats
{
    unsigned long long allocs, bytes, frees;
    long long live;
};
typedef void (*McFn)(McStats*);
inline McFn mcGlobalFn() { static McFn f = reinterpret_cast<McFn>(dlsym(RTLD_DEFAULT, "mc_global")); return f; }
inline McFn mcThreadFn() { static McFn f = reinterpret_cast<McFn>(dlsym(RTLD_DEFAULT, "mc_thread")); return f; }
typedef void (*McTraceFn)(int);
inline McTraceFn mcTraceFn() { static McTraceFn f = reinterpret_cast<McTraceFn>(dlsym(RTLD_DEFAULT, "mc_trace")); return f; }
inline McStats mcGlobal() { McStats s = {0, 0, 0, 0}; if (McFn f = mcGlobalFn()) f(&s); return s; }
inline McStats mcThread() { McStats s = {0, 0, 0, 0}; if (McFn f = mcThreadFn()) f(&s); return s; }
inline bool mcAvailable() { return mcGlobalFn() != nullptr; }

// ---- counting instructions -----------------------------------------------------------------------------------------------------
// The perf counters are closed to a user here (kernel.perf_event_paranoid=4), so the instructions of a stretch are counted by
// single-stepping it: the trap flag of the thread is set at the start of the stretch, every instruction then raises SIGTRAP, a
// handler counts them, and the flag is cleared at the end. About 3 microseconds an instruction: a stretch of a few million is
// seconds, a startup is not countable. Only the user-space instructions of the one thread are counted (a system call counts as one,
// the work of the kernel and of the other threads does not). The empty stretch costs a few instructions of the markers themselves:
// they are measured once and taken off. Run with ICOUNT=1 in the environment; the counts come out as
//     region <k>: <instructions>
// and "REGION <k> <name>" before each says which stretch it is.
inline int& icountOn() { static int v = getenv("ICOUNT") ? 1 : 0; return v; }
inline int& regionIndex() { static int v = 0; return v; }
inline volatile unsigned long long& steps() { static volatile unsigned long long v = 0; return v; }
// ICPROF=<file> (with ICOUNT=1): the address of every instruction of the region ICPROF_REGION (default 0) goes to <file>, and the
// memory map to <file>.maps: a histogram of where the instructions are (tools of the analysis, not of the benchmark).
enum { kProfMax = 16 * 1024 * 1024 };
inline unsigned long long*& profBuf() { static unsigned long long* p = nullptr; return p; }
inline size_t& profN() { static size_t n = 0; return n; }
inline void trapHandler(int, siginfo_t*, void* uc)
{
    ++steps();
    if (profBuf() && profN() < kProfMax) profBuf()[profN()++] = static_cast<unsigned long long>(static_cast<ucontext_t*>(uc)->uc_mcontext.gregs[REG_RIP]);
}
inline void stepsOn() { steps() = 0; __asm__ volatile("pushfq; orq $0x100,(%%rsp); popfq" ::: "cc", "memory"); }
inline unsigned long long stepsOff() { __asm__ volatile("pushfq; andq $~0x100,(%%rsp); popfq" ::: "cc", "memory"); return steps(); }
inline unsigned long long markerCost()
{
    static long long cost = -1;
    if (cost < 0)
    {
        struct sigaction sa;
        memset(&sa, 0, sizeof sa);
        sa.sa_sigaction = &trapHandler;
        sa.sa_flags = SA_SIGINFO;
        sigaction(SIGTRAP, &sa, nullptr);
        stepsOn();
        cost = static_cast<long long>(stepsOff());
        if (getenv("ICPROF")) profBuf() = static_cast<unsigned long long*>(malloc(sizeof(unsigned long long) * kProfMax));
    }
    return static_cast<unsigned long long>(cost);
}

// The phase being run ("idle", "hover"...): the name a counted frame gets. "settle" frames are never counted.
inline const char*& phaseName() { static const char* p = nullptr; return p; }
// Set by the runner: the phase injects input that the toolkit is expected to answer with a redraw (GTK waits for its frame clock).
inline bool& expectRedraw() { static bool v = false; return v; }
inline bool& regionActive() { static bool v = false; return v; }
// A counted stretch (only when ICOUNT is set): its number and name go to stdout, then the stepping starts; the end stops it and
// prints the count.
inline void regionBegin(const char* name = nullptr)
{
    if (!icountOn()) return;
    const char* nm = name ? name : phaseName();
    if (!nm || !strcmp(nm, "settle"))
    {
        regionActive() = false;
        return;
    }
    regionActive() = true;
    markerCost();
    printf("REGION %d %s\n", regionIndex(), nm);
    fflush(stdout);
    profN() = 0;
    stepsOn();
}
inline void regionEnd()
{
    if (!icountOn() || !regionActive()) return;
    const unsigned long long n = stepsOff();
    regionActive() = false;
    if (profBuf() && regionIndex() == (getenv("ICPROF_REGION") ? atoi(getenv("ICPROF_REGION")) : 0))
    {
        FILE* f = fopen(getenv("ICPROF"), "wb");
        if (f)
        {
            fwrite(profBuf(), sizeof(unsigned long long), profN(), f);
            fclose(f);
        }
        char mp[512];
        snprintf(mp, sizeof mp, "%s.maps", getenv("ICPROF"));
        FILE* in = fopen("/proc/self/maps", "r");
        FILE* out = fopen(mp, "w");
        char line[512];
        while (in && out && fgets(line, sizeof line, in)) fputs(line, out);
        if (in) fclose(in);
        if (out) fclose(out);
    }
    printf("region %d: %llu\n", regionIndex(), n - markerCost());
    fflush(stdout);
    ++regionIndex();
}

// What one frame of a library costs, filled by the library's frame().
struct FrameStats
{
    double guiUs = 0;     // CPU time of the GUI side: events in, widgets, layout, paint, the draw data ready (thread CPU time)
    double totalUs = 0;   // the whole frame: + the rasteriser or the GL submit + the present (thread CPU time)
    double wallUs = 0;    // wall time of the whole frame
    unsigned long long guiAllocs = 0, guiBytes = 0; // heap calls of the GUI side (calling thread)
    unsigned long long totalAllocs = 0;             // heap calls of the whole frame (calling thread)
    int drawCalls = -1, vertices = -1, indices = -1; // what the GUI side handed to the backend (-1: the library has no draw list)
};

// Times and counts a stretch of a frame; the markers are skipped when the instructions are being counted (they would count too).
struct Span
{
    double cpu0 = 0, wall0 = 0;
    McStats m0 = {0, 0, 0, 0};
    void begin()
    {
        cpu0 = threadCpuUs();
        wall0 = wallMs();
        m0 = mcThread();
    }
    double cpuUs() const { return threadCpuUs() - cpu0; }
    double wallUs() const { return (wallMs() - wall0) * 1000.0; }
    unsigned long long allocs() const { return mcThread().allocs - m0.allocs; }
    unsigned long long bytes() const { return mcThread().bytes - m0.bytes; }
};

// ---- real input events, from a second connection to the same X server (XTEST) --------------------------------------------------
extern "C"
{
int cmp_inj_open(void);
void cmp_inj_flush(void);
void cmp_inj_xflush(void);
void cmp_inj_move(int x, int y);
void cmp_inj_button(int b, int down);
void cmp_inj_key(unsigned long keysym);
}
struct Injector
{
    bool open() { return cmp_inj_open() != 0; }
    void flush() { cmp_inj_flush(); }
    void move(int x, int y) { cmp_inj_move(x, y); }
    void button(int b, bool down) { cmp_inj_button(b, down ? 1 : 0); }
    void click() { button(1, true); button(1, false); }
    void wheel(int dir) { button(dir > 0 ? 4 : 5, true); button(dir > 0 ? 4 : 5, false); }
    void key(unsigned long keysym) { cmp_inj_key(keysym); }
};

inline double median(double* v, int n)
{
    if (n <= 0) return 0.0;
    for (int i = 1; i < n; ++i)
    {
        double x = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; --j; }
        v[j + 1] = x;
    }
    return v[n / 2];
}

// ---- the run -----------------------------------------------------------------------------------------------------------------
struct Opt
{
    char scene = 'A';
    const char* mode = "frames";
    int n = 100;
    bool gl = false;     // iGUI: the OpenGL backend; false: the software rasteriser
    bool buildIc = false; // count the instructions of building the scene too (slow for the big ones)
    int seconds = 10;
    const char* phases = nullptr; // only these phases, comma separated (idle,hover,scroll,click,expand,typing); default all
};

inline bool parse(int argc, char** argv, Opt& o)
{
    if (argc < 3) return false;
    o.scene = argv[1][0];
    o.mode = argv[2];
    for (int i = 3; i < argc; ++i)
    {
        if (!strncmp(argv[i], "--n=", 4)) o.n = atoi(argv[i] + 4);
        else if (!strcmp(argv[i], "--gl")) o.gl = true;
        else if (!strcmp(argv[i], "--soft")) o.gl = false;
        else if (!strcmp(argv[i], "--build-ic")) o.buildIc = true;
        else if (!strncmp(argv[i], "--seconds=", 10)) o.seconds = atoi(argv[i] + 10);
        else if (!strncmp(argv[i], "--phases=", 9)) o.phases = argv[i] + 9;
        // --flat (iGUI) and --rounded (ImGui) are read by the programs themselves
    }
    if (icountOn() && o.n > 3) o.n = 2; // a counted frame takes seconds to step through; two samples are enough, they do not vary
    return o.scene >= 'A' && o.scene <= 'F';
}

inline void emit(const char* key, double v) { printf("%s=%.3f\n", key, v); }
inline void emitL(const char* key, long long v) { printf("%s=%lld\n", key, v); }

// The scene's input, as each phase does it.
inline void hoverPos(int i, int& x, int& y)
{
    x = 30 + (i * 37) % 940;
    y = 30 + (i * 53) % 640;
}

// One library. frame() runs a whole iteration of the library's loop and fills `FrameStats`; for a library that waits for events
// (FLTK, GTK) it handles what is pending and what that draws, and returns at once when there is nothing.
//   bool init()        the window, up and visible at kW x kH at 0,0 (no window manager here)
//   void build(char)   the scene
//   void frame(FrameStats&)
//   void treeOp(bool open)   scene D: expand or collapse every node (the call itself; the next frame shows it)
//   void natural(int seconds, long long& frames)   the library's own main loop, for `seconds`
//   bool changed()     the widget the latency test clicks has changed state and the frame showing it was presented
//   long long frames() the frames presented so far (for the loops that make them)
template <class L>
struct Runner
{
    L& lib;
    Opt o;
    Injector inj;
    explicit Runner(L& l) : lib(l) {}

    static void pr(const char* phase, const char* what, double v)
    {
        char k[96];
        snprintf(k, sizeof k, "%s_%s", phase, what);
        emit(k, v);
    }

    // n samples of one phase: the median of each number
    bool on(const char* name) const { return !o.phases || strstr(o.phases, name) != nullptr; }

    template <class Prep>
    void phase(const char* name, int n, Prep prep)
    {
        if (!on(name)) return;
        double gui[1024], tot[1024], wall[1024], als[1024];
        FrameStats last;
        phaseName() = name;
        expectRedraw() = strcmp(name, "idle") != 0;
        if (n > 1000) n = 1000;
        int got = 0;
        const long long x0 = xserverCpuNs();
        const double p0 = processCpuMs(), w0 = wallMs();
        for (int i = 0; i < n; ++i)
        {
            prep(i);
            FrameStats fs;
            const bool trace = i == n - 1 && getenv("CMP_TRACE_ALLOCS") && mcTraceFn();
            if (trace) mcTraceFn()(1); // where the heap calls of the last frame of the phase come from (stderr)
            lib.frame(fs);
            if (trace) mcTraceFn()(0);
            if (i == 0 && n > 1 && !icountOn()) { last = fs; continue; } // the first of a kind is not the steady state
            gui[got] = fs.guiUs;
            tot[got] = fs.totalUs;
            wall[got] = fs.wallUs;
            als[got] = static_cast<double>(fs.guiAllocs);
            last = fs;
            ++got;
        }
        if (icountOn()) return;
        const long long x1 = xserverCpuNs();
        const double p1 = processCpuMs(), w1 = wallMs();
        pr(name, "proc_us", (p1 - p0) * 1000.0 / n); // every thread of the process (a software OpenGL rasterises in its own), per frame
        if (x0 >= 0 && x1 >= 0) pr(name, "xsrv_us", static_cast<double>(x1 - x0) / 1000.0 / n); // the X server's CPU, per frame
        (void)w0;
        (void)w1;
        pr(name, "gui_us", median(gui, got));
        pr(name, "total_us", median(tot, got));
        pr(name, "wall_us", median(wall, got));
        pr(name, "gui_allocs", median(als, got));
        if (last.drawCalls >= 0)
        {
            pr(name, "draw_calls", last.drawCalls);
            pr(name, "vertices", last.vertices);
            pr(name, "indices", last.indices);
        }
    }

    void settle(int n)
    {
        phaseName() = "settle";
        expectRedraw() = false;
        for (int i = 0; i < n; ++i)
        {
            FrameStats fs;
            lib.frame(fs);
        }
    }

    int runFrames()
    {
        const bool ic = icountOn();
        t0Wall();
        t0Cpu();
        if (!lib.init()) { fprintf(stderr, "cmp: no window\n"); return 2; }
        const double tInit = wallMs();
        const McStats g0 = mcGlobal();
        const McStats h0 = mcThread();
        if (ic && o.buildIc) regionBegin("build");
        lib.build(o.scene);
        if (ic && o.buildIc) regionEnd();
        phaseName() = (ic && !on("first")) ? "settle" : "first"; // a counted first frame takes minutes to step through: only when asked
        const McStats g1 = mcGlobal();
        const McStats h1 = mcThread();
        const double tBuilt = wallMs();
        FrameStats first;
        lib.frame(first); // the first frame: the layout of everything, the glyphs it needs, the first present
        const double tFirst = wallMs();
        if (!ic)
        {
            emit("startup_wall_ms", tFirst - t0Wall());
            emit("startup_init_ms", tInit - t0Wall());
            emit("startup_build_ms", tBuilt - tInit);
            emit("startup_firstframe_ms", tFirst - tBuilt);
            emit("startup_cpu_ms", processCpuMs() - t0Cpu());
            if (lib.drawHash()) printf("draw_hash=%016llx\n", lib.drawHash());
            emit("first_gui_us", first.guiUs);
            emit("first_total_us", first.totalUs);
        }
        settle(8);
        if (!ic)
        {
            long rss, pss;
            memKb(rss, pss);
            emitL("rss_kb", rss);
            emitL("pss_kb", pss);
            emitL("threads", threadCount());
            const McStats g2 = mcGlobal();
            emitL("heap_live_bytes", g2.live);
            emitL("build_live_bytes", g1.live - g0.live);
            emitL("build_allocs", static_cast<long long>(g1.allocs - g0.allocs));
            emitL("build_bytes", static_cast<long long>(g1.bytes - g0.bytes));
            emitL("build_thread_allocs", static_cast<long long>(h1.allocs - h0.allocs));
            emit("build_ms", tBuilt - tInit);
            emitL("mcount", mcAvailable() ? 1 : 0);
        }
        if (!inj.open()) { fprintf(stderr, "cmp: no XTEST\n"); return 2; }
        usleep(100000);
        const int N = o.n;
        phase("idle", N, [&](int) {});
        phase("hover", N, [&](int i) {
            int x, y;
            hoverPos(i, x, y);
            inj.move(x, y);
            inj.flush();
        });
        if (o.scene == 'C' || o.scene == 'D' || o.scene == 'E')
        {
            inj.move(500, 350);
            inj.flush();
            settle(2);
            phase("scroll", N, [&](int i) {
                inj.wheel((i / 20) % 2 == 0 ? -1 : 1); // down for 20 notches, up for 20: stays in range
                inj.flush();
            });
        }
        if (o.scene == 'B')
        {
            // a drag: press on the first slider, move across it, release: three events in one frame
            phase("click", N, [&](int i) {
                inj.move(latencyX() + (i % 2) * 120 * 0, latencyY());
                inj.click();
                inj.flush();
            });
        }
        if (o.scene == 'D' && on("expand"))
        {
            // every node expanded or collapsed: the call, then the frame that shows it
            for (int k = 0; k < (ic ? 1 : 3); ++k)
            {
                for (int open = 0; open <= 1; ++open)
                {
                    const bool op = open != 0;
                    const char* nm = op ? "expand_all_call" : "collapse_all_call";
                    Span sp;
                    sp.begin();
                    regionBegin(nm);
                    lib.treeOp(op);
                    regionEnd();
                    const double us = sp.cpuUs();
                    FrameStats fs;
                    lib.frame(fs);
                    if (!ic && k == 2)
                    {
                        pr(op ? "expand_all" : "collapse_all", "call_us", us);
                        pr(op ? "expand_all" : "collapse_all", "frame_gui_us", fs.guiUs);
                        pr(op ? "expand_all" : "collapse_all", "frame_total_us", fs.totalUs);
                    }
                }
            }
        }
        if (o.scene == 'E')
        {
            inj.move(500, 350);
            inj.click();
            inj.flush();
            settle(2);
            phase("typing", N, [&](int i) {
                inj.key(0x61ul + static_cast<unsigned long>(i % 26)); // XK_a ..
                inj.flush();
            });
        }
        return 0;
    }

    int runIdleCpu()
    {
        t0Wall();
        if (!lib.init()) return 2;
        lib.build(o.scene);
        settle(8);
        long long frames0 = 0;
        usleep(200000);
        rusage r0, r1;
        getrusage(RUSAGE_SELF, &r0);
        const double c0 = processCpuMs(), w0 = wallMs();
        lib.natural(o.seconds, frames0);
        const double c1 = processCpuMs(), w1 = wallMs();
        getrusage(RUSAGE_SELF, &r1);
        const double wall = (w1 - w0) / 1000.0;
        emit("idle_cpu_pct", (c1 - c0) / (w1 - w0) * 100.0);
        emit("idle_wall_s", wall);
        emit("idle_wakeups_per_s", static_cast<double>(r1.ru_nvcsw - r0.ru_nvcsw) / wall);
        emit("idle_preempt_per_s", static_cast<double>(r1.ru_nivcsw - r0.ru_nivcsw) / wall);
        emit("idle_frames_per_s", static_cast<double>(frames0) / wall);
        return 0;
    }

    int runLatency()
    {
        if (!lib.init()) return 2;
        lib.build('B');
        settle(8);
        if (!inj.open()) return 3;
        usleep(100000);
        double ms[256], fr[256];
        int got = 0;
        const int trials = o.n > 200 ? 200 : o.n;
        for (int t = 0; t < trials; ++t)
        {
            lib.resetChanged();
            inj.move(latencyX(), latencyY());
            inj.flush();
            settle(3);
            const long long f0 = lib.frames();
            inj.click();
            const double t0 = wallMs();
            cmp_inj_xflush();
            // the library's own loop until the click shows
            const double deadline = t0 + 300.0;
            while (!lib.changed() && wallMs() < deadline) lib.pumpOnce();
            const double t1 = wallMs();
            if (!lib.changed()) continue;
            ms[got] = t1 - t0;
            fr[got] = static_cast<double>(lib.frames() - f0);
            ++got;
            // put it back
            lib.resetChanged();
            inj.click();
            cmp_inj_xflush();
            const double d2 = wallMs() + 300.0;
            while (!lib.changed() && wallMs() < d2) lib.pumpOnce();
        }
        if (got == 0) { printf("latency_ms=-1\n"); return 1; }
        double mn = ms[0], mx = ms[0];
        for (int i = 1; i < got; ++i) { if (ms[i] < mn) mn = ms[i]; if (ms[i] > mx) mx = ms[i]; }
        emit("latency_ms_min", mn);
        emit("latency_ms_max", mx);
        emit("latency_frames_med", median(fr, got));
        emit("latency_ms_med", median(ms, got));
        emitL("latency_trials", got);
        return 0;
    }
};

template <class L>
int run(L& lib, int argc, char** argv)
{
    Runner<L> r(lib);
    if (!parse(argc, argv, r.o))
    {
        fprintf(stderr, "usage: %s <scene A..F> <frames|idlecpu|latency> [--n=N] [--gl|--soft] [--build-ic] [--seconds=S]\n", argv[0]);
        return 2;
    }
    setvbuf(stdout, nullptr, _IOLBF, 0);
    int rc;
    if (!strcmp(r.o.mode, "frames")) rc = r.runFrames();
    else if (!strcmp(r.o.mode, "idlecpu")) rc = r.runIdleCpu();
    else if (!strcmp(r.o.mode, "latency")) rc = r.runLatency();
    else rc = 2;
    fflush(stdout);
    return rc;
}

} // namespace cmp
