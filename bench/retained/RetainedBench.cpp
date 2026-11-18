// Headless benchmark of the retained widgets' hot paths, on an editor-sized window
// (tests/EditorTree.hpp: menu bar, tool bars, a side panel of ~50 labels, sliders,
// combos and check boxes, and a timeline of 300 or 3000 clips): the cost of a frame
// where nothing changed (the apps run continuously, so this is the idle CPU), of the
// layout, of the update alone, of measuring text, of mouse movement over the timeline
// and of a clip drag. Every figure is the best of --reps repetitions (CPU time of the
// process, so a busy machine does not move it), in microseconds per iteration, with
// the heap allocations one iteration makes. No standard library: ct and C only.
//
//   igui_retained_bench [--reps=7] [--scale=1] [--only=substring] [--trace-allocs] [--profile] [--profile-only] [--hash]
//   --profile-only samples the timed loops of the scenes --only selects and prints the program counters (PC lines)
//
// Comparing two builds (A/B of a change): a row is not only what its code does but where the linker put that code and where
// malloc put its buffers. With the code of a row unchanged, 400 bytes of NOP appended to an unrelated object file moved
// loops/pianoroll_32_beats between 16.10 and 17.61 us (9%), and a change that only resizes a big buffer moved
// softraster/cached_frame by 5% (see the note above it). So a difference under ~10% on one row, with the functions it runs
// byte-for-byte equal in both binaries (compare them with objdump), is placement until shown otherwise: bench at least six
// alternating runs per side pinned to one core, compare the minima, and repeat with -falign-functions=64 -falign-loops=64.
// docs/FONTS-GL.md has the numbers and the sweep.
#include "../../tests/EditorTree.hpp"

#include <igui/widgets/AppWidgets.hpp>
#include <igui/widgets/ComboBox.hpp>
#include <igui/widgets/TextInputWidgets.hpp>
#include <igui/widgets/AssetBrowser.hpp>
#include <igui/widgets/AudioWidgets.hpp>
#include <igui/widgets/ConsoleWidget.hpp>
#include <igui/widgets/DataWidgets.hpp>
#include <igui/widgets/InputWidgets.hpp>
#include <igui/widgets/ScrollWidgets.hpp>
#include <igui/widgets/SpinSlider.hpp>
#include <igui/widgets/TreePropertyColorWidgets.hpp>
#include <igui_zen/SoftRaster.hpp>

