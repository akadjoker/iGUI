// A page for the web check (tools/check.sh, stage web; tests/web/run_web_gl.js): the zen adapter in a browser with WebGL 2, a window of
// widgets with colours that are known, and what the page does with them read back from the framebuffer of the page itself.
//
// Every iGUI widget is made of triangles that sample a texture, and the font atlas page (which holds the white texel the solid shapes
// use) is a one-byte texture read through a swizzle on desktop: WebGL 2 has no swizzle, and a page that used it drew everything in red.
// So the colours that come out of this window say whether the whole renderer works in a browser.
//
// The page publishes window.igProbe = { frames, usesGL, renderer, glError, preset, regions: [{ name, got: [r, g, b, a], want: [r, g, b] }],
// ink: { pixels, of } }; the script that drives the browser reads it. ?preset=dark (the default) or ?preset=light.

#include <igui_zen/ZenApp.hpp>

#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/PlotChart.hpp>
#include <igui/widgets/Theme.hpp>

#include <platform.h>

#include <emscripten.h>
#include <GLES3/gl3.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

using namespace ig::retained;

EM_JS(int, igPresetIsLight, (), { return location.search.indexOf('preset=light') >= 0 ? 1 : 0; });
EM_JS(void, igPublish, (const char* json), { window.igProbe = JSON.parse(UTF8ToString(json)); });

