// Headless benchmark of the retained widgets' hot paths, on an editor-sized window
// (tests/EditorTree.hpp: menu bar, tool bars, a side panel of ~50 labels, sliders,
// combos and check boxes, and a timeline of 300 or 3000 clips): the cost of a frame
// where nothing changed (the apps run continuously, so this is the idle CPU), of the
// layout, of the update alone, of measuring text, of mouse movement over the timeline
// and of a clip drag. Every figure is the best of --reps repetitions (CPU time of the
// process, so a busy machine does not move it), in microseconds per iteration, with
// the heap allocations one iteration makes. No standard library: ct and C only.
//
//   igui_retained_bench [--reps=7] [--scale=1] [--only=substring] [--trace-allocs] [--profile] [--hash]
#include "../../tests/EditorTree.hpp"

#include <igui_zen/SoftRaster.hpp>

#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <ucontext.h>

#include "../../tests/AllocCounter.hpp"

using namespace rh;

namespace
{

// --profile: a sampling profiler without perf: every millisecond of CPU time the program counter
// is recorded, and printed at the end as addresses with their counts (pipe through
// scripts that run addr2line on them: tools in the build log of this change).
unsigned long long gSamples[200000];
volatile unsigned long gSampleCount = 0;
void onProf(int, siginfo_t*, void* ctx)
{
    if (gSampleCount < sizeof gSamples / sizeof gSamples[0])
        gSamples[gSampleCount++] = static_cast<unsigned long long>(static_cast<ucontext_t*>(ctx)->uc_mcontext.gregs[REG_RIP]);
}
void profileStart()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = onProf;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGPROF, &sa, nullptr);
    struct itimerval it;
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 500;
    it.it_value = it.it_interval;
    setitimer(ITIMER_PROF, &it, nullptr);
}
void profileStop()
{
    struct itimerval it;
    memset(&it, 0, sizeof it);
    setitimer(ITIMER_PROF, &it, nullptr);
    for (unsigned long i = 0; i < gSampleCount; ++i) printf("PC %llx\n", gSamples[i]);
}

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

    if (wanted(scene, "draw_list_size"))
    {
        // What a frame produces: vertices, indices and draw commands (the work of the backend).
        const DrawList& dl = GetDrawList();
        printf("%-34s %10u vertices %u indices %u commands\n", scene, static_cast<unsigned>(dl.vertices().size()),
                static_cast<unsigned>(dl.indices().size()), static_cast<unsigned>(dl.commands().size()));
    }

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

    // The idle frame in other states: a text edit with the focus (a blinking cursor), a menu
    // open, a drop-down open, a tooltip showing.
    if (wanted(scene, "idle_frame_text_focus"))
    {
        h.clickWidget(ed.notes);
        h.frames(2);
        report(scene, "idle_frame_text_focus", measure(200, [&] { h.frame(); }));
        ed.menuBar->openMenu(1);
        h.frames(3);
        report(scene, "idle_frame_menu_open", measure(200, [&] { h.frame(); }));
        ed.menuBar->closeMenu();
        h.frames(30);
        ed.combos[1]->openDropdown();
        h.frames(30);
        report(scene, "idle_frame_combo_open", measure(200, [&] { h.frame(); }));
        h.app.closePopup();
        h.frames(30);
        ed.sliders[0]->setTooltip("a tooltip");
        float x = 0, y = 0;
        Harness::centre(ed.sliders[0], x, y);
        h.moveTo(x, y);
        h.io().deltaTime = 1.0f;
        h.frames(3);
        h.io().deltaTime = 1.0f / 60.0f;
        report(scene, "idle_frame_tooltip", measure(200, [&] { h.frame(); }));
        h.moveTo(5.0f, 5.0f);
        h.frames(2);
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
    if (!wanted("widget", name) && !wanted("widget", "geometry")) return;
    Result r = measure(wanted("widget", name) ? 200 : 1, [&] { h.frame(); });
    r.bestUs /= 200.0;
    r.allocs /= 200.0;
    char metric[64];
    snprintf(metric, sizeof metric, "%s_per_widget", name);
    if (wanted("widget", name)) report("widget", metric, r);
    if (wanted("widget", "geometry"))
    {
        const DrawList& dl = GetDrawList();
        printf("%-34s %10.1f vertices %.1f indices per widget\n", name, static_cast<double>(dl.vertices().size()) / 200.0,
                static_cast<double>(dl.indices().size()) / 200.0);
    }
}

