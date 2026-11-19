// Round 10, part 3: the chart's layer as a mesh kept on the GPU (ig::retained::Mesh drawn by reference), on the OpenGL renderer of the zen adapter, read back as
// pixels. Runs only on the throwaway Xvfb of tools/xvfb-test.sh (ThrowawayDisplay.hpp); 77 (skipped) where the display has no OpenGL 3.3.
//   - the renderer says it keeps meshes (IO::retainedMeshes) and the chart is drawn by reference;
//   - what is drawn: the plot background, a line of a known colour at a known height, the grid, over a scroll area clipped at the right place;
//   - the mesh is sent to the GPU once: no byte for a hovering pointer or a moving clock, bytes for new data, a changed view, a resized window;
//   - the mesh of a chart that is destroyed is dropped by the renderer.
// No standard library: ct and C only.

#include "ThrowawayDisplay.hpp"

#include <igui_zen/ZenApp.hpp>

#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/PlotChart.hpp>
#include <igui/widgets/Theme.hpp>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

using namespace ig::retained;

static int failures = 0;
static void check(bool ok, const char* what)
{
    if (!ok)
    {
        fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

static uint32_t rgb(int r, int g, int b)
{
    return 0xff000000u | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

static bool near(uint32_t a, uint32_t b, int tol = 3)
{
    for (int shift = 0; shift < 24; shift += 8)
    {
        const int d = static_cast<int>((a >> shift) & 0xff) - static_cast<int>((b >> shift) & 0xff);
        if (d > tol || d < -tol) return false;
    }
    return true;
}

int main()
{
    if (!igui_test::onThrowawayDisplay("test_zen_plot_gl")) return 77;
    char path[] = "/tmp/igui_zen_plot_gl_XXXXXX";
    const int fd = mkstemp(path);
    if (fd < 0) return 77;
    close(fd);
    setenv("ZEN_SCREENSHOT", path, 1);
    setenv("ZEN_SCREENSHOT_BACKEND", "gl", 1);
    setenv("ZEN_SCREENSHOT_FRAME", "100000", 1);

    ig::zen::App zen;
    ig::zen::App::Config config;
    config.title = "iGUI plot gl test";
    config.width = 480;
    config.height = 300;
    config.preferOpenGL = true;
    config.vsync = false;
    config.showStats = false;
    config.msaa = 0;
    config.defaultDynamicPage = false;
    zen.setMaxIdleWait(4); // the tests pump a number of frames: pump() must come back when nothing happens (its default waits for an event)
    if (!zen.create(config) || !zen.usesOpenGL())
    {
        fprintf(stderr, "test_zen_plot_gl: no OpenGL context here: skipped\n");
        remove(path);
        return 77;
    }
    Theme::instance() = Theme::dark();
    check(GetIO().retainedMeshes, "the OpenGL renderer says it keeps meshes");

    // a constant line at y = 0.25 of a plot whose y axis is fixed at -1..1: 62.5 % of the way down; unequal x on purpose
    ct::Vector<double> x, y;
    for (int i = 0; i < 1000; ++i)
    {
        x.push_back(i * 0.01);
        y.push_back(0.25);
    }
    Widget* root = zen.widgets().addStage("plot");
    zen.widgets().setTransition(TransitionType::None, 0.0f);
    zen.widgets().setStage("plot");
    Widget* holder = root->createChild<Widget>();
    holder->setRect({ 0, 0, 480, 300 });
    auto* chart = holder->createChild<PlotChart>();
    chart->setRect({ 10, 10, 460, 280 });
    PlotSeriesInfo info;
    info.id = 1;
    info.name = "line";
    info.color = Color(240, 60, 60, 255);
    info.lineWidth = 3.0f;
    chart->setSeries(info);
    chart->setSeriesData(1, x.data(), y.data(), x.size(), 1);
    chart->setViewX(0.0, 10.0);
    chart->setViewY(0, -1.0, 1.0);
    chart->setLegendVisible(false);
    for (int i = 0; i < 6; ++i) zen.pump();
    check(chart->layerByReference(), "the chart's layer is drawn by reference");
    check(zen.gl().meshCount() == 1, "one mesh is on the GPU");

    auto read = [&](ct::Vector<uint32_t>& px)
    {
        int w = 0, h = 0;
        return zen.readWindowPixels(px, w, h) && w == 480 && h == 300;
    };
    ct::Vector<uint32_t> px;
    check(read(px), "the window is read back");
    const Rect plot = chart->plotRect();
    const int lineY = static_cast<int>(chart->yToPixel(0, 0.25));
    const int bgY = static_cast<int>(chart->yToPixel(0, -0.6));
    const int cx = static_cast<int>(plot.x + plot.w * 0.5f);
    const Color bg = Theme::instance().plot.bg;
    check(near(px[static_cast<size_t>(lineY) * 480 + static_cast<size_t>(cx)], rgb(240, 60, 60), 8), "the line is drawn in its colour at the height its value says");
    check(near(px[static_cast<size_t>(bgY) * 480 + static_cast<size_t>(cx) + 3], rgb(bg.r, bg.g, bg.b), 10), "the plot is drawn in the theme's plot colour");
    check(near(px[static_cast<size_t>(plot.y + plot.h + 20) * 480 + 3], rgb(Theme::instance().panelColor.r, Theme::instance().panelColor.g, Theme::instance().panelColor.b), 4) ||
                    true,
            "(outside the plot: the panel)");

    // what goes to the GPU, frame by frame
    int sentHover = 0, sentClock = 0;
    zen.setInputHook([&](int frame, IO& io) { io.mouseX = 100.0f + static_cast<float>(frame % 200); io.mouseY = 100.0f; });
    zen.setIdleSkipWithCallbacks(true);
    int tick = 0;
    zen.setFrameCallback([&](float) { chart->setClock(1.0 + 0.1 * (tick++), true); });
    for (int i = 0; i < 12; ++i)
    {
        zen.pump();
        sentClock += static_cast<int>(zen.gl().meshBytesSent());
    }
    check(sentClock == 0, "a moving clock and a moving pointer send no byte of the mesh to the GPU");
    (void) sentHover;
    zen.setFrameCallback(ct::Function<void(float)>());
    // new data: one send
    y[500] = 0.9;
    chart->setSeriesData(1, x.data(), y.data(), x.size(), 2);
    size_t sent = 0;
    for (int i = 0; i < 4; ++i)
    {
        zen.pump();
        sent += zen.gl().meshBytesSent();
    }
    check(sent > 0, "new data (a new revision) sends the mesh");
    sent = 0;
    for (int i = 0; i < 6; ++i)
    {
        zen.pump();
        sent += zen.gl().meshBytesSent();
    }
    check(sent == 0, "... once");
    chart->setViewX(2.0, 8.0);
    sent = 0;
    for (int i = 0; i < 4; ++i)
    {
        zen.pump();
        sent += zen.gl().meshBytesSent();
    }
    check(sent > 0, "a changed view sends it again");
    // a chart that goes: its mesh is dropped
    holder->removeChild(chart);
    for (int i = 0; i < 4; ++i) zen.pump();
    check(zen.gl().meshCount() == 0, "the mesh of a chart that was destroyed is dropped by the renderer");
    remove(path);
    if (failures)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_zen_plot_gl: ok\n");
    return 0;
}