#include <dlfcn.h>
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
void profileStopQuiet()
{
    struct itimerval it;
    memset(&it, 0, sizeof it);
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

bool gProfileMeasure = false; // --profile-only: the timed loops of the selected scene are sampled
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
        if (gProfileMeasure) profileStart();
        const double t0 = cpuUs();
        for (int i = 0; i < iters; ++i) body();
        const double dt = (cpuUs() - t0) / static_cast<double>(iters);
        if (gProfileMeasure) profileStopQuiet();
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
    // cached_frame is a hash of the draw data and a copy of 1600x900 pixels (5.76 MB) from the rasteriser's cache to `pixels`.
    // Its time moves by about 5% with where glibc puts those two buffers, on the brk heap or in mmap chunks, which the dynamic mmap
    // threshold decides (freeing a big block raises it to that block's size, so the sizes of the atlas buffers the earlier scenes
    // free change it). To compare two builds, pin it for both: GLIBC_TUNABLES=glibc.malloc.mmap_threshold=33554432.
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

// DataGrid and TreeGrid with thousands of rows, scrolled to the middle: only the rows on screen are
// painted, so what is measured is the cost of the cells in view (28 rows of 6 columns on this window).
// kind 0: short texts (they fit, and stay in the string's own buffer: 11 characters), 1: texts of 30
// characters in columns that hold them (a copy of each needs the heap), 2: the same kind of text in
// columns too narrow for it, in every cell. kind 3: the short texts of kind 0 in a tree whose first
// column is wide enough for the deepest node (the same as kind 0 without the elision of that column).
void gridScene(const char* scene, bool tree, int kind)
{
    Harness h(1280.0f, 720.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(0.0f);
    auto text = [kind](int row, int col)
    {
        if (kind == 0 || kind == 3) return String("Item ") + String::number(row) + "." + String::number(col);
        return String("Entrevista ") + String::number(row) + " com o cliente " + String::number(col);
    };
    const int columns = 6;
    const float columnWidth = kind == 1 ? 280.0f : 140.0f;
    if (!tree)
    {
        DataGrid* grid = box->createChild<DataGrid>();
        grid->setStretch(1.0f);
        for (int c = 0; c < columns; ++c) grid->addColumn(String("Coluna ") + String::number(c), columnWidth);
        for (int r = 0; r < 5000; ++r)
        {
            ct::Vector<String> cells;
            for (int c = 0; c < columns; ++c) cells.push_back(text(r, c));
            grid->addRow(cells);
        }
    }
    else
    {
        TreeGrid* grid = box->createChild<TreeGrid>();
        grid->setStretch(1.0f);
        for (int c = 0; c < columns; ++c) grid->addColumn(String("Coluna ") + String::number(c), c == 0 && kind == 3 ? 400.0f : columnWidth);
        int n = 0;
        auto cells = [&](int depth)
        {
            ct::Vector<String> v;
            for (int c = 0; c < columns; ++c) v.push_back(text(n, c) + String::number(depth));
            ++n;
            return v;
        };
        for (int a = 0; a < 50; ++a)
        {
            TreeGrid::Node* root = grid->addRoot(cells(0));
            for (int b = 0; b < 20; ++b)
            {
                TreeGrid::Node* child = root->addChild(cells(1));
                for (int d = 0; d < 4; ++d) child->addChild(cells(2));
            }
        }
        grid->rebuild();
    }
    h.relayout();
    h.frames(3);
    float x = 0, y = 0;
    Harness::centre(box->children()[0], x, y);
    h.moveTo(x, y);
    h.wheel(-1250.0f); // 2400 rows down (a notch is two rows)
    h.moveTo(5.0f, 5.0f);
    h.frames(3);
    RUN_IF(scene, "paint_only", 200,
            [&]
            {
                NewFrame();
                h.app.paint(*GetDrawData(), h.font(), nullptr);
                Render();
            });
    RUN_IF(scene, "frame", 200, [&] { h.frame(); });
    // The wheel moving the list a notch (two rows) up and down: a frame each, with update and layout.
    if (wanted(scene, "scroll_frame"))
    {
        h.moveTo(x, y);
        report(scene, "scroll_frame", measure(200,
                [&]
                {
                    static int step = 0;
                    h.wheel((step++ & 1) ? 1.0f : -1.0f);
                }));
    }
    if (wanted(scene, "draw_list_size"))
    {
        const DrawList& dl = GetDrawList();
        printf("%-34s %10u vertices %u indices %u commands\n", scene, static_cast<unsigned>(dl.vertices().size()),
                static_cast<unsigned>(dl.indices().size()), static_cast<unsigned>(dl.commands().size()));
    }
}

// A PropertyGrid of 200 rows (a section, then rows of every kind in turn), the first screenful in view:
// kind 0: short names and values (they fit), 1: names and values too long for their columns.
void propertyScene(const char* scene, int kind)
{
    Harness h(480.0f, 720.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(0.0f);
    PropertyGrid* grid = box->createChild<PropertyGrid>();
    grid->setStretch(1.0f);
    auto name = [kind](int i)
    {
        return kind == 0 ? String("Propriedade ") + String::number(i)
                         : String("Propriedade numero ") + String::number(i) + " com um nome bem mais comprido do que a coluna";
    };
    ct::Vector<String> options;
    options.push_back(kind == 0 ? String("Opcao") : String("Uma opcao com um texto bem mais comprido do que a caixa"));
    options.push_back("Outra");
    for (int i = 0; i < 200; ++i)
    {
        switch (i % 10)
        {
            case 0: grid->addSection(name(i)); break;
            case 1: grid->addString(name(i), kind == 0 ? String("valor") : String("um valor de texto bem mais comprido do que a caixa")); break;
            case 2: grid->addFloat(name(i), 0.5f, 0.0f, 1.0f); break;
            case 3: grid->addInt(name(i), 3, 0, 10); break;
            case 4: grid->addBool(name(i), true); break;
            case 5: grid->addColor(name(i), Color(200, 40, 40, 255)); break;
            case 6: grid->addCombo(name(i), options, 0); break;
            case 7: grid->addVec3(name(i), 1.0f, 2.0f, 3.0f, 0.0f, 10.0f); break;
            case 8: grid->addButton(name(i)); break;
            default: grid->addRange(name(i), 2.0f, 6.0f, 0.0f, 10.0f); break;
        }
    }
    h.relayout();
    h.frames(3);
    RUN_IF(scene, "paint_only", 300,
            [&]
            {
                NewFrame();
                h.app.paint(*GetDrawData(), h.font(), nullptr);
                Render();
            });
    RUN_IF(scene, "frame", 300, [&] { h.frame(); });
    if (wanted(scene, "draw_list_size"))
    {
        const DrawList& dl = GetDrawList();
        printf("%-34s %10u vertices %u indices %u commands\n", scene, static_cast<unsigned>(dl.vertices().size()),
                static_cast<unsigned>(dl.indices().size()), static_cast<unsigned>(dl.commands().size()));
    }
}

// What an input event costs on top of an idle frame, and what it allocates: the pointer moving over widgets
// that react (a button, a slider, a list, a grid, a text field) and over one that does not (a label), and
// keys in a text field, a text editor and a list. Every figure is a whole frame (the update with the event,
// the layout it asks for and the paint), so the idle frame of the same window is the zero.
void inputScene()
{
    Harness h(900.0f, 700.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    Label* label = box->createChild<Label>("Um texto que nao faz nada");
    Button* button = box->createChild<Button>("Botao");
    Slider* slider = box->createChild<Slider>(0.0f, 1.0f, 0.3f);
    TextInput* field = box->createChild<TextInput>("um campo de texto com bastante texto dentro");
    ComboBox* combo = box->createChild<ComboBox>();
    combo->addItem("um");
    combo->addItem("dois");
    String doc;
    for (int i = 0; i < 40; ++i) doc += String("uma linha de texto numero ") + String::number(i) + " do documento\n";
    TextEdit* editor = box->createChild<TextEdit>(doc);
    editor->setStretch(1.0f);
    ListBox* list = box->createChild<ListBox>();
    for (int i = 0; i < 1000; ++i) list->addItem(String("Item ") + String::number(i));
    list->setStretch(1.0f);
    DataGrid* grid = box->createChild<DataGrid>();
    for (int c = 0; c < 4; ++c) grid->addColumn(String("Coluna ") + String::number(c), 120.0f);
    for (int r = 0; r < 1000; ++r) grid->addRow({ String::number(r), "b", "c", "d" });
    grid->setStretch(1.0f);
    h.relayout();
    h.frames(5);
    RUN_IF("input", "idle_frame", 300, [&] { h.frame(); });
    if (wanted("input", "trace_idle_frame"))
    {
        // one idle frame of this window, every allocation with its call stack (--only=trace_idle_frame)
        gTraceAllocs = true;
        h.frame();
        gTraceAllocs = false;
    }
    // the pointer moving over a widget: a step of a pixel each frame, back and forth
    auto sweep = [&](const char* metric, Widget* w)
    {
        if (!wanted("input", metric)) return;
        const Rect r = w->absoluteRect();
        const float y = r.y + (r.h > 40.0f ? 40.0f : r.h * 0.5f);
        h.moveTo(r.x + 20.0f, y);
        h.frames(2);
        int step = 0;
        report("input", metric, measure(300,
                [&]
                {
                    step = (step + 1) % 60;
                    h.moveTo(r.x + 20.0f + static_cast<float>(step < 30 ? step : 60 - step), y);
                }));
    };
    sweep("move_label", label);
    sweep("move_button", button);
    sweep("move_slider", slider);
    sweep("move_textinput", field);
    sweep("move_combo", combo);
    sweep("move_textedit", editor);
    sweep("move_list", list);
    sweep("move_datagrid", grid);
    // keys
    if (wanted("input", "key_textinput"))
    {
        h.clickWidget(field);
        h.frames(2);
        int n = 0;
        report("input", "key_textinput", measure(300, [&] { h.key((n++ & 1) ? Key::Right : Key::Left); }));
        report("input", "type_textinput", measure(300,
                [&]
                {
                    h.type("a");
                    h.key(Key::Backspace);
                }));
    }
    if (wanted("input", "key_textedit"))
    {
        h.clickWidget(editor);
        h.frames(2);
        int n = 0;
        report("input", "key_textedit", measure(300, [&] { h.key((n++ & 1) ? Key::Right : Key::Left); }));
        report("input", "type_textedit", measure(300,
                [&]
                {
                    h.type("a");
                    h.key(Key::Backspace);
                }));
    }
    if (wanted("input", "key_list"))
    {
        h.clickWidget(list);
        h.frames(2);
        int n = 0;
        report("input", "key_list", measure(300, [&] { h.key((n++ & 1) ? Key::Down : Key::Up); }));
    }
    if (wanted("input", "wheel_list"))
    {
        float x = 0, y = 0;
        Harness::centre(list, x, y);
        h.moveTo(x, y);
        int n = 0;
        report("input", "wheel_list", measure(300, [&] { h.wheel((n++ & 1) ? 1.0f : -1.0f); }));
    }
}

// Text fields with a lot of text: a document of 20000 lines, one line of 200000 characters, a single-line
// field with 100000: the frame, a letter typed, a key, and a big paste (what the clipboard gives).
void bigTextScene()
{
    {
        Harness h(900.0f, 600.0f);
        BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
        String doc;
        for (int i = 0; i < 20000; ++i) doc += String("linha numero ") + String::number(i) + " com um texto razoavel para ter largura\n";
        TextEdit* editor = box->createChild<TextEdit>(doc);
        editor->setStretch(1.0f);
        h.relayout();
        h.frames(3);
        h.clickWidget(editor);
        h.key(Key::Home, false, true); // Ctrl+Home: the start
        h.frames(2);
        RUN_IF("bigtext", "doc20000_frame", 100, [&] { h.frame(); });
        RUN_IF("bigtext", "doc20000_type", 20, [&] { h.type("a"); });
        RUN_IF("bigtext", "doc20000_key_down", 50, [&] { h.key(Key::Down); });
        RUN_IF("bigtext", "doc20000_key_pagedown", 50, [&] { h.key(Key::PageDown); });
        RUN_IF("bigtext", "doc20000_wheel", 50,
                [&]
                {
                    static int n = 0;
                    h.wheel((n++ & 1) ? 3.0f : -3.0f);
                });
        // paste: the clipboard holds a megabyte of text
        if (wanted("bigtext", "doc20000_paste_1MB"))
        {
            String big;
            for (int i = 0; i < 16000; ++i) big += "uma linha colada com uns sessenta e tal caracteres de texto\n";
            clipboardText() = big;
            h.key(Key::Home, false, true);
            report("bigtext", "doc20000_paste_1MB", measure(1, [&] { h.key(Key::V, false, true); }));
        }
    }
    {
        Harness h(900.0f, 300.0f);
        BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
        String line;
        for (int i = 0; i < 20000; ++i) line += "dez letras";
        TextEdit* editor = box->createChild<TextEdit>(line);
        editor->setStretch(1.0f);
        h.relayout();
        h.frames(3);
        h.clickWidget(editor);
        h.key(Key::End);
        h.frames(2);
        RUN_IF("bigtext", "line200000_frame", 20, [&] { h.frame(); });
        RUN_IF("bigtext", "line200000_type", 10, [&] { h.type("a"); });
        RUN_IF("bigtext", "line200000_key_left", 20, [&] { h.key(Key::Left); });
    }
    {
        Harness h(900.0f, 200.0f);
        BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
        String line;
        for (int i = 0; i < 10000; ++i) line += "dez letras";
        TextInput* field = box->createChild<TextInput>(line);
        h.relayout();
        h.frames(3);
        h.clickWidget(field);
        h.key(Key::End);
        h.frames(2);
        RUN_IF("bigtext", "field100000_frame", 20, [&] { h.frame(); });
        RUN_IF("bigtext", "field100000_type", 10, [&] { h.type("a"); });
        RUN_IF("bigtext", "field100000_key_left", 20, [&] { h.key(Key::Left); });
    }
}

// The widgets that draw by stepping through a range with a loop (grid lines, ticks, rulers), as one idle frame each
// on a window of 1280x720: what a change to those loops costs (the frame, with every other widget the same).
void loopScenes()
{
    {
        Harness h(1280.0f, 720.0f);
        BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
        PianoRoll* roll = box->createChild<PianoRoll>();
        roll->setStretch(1.0f);
        roll->setPitchRange(36, 96);
        for (int i = 0; i < 200; ++i) roll->addNote(40 + (i * 7) % 50, static_cast<float>(i) * 0.5f, 0.5f + 0.25f * static_cast<float>(i % 4));
        roll->setTimeRange(0.0f, 32.0f);
        h.relayout();
        h.frames(3);
        RUN_IF("loops", "pianoroll_32_beats", 300, [&] { h.frame(); });
        roll->setTimeRange(0.0f, 400.0f);
        h.frames(3);
        RUN_IF("loops", "pianoroll_400_beats", 300, [&] { h.frame(); });
    }
}

// The vertices, indices and commands of the draw list folded into a checksum (FNV-1a).
void mixDrawList(const DrawList& dl, unsigned long long& hv)
{
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
        else if (!strcmp(argv[i], "--profile-only"))
            gProfileMeasure = true;
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
            mixDrawList(dl, hv);
            printf("draw checksum with %d clips: %016llx (%u vertices, %u indices, %u commands)\n", clips, hv,
                    static_cast<unsigned>(dl.vertices().size()), static_cast<unsigned>(dl.indices().size()),
                    static_cast<unsigned>(dl.commands().size()));
        }
        // The piano roll (rows, grid lines, bar numbers, notes, playhead, keys) in a few views and bars: the
        // grid is drawn from a count worked out before the loop, and from the rectangle the paint was given,
        // which must not change what is on the screen. Only views with fewer beats than pixels (a view with
        // more draws every few beats, on purpose).
        {
            const float views[][2] = { { 0.0f, 32.0f }, { 0.0f, 400.0f }, { -5.0f, 20.0f }, { 3.5f, 9.2f }, { 100000.0f, 100040.0f } };
            const int bars[] = { 1, 3, 4, 7 };
            unsigned long long hv = 1469598103934665603ull;
            unsigned vertices = 0, indices = 0, commands = 0;
            int scenes = 0;
            for (const auto& view: views)
                for (int bpb: bars)
                {
                    Harness h(1280.0f, 720.0f);
                    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
                    PianoRoll* roll = box->createChild<PianoRoll>();
                    roll->setStretch(1.0f);
                    roll->setPitchRange(36, 96);
                    for (int i = 0; i < 200; ++i)
                        roll->addNote(40 + (i * 7) % 50, static_cast<float>(i) * 0.5f - 3.0f, 0.5f + 0.25f * static_cast<float>(i % 4));
                    roll->setTimeRange(view[0], view[1]);
                    roll->setBeatsPerBar(bpb);
                    roll->setPlayhead(view[0] + (view[1] - view[0]) * 0.37f);
                    h.relayout();
                    h.frames(2);
                    const DrawList& dl = GetDrawList();
                    mixDrawList(dl, hv);
                    vertices += static_cast<unsigned>(dl.vertices().size());
                    indices += static_cast<unsigned>(dl.indices().size());
                    commands += static_cast<unsigned>(dl.commands().size());
                    ++scenes;
                }
            printf("draw checksum of the piano roll in %d views: %016llx (%u vertices, %u indices, %u commands)\n", scenes, hv,
                    vertices, indices, commands);
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
    if (gOnly && !strncmp(gOnly, "loops", 5))
    {
        // only the loop scenes: the other scenes take seconds to build and are not measured then
        loopScenes();
        shutdown();
        return 0;
    }
    struct SampleDump
    {
        ~SampleDump()
        {
            if (gProfileMeasure)
            {
                Dl_info info;
                dladdr(reinterpret_cast<void*>(&profileStart), &info);
                for (unsigned long i = 0; i < gSampleCount; ++i)
                    printf("PC %llx\n", gSamples[i] - static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(info.dli_fbase)));
            }
        }
    } sampleDump;
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
    widgetCost("radiobutton", [](BoxLayout* b, int i) { b->createChild<RadioButton>(String("Opção ") + String::number(i)); });
    widgetCost("switch", [](BoxLayout* b, int i) { b->createChild<Switch>(String("Opção ") + String::number(i)); });
    widgetCost("splitbutton", [](BoxLayout* b, int i) { b->createChild<SplitButton>(String("Acção ") + String::number(i)); });
    widgetCost("collapsible", [](BoxLayout* b, int i) { b->createChild<Collapsible>(String("Secção ") + String::number(i), false); });
    widgetCost("statusbar", [](BoxLayout* b, int i) { b->createChild<StatusBar>()->setText(String("Pronto ") + String::number(i)); });
    widgetCost("spinbox", [](BoxLayout* b, int) { b->createChild<SpinBox>(); });
    widgetCost("breadcrumbs",
            [](BoxLayout* b, int)
            {
                ct::Vector<String> path;
                path.push_back("Projecto");
                path.push_back("Média");
                path.push_back("Clips");
                b->createChild<Breadcrumbs>()->setPath(path);
            });
    widgetCost("progressbar", [](BoxLayout* b, int i) { b->createChild<ProgressBar>()->setText(String("Passo ") + String::number(i)); });
    widgetCost("spinslider", [](BoxLayout* b, int i) { b->createChild<SpinSlider>()->setLabel(String("Ganho ") + String::number(i)); });
    widgetCost("tablayout",
            [](BoxLayout* b, int i)
            {
                TabLayout* t = b->createChild<TabLayout>();
                t->addTab<Panel>(String("Separador ") + String::number(i));
                t->addTab<Panel>("Outro");
            });
    widgetCost("listbox",
            [](BoxLayout* b, int i)
            {
                ListBox* l = b->createChild<ListBox>();
                l->addItem(String("Item ") + String::number(i));
            });
    widgetCost("assetbrowser",
            [](BoxLayout* b, int i)
            {
                AssetBrowser* a = b->createChild<AssetBrowser>();
                a->setStretch(1.0f);
                a->setPath(String("/projecto/media/clips/") + String::number(i));
                AssetItem item;
                item.name = "clip.mp4";
                a->addItem(item);
            });
    widgetCost("console",
            [](BoxLayout* b, int i)
            {
                ConsoleWidget* c = b->createChild<ConsoleWidget>();
                c->setStretch(1.0f);
                c->log(LogLevel::Info, String("Linha de registo ") + String::number(i));
            });
    widgetCost("searchbar", [](BoxLayout* b, int i) { b->createChild<SearchBar>()->setText(String("busca ") + String::number(i)); });
    softRasterScene();
    textScene();
    gridScene("datagrid_short", false, 0);
    gridScene("datagrid_fit", false, 1);
    gridScene("datagrid_long", false, 2);
    gridScene("treegrid_short", true, 0);
    gridScene("treegrid_fit", true, 1);
    gridScene("treegrid_long", true, 2);
    gridScene("treegrid_short_wide", true, 3);
    propertyScene("propgrid_short", 0);
    propertyScene("propgrid_long", 1);
    inputScene();
    bigTextScene();
    loopScenes();
    shutdown();
    return 0;
}
