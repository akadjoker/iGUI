// Headless benchmark of the retained widgets' hot paths, on an editor-sized window
// (tests/EditorTree.hpp: menu bar, tool bars, a side panel of ~50 labels, sliders,
// combos and check boxes, and a timeline of 300 or 3000 clips): the cost of a frame
// where nothing changed (the apps run continuously, so this is the idle CPU), of the
// layout, of the update alone, of measuring text, of mouse movement over the timeline
// and of a clip drag. Every figure is the best of --reps repetitions (CPU time of the
// process, so a busy machine does not move it), in microseconds per iteration, with
// the heap allocations one iteration makes. No standard library: ct and C only.
//
//   igui_retained_bench [--reps=7] [--scale=1] [--only=substring] [--trace-allocs]
#include "../../tests/EditorTree.hpp"

#include <execinfo.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// ── Allocation counter ───────────────────────────────────────────────────────
// ct allocates with malloc, the rest with operator new: both are counted.
extern "C" void* __real_malloc(size_t);
extern "C" void* __real_calloc(size_t, size_t);
extern "C" void* __real_realloc(void*, size_t);

static unsigned long long gAllocs = 0;
static bool gTrace = false; // --trace-allocs: where the allocations of one idle frame come from

static void traceAlloc()
{
    if (!gTrace) return;
    gTrace = false; // backtrace() itself may allocate
    void* frames[14];
    const int n = backtrace(frames, 14);
    backtrace_symbols_fd(frames + 2, n - 2, 2);
    if (write(2, "--\n", 3) < 0) return;
    gTrace = true;
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

using namespace rh;

namespace
{

double cpuUs()
{
    struct timespec t;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);
    return static_cast<double>(t.tv_sec) * 1e6 + static_cast<double>(t.tv_nsec) * 1e-3;
}

int gReps = 7;
float gScale = 1.0f;
const char* gOnly = nullptr;

struct Result
{
    double bestUs = 1e30;
    double allocs = 0;
};

// Runs `body` `iters` times per repetition, `reps` repetitions, after a warm-up; the best
// repetition's time per iteration, and the allocations per iteration of the last one.
template<typename F>
Result measure(int iters, F body)
{
    for (int i = 0; i < (iters < 20 ? iters : 20); ++i) body();
    Result r;
    for (int rep = 0; rep < gReps; ++rep)
    {
        const unsigned long long a0 = gAllocs;
        const double t0 = cpuUs();
        for (int i = 0; i < iters; ++i) body();
        const double dt = (cpuUs() - t0) / static_cast<double>(iters);
        if (dt < r.bestUs) r.bestUs = dt;
        r.allocs = static_cast<double>(gAllocs - a0) / static_cast<double>(iters);
    }
    return r;
}

void report(const char* scene, const char* metric, const Result& r)
{
    char name[96];
    snprintf(name, sizeof name, "%s/%s", scene, metric);
    if (gOnly && !strstr(name, gOnly)) return;
    printf("%-34s %10.2f us   %9.1f allocs\n", name, r.bestUs, r.allocs);
    fflush(stdout);
}

bool wanted(const char* scene, const char* metric)
{
    if (!gOnly) return true;
    char name[96];
    snprintf(name, sizeof name, "%s/%s", scene, metric);
    return strstr(name, gOnly) != nullptr;
}

#define RUN_IF(scene, metric, iters, body) \
    do \
    { \
        if (wanted(scene, metric)) report(scene, metric, measure(iters, body)); \
    } while (0)

void editorScene(const char* scene, int clips)
{
    Harness h(1600.0f * gScale, 900.0f * gScale);
    EditorTree ed;
    ed.build(h, clips);
    h.relayout();
    h.frames(5);

    // A frame where nothing changed: the idle CPU of an application sitting open.
    RUN_IF(scene, "idle_frame", 200, [&] { h.frame(); });

    // The same frame in pieces.
    RUN_IF(scene, "update_only", 2000, [&] { h.app.update(h.io()); });
    RUN_IF(scene, "paint_only", 200,
            [&]
            {
                NewFrame();
                h.app.paint(*GetDrawData(), h.font(), nullptr);
                Render();
            });
    RUN_IF(scene, "layout_only", 200, [&] { h.root()->layout(); });
    RUN_IF(scene, "layout_frame", 100, [&] { h.relayout(); });

    // Text: the widths of the side panel's labels, as sizeHint and paint ask for them.
    RUN_IF(scene, "label_sizehints", 500,
            [&]
            {
                float sum = 0;
                for (size_t i = 0; i < ed.labels.size(); ++i) sum += ed.labels[i]->sizeHint().x;
                if (sum < 0) printf("?");
            });
    RUN_IF(scene, "text_width_x100", 500,
            [&]
            {
                float sum = 0;
                for (int i = 0; i < 100; ++i) sum += h.app.textWidth("Parâmetro número 7 da cena");
                if (sum < 0) printf("?");
            });

    // The pointer moving over the timeline (hover, tooltip, the clip under it): a frame each.
    RUN_IF(scene, "timeline_hover_move", 300,
            [&]
            {
                static float x = 300.0f;
                x += 7.0f;
                if (x > 1500.0f) x = 300.0f;
                h.moveTo(x, 760.0f * gScale);
            });

    // A clip drag: press on a clip, then frames of mouse movement, released at the end.
    if (wanted(scene, "clip_drag_frame"))
    {
        float x = 0, y = 0;
        ed.timeline->pointAt(0, 3.0f, x, y);
        h.moveTo(x, y);
        h.down();
        const Result r = measure(300,
                [&]
                {
                    static int step = 0;
                    step = (step + 1) % 80;
                    h.moveTo(x + static_cast<float>(step), y);
                });
        h.up();
        report(scene, "clip_drag_frame", r);
    }

    // Where the idle frame goes: the same frame with parts of the window hidden.
    if (wanted(scene, "idle_frame_no_timeline"))
    {
        ed.timeline->setVisible(false);
        report(scene, "idle_frame_no_timeline", measure(200, [&] { h.frame(); }));
        ed.sidePanel->setVisible(false);
        report(scene, "idle_frame_no_tl_no_side", measure(200, [&] { h.frame(); }));
        ed.menuBar->setVisible(false);
        ed.tools->setVisible(false);
        report(scene, "idle_frame_no_tl_side_menu", measure(200, [&] { h.frame(); }));
        ed.timeline->setVisible(true);
        ed.sidePanel->setVisible(true);
        ed.menuBar->setVisible(true);
        ed.tools->setVisible(true);
        h.relayout();
    }

    // A frame with the window being resized: layout + paint every time.
    RUN_IF(scene, "resize_frame", 100,
            [&]
            {
                static int w = 0;
                w = (w + 1) % 40;
                h.resize(1600.0f * gScale - static_cast<float>(w), 900.0f * gScale);
            });
}

void timelineScene(const char* scene, int clips)
{
    Harness h(1600.0f * gScale, 500.0f * gScale);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    Timeline* tl = box->createChild<Timeline>();
    tl->setStretch(1.0f);
    for (int t = 0; t < 4; ++t) tl->addTrack("Track", t >= 2 ? TrackKind::Audio : TrackKind::Video);
    tl->setTimeRange(0.0f, 20.0f);
    Rng rng(99);
    float cursor[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < clips; ++i)
    {
        const int t = i % 4;
        const float length = 0.4f + rng.unit() * 2.0f;
        const int id = tl->addClip(t, cursor[t], cursor[t] + length, "Clip");
        tl->track(t).clips[id].media = t >= 2 ? i : -1;
        cursor[t] += length;
    }
    // A waveform for the audio clips, as the application provides it.
    tl->setPeakProvider([](int media, float from, float to) { return 0.2f + 0.6f * fabsf(sinf(from * 3.0f + static_cast<float>(media))) * (to > from ? 1.0f : 0.0f); });
    h.relayout();
    h.frames(3);
    RUN_IF(scene, "idle_frame", 200, [&] { h.frame(); });
    // Zoomed on all of it: every clip on screen.
    tl->setTimeRange(0.0f, cursor[0] > 1.0f ? cursor[0] : 1.0f);
    h.frames(3);
    RUN_IF(scene, "idle_frame_all_visible", 100, [&] { h.frame(); });
}

// What one widget of each kind costs in an idle frame: 200 of them in a box, the frame time
// divided by 200.
template<typename Make>
void widgetCost(const char* name, Make make)
{
    Harness h(800.0f, 12000.0f); // tall enough that all 200 are on screen
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(0.0f);
    box->setSpacing(0.0f);
    for (int i = 0; i < 200; ++i) make(box, i);
    h.relayout();
    h.frames(3);
    if (!wanted("widget", name)) return;
    Result r = measure(200, [&] { h.frame(); });
    r.bestUs /= 200.0;
    r.allocs /= 200.0;
    char metric[64];
    snprintf(metric, sizeof metric, "%s_per_widget", name);
    report("widget", metric, r);
}

void textScene()
{
    Harness h(800.0f, 400.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    String longName;
    for (int i = 0; i < 400; ++i) longName += "Entrevista com Ação ";
    Label* elided = box->createChild<Label>(longName);
    elided->setElide(true);
    Label* plain = box->createChild<Label>(String("short label"));
    (void) plain;
    h.relayout();
    h.frames(3);
    RUN_IF("text", "elided_8000_chars_frame", 5, [&] { h.frame(); });
}

} // namespace

int main(int argc, char** argv)
{
    bool traceIdle = false;
    for (int i = 1; i < argc; ++i)
    {
        if (!strncmp(argv[i], "--reps=", 7)) gReps = atoi(argv[i] + 7);
        else if (!strncmp(argv[i], "--scale=", 8))
            gScale = static_cast<float>(atof(argv[i] + 8));
        else if (!strncmp(argv[i], "--only=", 7))
            gOnly = argv[i] + 7;
        else if (!strcmp(argv[i], "--trace-allocs"))
            traceIdle = true;
    }
    if (traceIdle)
    {
        // One idle frame of the editor window, every allocation with its call stack.
        Harness h(1600.0f, 900.0f);
        EditorTree ed;
        ed.build(h, 300);
        h.relayout();
        h.frames(5);
        gTrace = true;
        h.frame();
        gTrace = false;
        shutdown();
        return 0;
    }
    printf("igui_retained_bench: best of %d, CPU time per iteration\n", gReps);
    editorScene("editor300", 300);
    editorScene("editor3000", 3000);
    timelineScene("timeline300", 300);
    timelineScene("timeline3000", 3000);
    widgetCost("label", [](BoxLayout* b, int i) { b->createChild<Label>(String("Parâmetro ") + String::number(i)); });
    widgetCost("label_elide", [](BoxLayout* b, int i) { b->createChild<Label>(String("Parâmetro número ") + String::number(i))->setElide(true); });
    widgetCost("button", [](BoxLayout* b, int i) { b->createChild<Button>(String("Botão ") + String::number(i)); });
    widgetCost("slider", [](BoxLayout* b, int) { b->createChild<Slider>(0.0f, 1.0f, 0.3f); });
    widgetCost("checkbox", [](BoxLayout* b, int i) { b->createChild<CheckBox>(String("Opção ") + String::number(i)); });
    widgetCost("combo", [](BoxLayout* b, int) { b->createChild<ComboBox>()->addItem("Cross dissolve"); });
    widgetCost("textinput", [](BoxLayout* b, int i) { b->createChild<TextInput>(String("texto ") + String::number(i)); });
    widgetCost("panel_empty", [](BoxLayout* b, int) { b->createChild<Panel>(); });
    textScene();
    shutdown();
    return 0;
}
