// Round 10: the cost of long lines and of the interactive chart, in a real zen window (OpenGL, or the software rasteriser), per frame:
//   GUI    the CPU time from the start of the frame (events read) to the draw data being built (NewFrame, update, paint, Render)
//   submit the time from there to the end of the present (the GL calls and the flush / the rasteriser)
// and the heap allocations a frame makes. Run on a private Xvfb (tools/xvfb-test.sh), llvmpipe is the GL there: its "submit" is the
// CPU time of a software GL, which is not what a GPU costs; the GUI column is what the library spends and is the same anywhere.
//
//   igui_plot_bench [--soft] [--frames=60] [--only=polyline|chart] [--series=16] [--points=4000] [--size=1400x900]
//
// No standard library: ct and C only.

#include <igui_zen/ZenApp.hpp>

#include <igui/widgets/ViewWidgets.hpp>

#include "../../tests/AllocCounter.hpp"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

using namespace ig::retained;

namespace
{

double now()
{
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return static_cast<double>(t.tv_sec) * 1000.0 + static_cast<double>(t.tv_nsec) / 1.0e6;
}

struct Probe
{
    double t[4] = { 0, 0, 0, 0 };
    double gui = 0.0, submit = 0.0;
    unsigned long long allocs0 = 0;
    unsigned long long allocs = 0;
    ct::Vector<double> guis, submits;
    ct::Vector<unsigned long long> allocCounts;
    bool recording = false;
};

void probeFn(int phase, void* user)
{
    Probe& p = *static_cast<Probe*>(user);
    p.t[phase] = now();
    if (phase == 0) p.allocs0 = gAllocs;
    if (phase == 1) p.allocs = gAllocs - p.allocs0;
    if (phase == 3 && p.recording)
    {
        p.guis.push_back(p.t[1] - p.t[0]);
        p.submits.push_back(p.t[3] - p.t[1]);
        p.allocCounts.push_back(p.allocs);
    }
}

double median(ct::Vector<double>& v)
{
    if (v.empty()) return 0.0;
    for (size_t i = 1; i < v.size(); ++i)
    {
        const double x = v[i];
        size_t j = i;
        while (j > 0 && v[j - 1] > x)
        {
            v[j] = v[j - 1];
            --j;
        }
        v[j] = x;
    }
    return v[v.size() / 2];
}

struct Options
{
    bool soft = false;
    int frames = 60;
    int series = 16;
    int points = 4000;
    int width = 1400, height = 900;
    const char* only = nullptr;
};

bool wanted(const Options& o, const char* what)
{
    return !o.only || strcmp(o.only, what) == 0;
}

// ── PaintContext::drawPolyline for long lines ─────────────────────────────────────────────────────────────────────────────
struct Lines
{
    ct::Vector<ct::Vector<Vec2>> series;
    void make(int count, int points, float w, float h)
    {
        series.resize(static_cast<size_t>(count));
        for (int s = 0; s < count; ++s)
        {
            ct::Vector<Vec2>& line = series[static_cast<size_t>(s)];
            line.resize(static_cast<size_t>(points));
            for (int i = 0; i < points; ++i)
            {
                const float x = 10.0f + (w - 20.0f) * static_cast<float>(i) / static_cast<float>(points - 1);
                const float t = static_cast<float>(i) * 0.01f + static_cast<float>(s);
                const float y = h * 0.5f + h * 0.3f * sinf(t * 0.7f) * cosf(t * 0.13f + static_cast<float>(s)) + 8.0f * sinf(t * 9.0f);
                line[static_cast<size_t>(i)] = { x, y };
            }
        }
    }
};

void runPolyline(const Options& o)
{
    ig::zen::App zen;
    ig::zen::App::Config config;
    config.title = "iGUI plot bench";
    config.width = o.width;
    config.height = o.height;
    config.preferOpenGL = !o.soft;
    config.vsync = false;
    config.showStats = false;
    config.msaa = 0;
    config.defaultDynamicPage = false;
    if (!zen.create(config))
    {
        printf("polyline: no window\n");
        return;
    }
    if (!o.soft && !zen.usesOpenGL()) printf("(no OpenGL here: the software rasteriser)\n");
    zen.setSkipIdleFrames(false);
    Probe probe;
    zen.setFrameProbe(probeFn, &probe);
    Lines lines;
    lines.make(o.series, o.points, static_cast<float>(o.width), static_cast<float>(o.height));
    Widget* root = zen.widgets().addStage("bench");
    zen.widgets().setTransition(TransitionType::None, 0.0f);
    zen.widgets().setStage("bench");
    auto* canvas = root->createChild<Canvas>();
    canvas->setRect({ 0, 0, static_cast<float>(o.width), static_cast<float>(o.height) });
    canvas->setOnPaint(
            [&](PaintContext& ctx, const Rect&)
            {
                for (size_t s = 0; s < lines.series.size(); ++s)
                {
                    ctx.line.SetColor(Color(static_cast<uint8_t>(60 + s * 12), static_cast<uint8_t>(200 - s * 8), static_cast<uint8_t>(90 + s * 9), 255));
                    ctx.drawPolyline(lines.series[s], 1.5f, false);
                }
            });
    for (int i = 0; i < 12; ++i) zen.pump();
    probe.recording = true;
    for (int i = 0; i < o.frames; ++i) zen.pump();
    probe.recording = false;
    const DrawData* data = GetDrawData();
    size_t vertices = 0, indices = 0, commands = 0;
    for (size_t p = 0; p < data->passes.size(); ++p)
        if (data->passes[p].list)
        {
            vertices += data->passes[p].list->vertices().size();
            indices += data->passes[p].list->indices().size();
            commands += data->passes[p].list->commands().size();
        }
    ct::Vector<double> g = probe.guis, s = probe.submits;
    ct::Vector<double> counts;
    for (size_t i = 0; i < probe.allocCounts.size(); ++i) counts.push_back(static_cast<double>(probe.allocCounts[i]));
    printf("polyline  %s  %d series x %d points, %dx%d: GUI %.3f ms/frame (paint into the draw list), submit+present %.3f ms, %zu vertices %zu indices %zu commands, allocations/frame %.0f\n",
            zen.usesOpenGL() ? "GL (llvmpipe)" : "software", o.series, o.points, o.width, o.height, median(g), median(s), vertices, indices, commands, median(counts));
}

} // namespace

int main(int argc, char** argv)
{
    Options o;
    for (int i = 1; i < argc; ++i)
    {
        const char* a = argv[i];
        if (!strcmp(a, "--soft")) o.soft = true;
        else if (!strncmp(a, "--frames=", 9)) o.frames = atoi(a + 9);
        else if (!strncmp(a, "--series=", 9)) o.series = atoi(a + 9);
        else if (!strncmp(a, "--points=", 9)) o.points = atoi(a + 9);
        else if (!strncmp(a, "--only=", 7)) o.only = a + 7;
        else if (!strncmp(a, "--size=", 7)) sscanf(a + 7, "%dx%d", &o.width, &o.height);
    }
    if (wanted(o, "polyline")) runPolyline(o);
    return 0;
}