namespace
{

struct Region
{
    const char* name;
    Widget* widget;
    Color want;
};

ig::zen::App* gZen = nullptr;
Region gRegions[8];
int gRegionCount = 0;
Label* gLabel = nullptr;
Panel* gLabelPanel = nullptr;
PlotChart* gChart = nullptr;
double gX[600], gY[600];
int gFrames = 0;
bool gLight = false;

// The centre of a widget in window pixels (the window is not scaled here: one unit is one pixel)
void centreOf(const Widget* w, int& x, int& y)
{
    const Rect r = w->absoluteRect();
    x = static_cast<int>(r.x + r.w * 0.5f);
    y = static_cast<int>(r.y + r.h * 0.5f);
}

void publish()
{
    int fbW = 0, fbH = 0;
    window_get_framebuffer_size(gZen->window(), &fbW, &fbH);
    char json[4096];
    int n = snprintf(json, sizeof json,
            "{\"frames\":%d,\"usesGL\":%s,\"renderer\":\"%s\",\"preset\":\"%s\",\"fb\":[%d,%d],\"regions\":[", gFrames,
            gZen->usesOpenGL() ? "true" : "false", gZen->usesOpenGL() ? gZen->gl().renderer() : "software", gLight ? "light" : "dark", fbW, fbH);
    for (int i = 0; i < gRegionCount; ++i)
    {
        int x = 0, y = 0;
        centreOf(gRegions[i].widget, x, y);
        unsigned char px[4] = { 0, 0, 0, 0 };
        glReadPixels(x, fbH - 1 - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        n += snprintf(json + n, sizeof json - static_cast<size_t>(n), "%s{\"name\":\"%s\",\"got\":[%d,%d,%d,%d],\"want\":[%d,%d,%d]}", i ? "," : "",
                gRegions[i].name, px[0], px[1], px[2], px[3], gRegions[i].want.r, gRegions[i].want.g, gRegions[i].want.b);
    }
    // the text of the label: how many pixels of its rectangle are neither the panel behind it nor near it, in at least two channels
    // (text is a different colour from its panel; a window that has lost two of its three channels cannot show it)
    int ink = 0, of = 0;
    {
        const Rect r = gLabel->absoluteRect();
        const Color bg = gLabelPanel->bgColor();
        const int x0 = static_cast<int>(r.x), y0 = static_cast<int>(r.y), w = static_cast<int>(r.w), h = static_cast<int>(r.h);
        static unsigned char buffer[400 * 40 * 4];
        const int cw = w < 400 ? w : 400, ch = h < 40 ? h : 40;
        glReadPixels(x0, fbH - y0 - ch, cw, ch, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
        for (int i = 0; i < cw * ch; ++i)
        {
            const int dr = buffer[i * 4] - bg.r, dg = buffer[i * 4 + 1] - bg.g, db = buffer[i * 4 + 2] - bg.b;
            const int big = (dr > 60 || dr < -60 ? 1 : 0) + (dg > 60 || dg < -60 ? 1 : 0) + (db > 60 || db < -60 ? 1 : 0);
            if (big >= 2) ++ink;
            ++of;
        }
    }
    // the chart: its layer is a Mesh, drawn by reference (a buffer of its own on the GPU): pixels in its rectangle that are not the colour of its corner
    int chartInk = 0;
    {
        const Rect r = gChart->absoluteRect();
        const int x0 = static_cast<int>(r.x), y0 = static_cast<int>(r.y), w = static_cast<int>(r.w < 400 ? r.w : 400), h = static_cast<int>(r.h < 100 ? r.h : 100);
        static unsigned char buffer[400 * 100 * 4];
        glReadPixels(x0, fbH - y0 - h, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
        const unsigned char* corner = buffer;
        for (int i = 0; i < w * h; ++i)
        {
            const int big = (buffer[i * 4] - corner[0] > 40 || corner[0] - buffer[i * 4] > 40) + (buffer[i * 4 + 1] - corner[1] > 40 || corner[1] - buffer[i * 4 + 1] > 40) +
                    (buffer[i * 4 + 2] - corner[2] > 40 || corner[2] - buffer[i * 4 + 2] > 40);
            if (big >= 1) ++chartInk;
        }
    }
    n += snprintf(json + n, sizeof json - static_cast<size_t>(n), "],\"ink\":{\"pixels\":%d,\"of\":%d},\"chart\":{\"pixels\":%d},\"glError\":%d}", ink, of, chartInk, static_cast<int>(glGetError()));
    igPublish(json);
}

void afterRender()
{
    ++gFrames;
    if (gFrames >= 6 && gFrames % 20 == 6) publish();
}

} // namespace

int main()
{
    gLight = igPresetIsLight() != 0;
    ig::zen::App zen;
    gZen = &zen;
    ig::zen::App::Config config;
    config.title = "web probe";
    config.width = 480;
    config.height = 320;
    config.preferOpenGL = true;
    config.openGLES = true;
    config.msaa = 0;
    config.showStats = false;
    config.background = 0xff0000ffu; // not the colour of any widget below: 0xAARRGGBB, a pure blue behind everything
    if (!zen.create(config)) return 1;

    WidgetApp& app = zen.widgets();
    app.setThemePreset(gLight ? "light" : "dark");
    Widget* root = app.addStage("probe");
    Widget* holder = root->createChild<Widget>();
    holder->setRect({ 0, 0, 480, 320 });

    auto panel = [&](const char* name, float x, float y, Color color, bool explicitColor) {
        Panel* p = holder->createChild<Panel>();
        p->setRect({ x, y, 110, 70 });
        if (explicitColor) p->setBgColor(color);
        gRegions[gRegionCount++] = { name, p, p->bgColor() };
        return p;
    };
    // three colours of their own, one in each channel (a window that lost two of the channels cannot draw them) and the theme's
    panel("orange", 10, 10, Color(215, 100, 30, 255), true);
    panel("green", 130, 10, Color(40, 170, 90, 255), true);
    panel("blue", 250, 10, Color(50, 90, 225, 255), true);
    panel("theme panel", 370, 10, Color(0, 0, 0, 255), false);
    gLabelPanel = panel("label panel", 10, 100, Color(0, 0, 0, 255), false);
    --gRegionCount; // (its middle is text: only its ink is looked at)
    gLabelPanel->setRect({ 10, 100, 360, 40 });
    gLabel = gLabelPanel->createChild<Label>("Text in the theme's colour");
    gLabel->setRect({ 8, 8, 340, 24 });
    holder->createChild<Button>("Button")->setRect({ 10, 160, 120, 30 });
    holder->createChild<CheckBox>("Check")->setRect({ 140, 160, 120, 30 });
    gChart = holder->createChild<PlotChart>();
    gChart->setRect({ 10, 200, 460, 110 });
    for (int i = 0; i < 600; ++i)
    {
        gX[i] = i * 0.01;
        gY[i] = sin(gX[i] * 3.0) * 40.0 + 50.0;
    }
    PlotSeriesInfo info;
    info.id = 1;
    info.name = "sine";
    gChart->setSeries(info);
    gChart->setSeriesData(1, gX, gY, 600, 1);
    gChart->fit();
    app.setStage("probe");
    // the colours are read after the layout has settled; the panels were given an absolute place, which is theirs
    zen.setAfterRenderCallback(afterRender);
    zen.run();
    return 0;
}