// The software rasteriser (the zen adapter without OpenGL) on the editor window: a frame whose draw
// data did not change is copied from its cache (the data is hashed to tell), a changed one is drawn.
void softRasterScene()
{
    // baked fonts only, and the atlas as a texture of the rasteriser itself (the handles the
    // other scenes use are not memory it can read)
    Harness h(1600.0f, 900.0f, false);
    ig::zen::SoftRaster raster;
    h.atlas().setTexture(raster.createTexture(h.atlas().width(), h.atlas().height(), h.atlas().pixels()));
    EditorTree ed;
    ed.build(h, 300);
    h.relayout();
    h.frames(5);
    ct::Vector<uint32_t> pixels;
    pixels.resize(1600u * 900u);
    const uint32_t bg = 0xff181a1eu;
    raster.render(*GetDrawData(), pixels.data(), 1600, 900, 1600, bg);
    // render() reads the draw data of the last frame; the frame itself is not repeated here
    RUN_IF("softraster", "cached_frame", 100, [&] { raster.render(*GetDrawData(), pixels.data(), 1600, 900, 1600, bg); });
    RUN_IF("softraster", "full_render_aa2", 5,
            [&]
            {
                static uint32_t shade = 0;
                shade ^= 1; // a changed background: the cache is not used
                raster.render(*GetDrawData(), pixels.data(), 1600, 900, 1600, bg + shade);
            });
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
    bool traceIdle = false, profile = false, hashOnly = false;
    for (int i = 1; i < argc; ++i)
    {
        if (!strncmp(argv[i], "--reps=", 7)) gReps = atoi(argv[i] + 7);
        else if (!strncmp(argv[i], "--scale=", 8))
            gScale = static_cast<float>(atof(argv[i] + 8));
        else if (!strncmp(argv[i], "--only=", 7))
            gOnly = argv[i] + 7;
        else if (!strcmp(argv[i], "--trace-allocs"))
            traceIdle = true;
        else if (!strcmp(argv[i], "--profile"))
            profile = true;
        else if (!strcmp(argv[i], "--hash"))
            hashOnly = true;
    }
    if (traceIdle)
    {
        // One idle frame of the editor window, every allocation with its call stack.
        Harness h(1600.0f, 900.0f);
        EditorTree ed;
        ed.build(h, 300);
        h.relayout();
        h.frames(5);
        gTraceAllocs = true;
        h.frame();
        gTraceAllocs = false;
        shutdown();
        return 0;
    }
    if (hashOnly)
    {
        // A checksum of what the frames of the editor window draw (vertices, indices, commands): the
        // same before and after a change that is only meant to make drawing faster.
        for (int clips = 300; clips <= 3000; clips *= 10)
        {
            Harness h(1600.0f, 900.0f);
            EditorTree ed;
            ed.build(h, clips);
            h.relayout();
            h.frames(5);
            h.clickWidget(ed.notes);
            h.frame();
            ed.menuBar->openMenu(1);
            h.frames(3);
            const DrawList& dl = GetDrawList();
            unsigned long long hv = 1469598103934665603ull;
            auto mix = [&hv](const void* p, size_t n)
            {
                const unsigned char* b = static_cast<const unsigned char*>(p);
                for (size_t i = 0; i < n; ++i) hv = (hv ^ b[i]) * 1099511628211ull;
            };
            mix(dl.vertices().data(), dl.vertices().size() * sizeof(DrawVertex));
            mix(dl.indices().data(), dl.indices().size() * sizeof(uint32_t));
            for (size_t i = 0; i < dl.commands().size(); ++i)
            {
                const DrawCmd& c = dl.commands()[i];
                mix(&c.indexOffset, sizeof c.indexOffset);
                mix(&c.indexCount, sizeof c.indexCount);
                mix(&c.clip, sizeof c.clip);
            }
            printf("draw checksum with %d clips: %016llx (%u vertices, %u indices, %u commands)\n", clips, hv,
                    static_cast<unsigned>(dl.vertices().size()), static_cast<unsigned>(dl.indices().size()),
                    static_cast<unsigned>(dl.commands().size()));
        }
        shutdown();
        return 0;
    }
    if (profile)
    {
        // the idle frame of the editor window with 300 clips, many times, sampled
        Harness h(1600.0f, 900.0f);
        EditorTree ed;
        ed.build(h, 300);
        h.relayout();
        h.frames(5);
        profileStart();
        for (int i = 0; i < 60000; ++i) h.frame();
        profileStop();
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
    softRasterScene();
    textScene();
    shutdown();
    return 0;
}
