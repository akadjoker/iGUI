// Round 9, the theme (docs/ROUND9.md): a window with a widget of most types in it, the theme switched through the presets at
// run time, and
//   a) the dark preset draws what it always drew (a checksum of the draw list, taken before the colours moved into the theme),
//   b) under the light presets no region fill that is dark is left in the widgets that were painted with fixed dark colours,
//   c) WidgetApp::themeChanged is emitted once for each change (setTheme, a preset by name, an assignment or a field written
//      by hand that update() finds) and the layout is done again,
//   d) the text drawn is legible: the contrast ratio of each text colour against what is under it (the fills drawn before it
//      composited, read from the draw list) is 4.5 or more, in every preset,
// plus what the ConsoleWidget, the TreeView and the ToolBar were asked to do with their scroll bars and sprite-sheet icons.
// With ROUND9_SHOTS=<directory> it writes a picture of the window in each preset there (PPM). No standard library: ct and C only.
//
// Round 10 adds to it (docs/ROUND10.md): the draw list of the window in EVERY preset and the pixels of a docked splitter against
// goldens (macOSDark and the split dock were not looked at before); ThemeParts::Colors (a palette over the application's own sizes: no
// layout, the same rectangles); the Switch's theme colours; the popups (menu, combo list, tooltip, colour popup, date popup, context
// menu, completion list) in every preset: no dark fill under the light ones, text legible; and the selected row of a list in the dark
// preset at 4.5 now. With ROUND10_SHOTS=<directory> it writes a sheet of the popups of each preset (PPM).

#include "RasterScreen.hpp"
#include "Round9Window.hpp"

using namespace rh;

namespace
{

const char* const kPresets[] = { "dark", "light", "xp", "macos", "macos-dark" };
constexpr int kPresetCount = 5;

// The checksum of what the window draws in the dark preset, taken with the library of origin/main (0d86927), before any colour
// moved into the theme: the dark preset must go on drawing exactly this, whatever theme was set before. It was 0x2710448eb3ee6e18
// (7722 vertices) until the corners of rounded rectangles began to take the number of segments the error asks for, shapes made
// from tables, and commands merged (docs/ROUND9.md section 2.1): the same window, a fifth fewer vertices and fewer commands. Then it was
// 0xb5c94cffdf3657d9 (6354 vertices) until round 10 gave the selected row of a ListBox and of a ListWidget the text colour selectionText, like the
// trees (the text over the selection was 3.8:1 in the dark preset): the same vertices, one text colour changed.
constexpr uint64_t kDarkGolden = 0xf05be51a369160b1ull;
constexpr uint32_t kDarkGoldenVertices = 6354;
// Round 10: the same window in each preset (dark, light, xp, macos, macos-dark), taken at the start of the round: a change that moves
// what one of them draws has to say so (update the constant with the reason, as for the dark one above). 0 = not set yet.
constexpr uint64_t kPresetGolden[5] = { 0, 0xfcf6b2b959485339ull, 0x833a96055847bc72ull, 0x3b05a2c45f63cc44ull, 0xe763b10d01e4cee4ull };
// The pixels (software rasteriser, 1 sample) of a dock with a split, 400 by 220, in the dark preset (identical before and after round 9:
// checked against the library of 0d86927) and in macos-dark.
constexpr uint64_t kDockSplitGoldenDark = 0xd5b9fa6f2817d9b3ull;
constexpr uint64_t kDockSplitGoldenMacDark = 0x14561e934f096fffull;

float channel(unsigned char v)
{
    const float c = static_cast<float>(v) / 255.0f;
    return c <= 0.03928f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}
float luminance(float r, float g, float b)
{
    return 0.2126f * channel(static_cast<unsigned char>(clampf(r, 0, 255) + 0.5f)) +
           0.7152f * channel(static_cast<unsigned char>(clampf(g, 0, 255) + 0.5f)) +
           0.0722f * channel(static_cast<unsigned char>(clampf(b, 0, 255) + 0.5f));
}
float contrastOf(float la, float lb)
{
    return (maxf(la, lb) + 0.05f) / (minf(la, lb) + 0.05f);
}

struct Rgb
{
    float r = 0, g = 0, b = 0;
};

// ── What the draw list of the last frame holds, as triangles ────────────────────────────────────────────────────────

struct Tri
{
    float x[3], y[3];
    Color c[3];
    bool solid; // a fill: its texture coordinates are the white texel of the atlas
};

void trianglesOf(Harness&, ct::Vector<Tri>& out)
{
    out.clear();
    const DrawList& dl = GetDrawList();
    const ct::Vector<DrawVertex>& v = dl.vertices();
    if (v.empty()) return;
    // the first thing a frame draws is the background of the stage, a fill: its texture coordinates are the white texel
    const Vec2 white = { v[0].u, v[0].v };
    const ct::Vector<uint32_t>& ix = dl.indices();
    for (size_t c = 0; c < dl.commands().size(); ++c)
    {
        const DrawCmd& cmd = dl.commands()[c];
        for (uint32_t i = cmd.indexOffset; i + 2 < cmd.indexOffset + cmd.indexCount; i += 3)
        {
            Tri t;
            bool solid = true;
            for (int k = 0; k < 3; ++k)
            {
                const DrawVertex& dv = v[ix[i + static_cast<uint32_t>(k)]];
                t.x[k] = dv.x;
                t.y[k] = dv.y;
                t.c[k] = dv.color;
                if (dv.u != white.x || dv.v != white.y) solid = false;
            }
            t.solid = solid;
            out.push_back(t);
        }
    }
}

// the shortest height of the triangle: a line a pixel wide is not a fill, however long
float heightOf(const Tri& t)
{
    float longest = 0.0f;
    for (int k = 0; k < 3; ++k)
    {
        const float dx = t.x[(k + 1) % 3] - t.x[k], dy = t.y[(k + 1) % 3] - t.y[k];
        const float len = sqrtf(dx * dx + dy * dy);
        if (len > longest) longest = len;
    }
    return longest > 0.0f ? 2.0f * (0.5f * fabsf((t.x[1] - t.x[0]) * (t.y[2] - t.y[0]) - (t.x[2] - t.x[0]) * (t.y[1] - t.y[0]))) / longest : 0.0f;
}

float areaOf(const Tri& t)
{
    return 0.5f * fabsf((t.x[1] - t.x[0]) * (t.y[2] - t.y[0]) - (t.x[2] - t.x[0]) * (t.y[1] - t.y[0]));
}

// The colour of the triangle at (px, py) with its alpha, by barycentric weights; false when the point is outside.
bool colourAt(const Tri& t, float px, float py, float& r, float& g, float& b, float& a)
{
    const float d = (t.y[1] - t.y[2]) * (t.x[0] - t.x[2]) + (t.x[2] - t.x[1]) * (t.y[0] - t.y[2]);
    if (fabsf(d) < 1e-6f) return false;
    const float w0 = ((t.y[1] - t.y[2]) * (px - t.x[2]) + (t.x[2] - t.x[1]) * (py - t.y[2])) / d;
    const float w1 = ((t.y[2] - t.y[0]) * (px - t.x[2]) + (t.x[0] - t.x[2]) * (py - t.y[2])) / d;
    const float w2 = 1.0f - w0 - w1;
    if (w0 < 0 || w1 < 0 || w2 < 0) return false;
    r = w0 * t.c[0].r + w1 * t.c[1].r + w2 * t.c[2].r;
    g = w0 * t.c[0].g + w1 * t.c[1].g + w2 * t.c[2].g;
    b = w0 * t.c[0].b + w1 * t.c[1].b + w2 * t.c[2].b;
    a = (w0 * t.c[0].a + w1 * t.c[1].a + w2 * t.c[2].a) / 255.0f;
    return true;
}

bool inside(const Rect& r, float x, float y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

// (b): the fills of a region that are dark. A region fill is a run of triangles of 40 square pixels or more, opaque enough to
// be a background and not a shadow; dark is a luminance (0..255) under `limit`. Lines, icons and text are thinner or smaller.
int darkFills(const ct::Vector<Tri>& tris, const Rect& region, float limit, const ct::Vector<Color>& content, Color* first = nullptr)
{
    int n = 0;
    for (size_t i = 0; i < tris.size(); ++i)
    {
        const Tri& t = tris[i];
        if (!t.solid || areaOf(t) < 40.0f || heightOf(t) < 3.0f) continue;
        if (!inside(region, (t.x[0] + t.x[1] + t.x[2]) / 3.0f, (t.y[0] + t.y[1] + t.y[2]) / 3.0f)) continue;
        if (t.c[0].a < 128 || t.c[1].a < 128 || t.c[2].a < 128) continue;
        const float luma = 0.299f * t.c[0].r + 0.587f * t.c[0].g + 0.114f * t.c[0].b;
        bool isContent = false;
        for (size_t k = 0; k < content.size(); ++k)
            if (content[k].r == t.c[0].r && content[k].g == t.c[0].g && content[k].b == t.c[0].b) isContent = true;
        if (luma < limit && !isContent)
        {
            if (n == 0 && first) *first = t.c[0];
            ++n;
        }
    }
    return n;
}

// (d): every glyph triangle of the region, against what the fills drawn before it left under it.
struct Legibility
{
    int glyphs = 0;
    int failed = 0;
    float worst = 1000.0f;
    Color worstText;
    Color worstBg;
    float failX = 0.0f, failY = 0.0f; // where the first glyph under the floor is
};

Legibility legibility(const ct::Vector<Tri>& tris, const Rect& region, float atLeast, const ct::Vector<Color>& exempt)
{
    Legibility out;
    for (size_t i = 0; i < tris.size(); ++i)
    {
        const Tri& g = tris[i];
        if (g.solid) continue;
        const float px = (g.x[0] + g.x[1] + g.x[2]) / 3.0f, py = (g.y[0] + g.y[1] + g.y[2]) / 3.0f;
        if (!inside(region, px, py)) continue;
        if (g.c[0].a < 40) continue;
        // what is under it: the fills before this triangle, composited in order
        Rgb bg;
        for (size_t j = 0; j < i; ++j)
        {
            const Tri& f = tris[j];
            if (!f.solid) continue;
            float r, gg, b, a;
            if (!colourAt(f, px, py, r, gg, b, a)) continue;
            bg.r = bg.r * (1 - a) + r * a;
            bg.g = bg.g * (1 - a) + gg * a;
            bg.b = bg.b * (1 - a) + b * a;
        }
        const float ta = g.c[0].a / 255.0f;
        const float tr = g.c[0].r * ta + bg.r * (1 - ta), tg = g.c[0].g * ta + bg.g * (1 - ta), tb = g.c[0].b * ta + bg.b * (1 - ta);
        const float ratio = contrastOf(luminance(tr, tg, tb), luminance(bg.r, bg.g, bg.b));
        bool skip = false;
        for (size_t e = 0; e < exempt.size(); ++e)
            if (exempt[e].r == g.c[0].r && exempt[e].g == g.c[0].g && exempt[e].b == g.c[0].b) skip = true;
        ++out.glyphs;
        const float need = skip ? 1.5f : atLeast;
        if (ratio < need && out.failed == 0)
        {
            out.failX = px;
            out.failY = py;
            if (getenv("ROUND9_DEBUG"))
                for (size_t q = 0; q < tris.size(); ++q)
                    if (!tris[q].solid && fabsf((tris[q].x[0] + tris[q].x[1] + tris[q].x[2]) / 3.0f - px) < 200.0f && fabsf((tris[q].y[0] + tris[q].y[1] + tris[q].y[2]) / 3.0f - py) < 40.0f)
                        printf("      glyph tri %zu colour %d,%d,%d at %.1f,%.1f\n", q, tris[q].c[0].r, tris[q].c[0].g, tris[q].c[0].b, (tris[q].x[0] + tris[q].x[1] + tris[q].x[2]) / 3.0f, (tris[q].y[0] + tris[q].y[1] + tris[q].y[2]) / 3.0f);
            if (getenv("ROUND9_DEBUG"))
                for (size_t q = 0; q < i; ++q)
                {
                    float rr, gg2, bb, aa;
                    if (tris[q].solid && colourAt(tris[q], px, py, rr, gg2, bb, aa) && aa > 0.01f)
                        printf("      fill tri %zu colour %.0f,%.0f,%.0f a %.2f area %.0f\n", q, rr, gg2, bb, aa, areaOf(tris[q]));
                }
            if (getenv("ROUND9_DEBUG"))
                for (size_t q = 960; q < 1030 && q < tris.size(); ++q)
                    printf("      tri %zu %s colour %d,%d,%d,%d at %.1f,%.1f\n", q, tris[q].solid ? "fill" : "glyph", tris[q].c[0].r, tris[q].c[0].g, tris[q].c[0].b, tris[q].c[0].a,
                            (tris[q].x[0] + tris[q].x[1] + tris[q].x[2]) / 3.0f, (tris[q].y[0] + tris[q].y[1] + tris[q].y[2]) / 3.0f);
            if (getenv("ROUND9_DEBUG")) printf("   first failing glyph triangle %zu of %zu: colour %d,%d,%d,%d at %.1f,%.1f, bg %.0f,%.0f,%.0f ratio %.2f\n", i, tris.size(), g.c[0].r, g.c[0].g, g.c[0].b, g.c[0].a, px, py, bg.r, bg.g, bg.b, ratio);
        }
        if (ratio < need) ++out.failed;
        if (!skip && ratio < out.worst)
        {
            out.worst = ratio;
            out.worstText = g.c[0];
            out.worstBg = Color(static_cast<unsigned char>(bg.r), static_cast<unsigned char>(bg.g), static_cast<unsigned char>(bg.b), 255);
        }
    }
    return out;
}

struct Region
{
    const char* name;
    Rect rect;
    bool asserted; // the region is looked at for legibility (the widgets that were painted with fixed colours, and the lists with a selection)
    bool chrome;   // ... and for fills that are dark (what is behind the widget, not the colours of what it shows)
};

// where Round9Window puts what
const Region kRegions[] = {
    { "tool bar", { 0, 0, 1280, 34 }, true, true },
    { "dock", { 0, 36, 640, 330 }, true, true },
    { "tabs", { 646, 36, 300, 100 }, true, true },
    { "splitter", { 952, 36, 320, 100 }, true, true },
    { "search bar", { 852, 208, 200, 26 }, false, true },
    { "text input, combo box", { 646, 208, 410, 26 }, false, true },
    { "controls", { 646, 144, 630, 56 }, false, true }, // (a switch follows the theme since round 10)
    { "text edit", { 646, 242, 200, 70 }, false, true },
    { "list box (a selected row)", { 852, 242, 140, 70 }, true, true },
    { "collapsible", { 998, 242, 270, 70 }, false, false }, // (its arrow is a fill of the text colour)
    { "breadcrumbs", { 646, 320, 300, 26 }, true, true },
    { "status bar", { 952, 320, 320, 26 }, true, true },
    { "data grid (a selected row)", { 0, 372, 250, 110 }, true, true },
    { "tree grid", { 256, 372, 200, 110 }, true, true },
    { "thumbnail grid", { 462, 372, 220, 110 }, true, false }, // (the items have colours of their own)
    { "asset browser", { 688, 372, 300, 110 }, true, false },
    { "code editor", { 0, 488, 420, 100 }, true, true },
    { "timeline", { 426, 488, 440, 100 }, true, true },
    { "node editor", { 872, 488, 400, 100 }, true, false },
};

struct Counter
{
    int n = 0;
};

// ── The tests ───────────────────────────────────────────────────────────────────────────────────────────────────────

void testGolden()
{
    printf("-- the dark preset draws what it drew before the colours moved into the theme\n");
    Theme::instance() = Theme::dark();
    Harness h(1280, 720);
    EveryWidget w = buildEveryWidget(h, false);
    h.frames(3);
    uint32_t vertices = 0;
    const uint64_t hash = drawChecksum(&vertices);
    printf("  dark: %016llx (%u vertices)\n", static_cast<unsigned long long>(hash), vertices);
    check(hash == kDarkGolden && vertices == kDarkGoldenVertices, "the draw list of the dark window is the one of origin/main");
}

void testSwitching()
{
    printf("-- the presets, switched at run time\n");
    Theme::instance() = Theme::dark();
    Harness h(1280, 720);
    EveryWidget w = buildEveryWidget(h, true);
    h.app.setFocused(nullptr); // the focus off the code editor: its caret blinks with the time, and a picture that comes back must match
    h.frames(2);

    Counter changed;
    const auto slot = h.app.themeChanged.connect([&changed] { ++changed.n; }); // (the signal outlives this function: disconnected below)

    uint64_t firstHash[kPresetCount] = {};
    for (int round = 0; round < 2; ++round)
    {
        for (int i = 0; i < kPresetCount; ++i)
        {
            const int before = changed.n;
            const int layoutsBefore = layoutCalls();
            check(h.app.setThemePreset(kPresets[i]), "a preset by name is found");
            check(changed.n == before + 1, "setThemePreset emits themeChanged once");
            h.frames(3);
            check(changed.n == before + 1, "...and not again in the frames that follow");
            check(layoutCalls() > layoutsBefore, "...and the layout is done again");
            uint32_t v = 0;
            const uint64_t hash = drawChecksum(&v);
            if (round == 0) firstHash[i] = hash;
            else
                check(hash == firstHash[i], "a preset draws the same when it comes back (nothing keeps the colours of the one before)");
        }
    }
    check(!h.app.setThemePreset("no such preset") && !h.app.setThemePreset(nullptr), "an unknown preset name is refused");
    check(firstHash[0] != firstHash[1] && firstHash[1] != firstHash[3] && firstHash[0] != firstHash[4], "the presets draw different windows");

    // a widget that kept a colour of the theme it was made in: the menu icon follows the text colour of each preset
    for (int i = 0; i < kPresetCount; ++i)
    {
        h.app.setThemePreset(kPresets[i]);
        h.frames(2);
        const Color text = Theme::instance().textColor;
        const Rect r = w.menuIcon->absoluteRect();
        const DrawList& dl = GetDrawList();
        bool seen = false;
        for (size_t k = 0; k < dl.vertices().size(); ++k)
        {
            const DrawVertex& v = dl.vertices()[k];
            if (v.x >= r.x && v.x <= r.x + r.w && v.y >= r.y && v.y <= r.y + r.h && v.color.r == text.r && v.color.g == text.g && v.color.b == text.b &&
                    v.color.a > 0)
                seen = true;
        }
        char what[100];
        snprintf(what, sizeof what, "%s: the menu icon is drawn in the text colour of the preset", kPresets[i]);
        check(seen, what);
    }

    // what an application does today: assign the theme, or write a field; update() finds it, once
    int before = changed.n;
    int layouts = layoutCalls();
    Theme::instance() = Theme::light();
    h.frames(3);
    check(changed.n == before + 1, "an assignment to Theme::instance() is found by the next frame, once");
    check(layoutCalls() > layouts, "...and lays the window out again");
    before = changed.n;
    layouts = layoutCalls();
    Theme::instance().fontSize = 18.0f;
    h.frames(3);
    check(changed.n == before + 1, "a field written by hand is found by the next frame, once");
    check(layoutCalls() > layouts, "...and lays the window out again");
    before = changed.n;
    h.frames(30);
    check(changed.n == before, "frames that change nothing emit nothing");
    // an application that animates a theme field turns the looking off: nothing is found, and notifyThemeChanged still tells
    before = changed.n;
    h.app.watchTheme(false);
    Theme::instance().padding = 7.0f;
    h.frames(3);
    check(changed.n == before, "with watchTheme(false) a field written by hand is not looked for");
    h.app.notifyThemeChanged();
    check(changed.n == before + 1, "...and notifyThemeChanged tells all the same");
    h.app.watchTheme(true);
    h.frames(2);
    check(changed.n == before + 1, "...and turning the looking on again finds nothing more");
    // explicit: setTheme, then the frame that follows does not emit it again
    before = changed.n;
    h.app.setTheme(Theme::macOS());
    h.frames(2);
    check(changed.n == before + 1, "setTheme emits once, and the frame after it does not find the same change again");
    before = changed.n;
    Theme::instance().padding = 9.0f;
    h.app.notifyThemeChanged();
    h.frames(2);
    check(changed.n == before + 1, "notifyThemeChanged after a field written emits once, and the frame after it does not again");

    // colours an application set on a widget stay when the theme changes
    w.console->setBgColor(Color(10, 60, 10, 255));
    w.console->setColors(ConsoleColors::light());
    h.app.setThemePreset("dark");
    h.frames(3);
    const DrawList& dl = GetDrawList();
    bool bgSeen = false, ownSeen = false;
    for (size_t i = 0; i < dl.vertices().size(); ++i)
    {
        const Color& c = dl.vertices()[i].color;
        if (c.r == 10 && c.g == 60 && c.b == 10 && c.a == 255) bgSeen = true;
        if (c.r == 238 && c.g == 238 && c.b == 242 && c.a == 255) ownSeen = true; // ConsoleColors::light().toolbarBg
    }
    check(bgSeen, "a console background set by the application is still there after a theme change");
    check(ownSeen, "colours given to one console with setColors are still there after a theme change");
    h.app.themeChanged.disconnect(slot);
    Theme::instance() = Theme::dark();
    h.frames(2);
}

// The text colours that are dimmer than 4.5 on purpose or because they are the project's. In every preset: the theme's own dim
// texts (textDisabled and the line numbers: tests/test_themes.cpp holds them to a floor of their own), the colour of a track
// (the project's). In the dark presets also the colour of a comment in code (dimmed by convention). In the dark preset only the
// colours it always drew and that are below 4.5: changing one moves what the dark window draws, which must stay as it was.
void exemptions(int preset, ct::Vector<Color>& out)
{
    const Theme& t = Theme::instance();
    out.push_back(t.textDisabled);
    out.push_back(t.lineNumberColor);
    out.push_back(t.menuItemDisabled);       // a disabled entry of a menu (inactive: exempt from the ratio, as in the web guidelines)
    out.push_back(Color(90, 150, 220, 255));  // the colour of a video track
    out.push_back(Color(220, 145, 75, 255));  // ... of an audio track
    if (preset == 0 || preset == 4) out.push_back(Color(106, 115, 125, 255)); // a comment
    if (preset == 0)
    {
        out.push_back(t.console.timestamp);
        out.push_back(t.console.buttonOffText);
        out.push_back(t.console.searchHint);
        out.push_back(t.console.error);        // on the red of its own row (3.8)
        out.push_back(t.console.trace);        // on the tint of its own filter button when on (3.9)
        out.push_back(t.console.badgeText);    // the "x2" of a repeated message (4.2)
        out.push_back(t.editor.tlRulerText);
        out.push_back(t.editor.tlRulerTextMinor);
    }
}

void testPresetLooks()
{
    printf("-- no dark fill left, text that can be read\n");
    ct::Vector<Tri> tris;
    for (int i = 0; i < kPresetCount; ++i)
    {
        Theme::instance() = Theme::byName(kPresets[i]);
        const bool dark = i == 0 || i == 4;
        Harness h(1280, 720);
        EveryWidget w = buildEveryWidget(h, false);
        h.frames(2);

        ct::Vector<Color> exempt;
        exemptions(i, exempt);
        // fills that are the colour of what a widget shows, not of the widget: the default colour of a thumbnail, the header
        // of a node, the face of an asset's thumbnail, the dark track of a switch
        ct::Vector<Color> content;
        content.push_back(Color(80, 90, 110, 255));
        content.push_back(Color(70, 90, 130, 255));
        content.push_back(Color(36, 38, 42, 255));
        content.push_back(Color(70, 70, 75, 255));

        for (int tab = 0; tab < 2; ++tab)
        {
            w.dock->showPanel(tab == 0 ? "Console" : "Tree");
            h.frames(2);
            trianglesOf(h, tris);
            for (size_t r = 0; r < sizeof kRegions / sizeof kRegions[0]; ++r)
            {
                const Region& region = kRegions[r];
                if (tab == 1 && r != 1) continue; // the other tab of the dock only
                char what[220];
                if (!dark && region.chrome)
                {
                    Color first;
                    const int n = darkFills(tris, region.rect, 90.0f, content, &first);
                    snprintf(what, sizeof what, "%s: no dark fill in the %s (%d found, first %d,%d,%d,%d)", kPresets[i], region.name, n, first.r,
                            first.g, first.b, first.a);
                    check(n == 0, what);
                }
                // (the selected row of a list was textColor over the selection, 3.8 in the dark preset; it is selectionText now, like the trees)
                const float floor = 4.5f;
                const Legibility l = legibility(tris, region.rect, floor, exempt);
                snprintf(what, sizeof what, "%s: text in the %s%s is legible (%d of %d glyph triangles under %.1f, worst %.2f: %d,%d,%d on %d,%d,%d)",
                        kPresets[i], region.name, tab == 1 ? " (tree tab)" : "", l.failed, l.glyphs, floor, l.worst, l.worstText.r, l.worstText.g,
                        l.worstText.b, l.worstBg.r, l.worstBg.g, l.worstBg.b);
                if (region.asserted) check(l.failed == 0, what);
                else if (l.failed > 0)
                    printf("  (not asserted) %s\n", what);
            }
        }
    }
    Theme::instance() = Theme::dark();
}

// the check above sees the text of a selected row: with the selected-row text forced back to white under a light preset (the
// defect this round fixed) it must find it unreadable in the tree, the list and the data grid
void testSelectedTextIsSeen()
{
    printf("-- a selected row with white text under a light preset is found\n");
    Theme::instance() = Theme::light();
    Theme::instance().selectionText = Color(255, 255, 255, 255);
    Harness h(1280, 720);
    EveryWidget w = buildEveryWidget(h, false);
    ct::Vector<Tri> tris;
    ct::Vector<Color> none;
    w.dock->showPanel("Tree");
    h.frames(2);
    trianglesOf(h, tris);
    check(legibility(tris, kRegions[1].rect, 4.5f, none).failed > 0, "white text on the selected node of the tree is seen as unreadable");
    check(legibility(tris, kRegions[12].rect, 4.5f, none).failed > 0, "white text on the selected row of the data grid is seen as unreadable");
    Theme::instance() = Theme::dark();
}

// item 4: the scroll bars of the console and the tree are drawn in the colours of ScrollBar; item 5: sprite-sheet icons are tinted
void testScrollBarsAndIcons()
{
    printf("-- scroll bars and sprite-sheet icons\n");
    for (int i = 0; i < kPresetCount; ++i)
    {
        Theme::instance() = Theme::byName(kPresets[i]);
        Harness h(1280, 720);
        EveryWidget w = buildEveryWidget(h, true);
        h.frames(2);
        const Color track = ScrollBar::trackColor();
        const Color thumb = ScrollBar::thumbColor();
        const Theme& t = Theme::instance();
        check(track.r == t.panelColor.r && track.g == t.panelColor.g && track.b == t.panelColor.b && track.a == 80 &&
                        thumb.r == t.scrollbarThumb.r && thumb.a == t.scrollbarThumb.a,
                "the scroll bar colours are the theme's");
        // the console tab, then the tree tab: each has a bar, in the track colour and in the thumb colour
        for (int tab = 0; tab < 2; ++tab)
        {
            w.dock->showPanel(tab == 0 ? "Console" : "Tree");
            h.frames(2);
            const DrawList& dl = GetDrawList();
            bool trackSeen = false, thumbSeen = false;
            for (size_t k = 0; k < dl.vertices().size(); ++k)
            {
                const DrawVertex& v = dl.vertices()[k];
                if (v.x > 640 || v.y < 36 || v.y > 366) continue;
                if (v.color.r == track.r && v.color.g == track.g && v.color.b == track.b && v.color.a == track.a) trackSeen = true;
                if (v.color.r == thumb.r && v.color.g == thumb.g && v.color.b == thumb.b && v.color.a == thumb.a) thumbSeen = true;
            }
            char what[120];
            snprintf(what, sizeof what, "%s: the %s has the scroll bar track in the theme's colour", kPresets[i], tab == 0 ? "console" : "tree");
            check(trackSeen, what);
            snprintf(what, sizeof what, "%s: the %s has the scroll bar thumb in the theme's colour", kPresets[i], tab == 0 ? "console" : "tree");
            check(thumbSeen, what);
        }

        // a tool bar with a sprite sheet: the icons are tinted with the theme's icon colour (enabled) and the dimmer one (disabled)
        Widget* holder = h.root()->createChild<Widget>();
        holder->setRect({ 0, 600, 300, 34 });
        ToolBar* tb = holder->createChild<ToolBar>();
        tb->setTexture(handle(7), 64, 64, 16, 16);
        const int on = tb->addButton(0, "one");
        const int off = tb->addButton(1, "two");
        tb->setItemEnabled(off, false);
        tb->setRect({ 0, 0, 300, 34 });
        (void) on;
        h.relayout();
        h.frames(2);
        const DrawList& dl = GetDrawList();
        bool tintSeen = false, dimSeen = false;
        for (size_t c = 0; c < dl.commands().size(); ++c)
        {
            const DrawCmd& cmd = dl.commands()[c];
            if (cmd.texture.value != 7) continue;
            for (uint32_t k = cmd.indexOffset; k < cmd.indexOffset + cmd.indexCount; ++k)
            {
                const Color& col = dl.vertices()[dl.indices()[k]].color;
                if (col.r == t.toolbarIcon.r && col.g == t.toolbarIcon.g && col.b == t.toolbarIcon.b) tintSeen = true;
                if (col.r == t.toolbarIconDisabled.r && col.g == t.toolbarIconDisabled.g && col.b == t.toolbarIconDisabled.b) dimSeen = true;
            }
        }
        char what[120];
        snprintf(what, sizeof what, "%s: a sprite-sheet icon is tinted with toolbarIcon", kPresets[i]);
        check(tintSeen, what);
        snprintf(what, sizeof what, "%s: a disabled sprite-sheet icon is tinted with toolbarIconDisabled", kPresets[i]);
        check(dimSeen, what);
    }
    Theme::instance() = Theme::dark();
}

void writePicture(const char* dir, const char* name, const Shot& s)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    if (FILE* f = fopen(path, "wb"))
    {
        fprintf(f, "P6\n%d %d\n255\n", s.w, s.h);
        for (size_t i = 0; i < s.px.size(); ++i)
        {
            const unsigned char rgb[3] = { static_cast<unsigned char>(s.px[i] >> 16), static_cast<unsigned char>(s.px[i] >> 8),
                static_cast<unsigned char>(s.px[i]) };
            fwrite(rgb, 1, 3, f);
        }
        fclose(f);
        printf("  wrote %s\n", path);
    }
}

void testPictures(const char* dir)
{
    printf("-- pictures of the window in each preset, into %s\n", dir);
    for (int crowded = 0; crowded < 2; ++crowded)
    {
        for (int i = 0; i < kPresetCount; ++i)
        {
            Theme::instance() = Theme::byName(kPresets[i]);
            Screen s(1280, 720);
            s.useDynamicPage();
            EveryWidget w = buildEveryWidget(s.h, crowded != 0);
            s.h.frames(2);
            char name[64];
            snprintf(name, sizeof name, "window-%s%s", kPresets[i], crowded ? "-scrolling" : "");
            writePicture(dir, name, s.take());
        }
    }
    Theme::instance() = Theme::dark();
}


// ── Round 10 ─────────────────────────────────────────────────────────────────────────────────────────────────────────────

uint64_t pixelHash(const Shot& s)
{
    uint64_t hash = 1469598103934665603ull;
    for (size_t i = 0; i < s.px.size(); ++i)
        for (int k = 0; k < 4; ++k) hash = (hash ^ ((s.px[i] >> (8 * k)) & 0xffu)) * 1099511628211ull;
    return hash;
}

// a dock of two tabs with a third panel split off to the right and the first split below: what a docked splitter is
Shot dockSplitShot(const char* preset)
{
    Theme::instance() = Theme::byName(preset);
    Screen screen(400.0f, 220.0f);
    Harness& h = screen.h;
    Widget* holder = h.root()->createChild<Widget>();
    holder->setRect({ 0, 0, 400, 220 });
    DockPanel* dock = holder->createChild<DockPanel>();
    dock->setRect({ 0, 10, 400, 200 });
    dock->addPanel("Alpha", new Label("a"));
    dock->addPanel("Beta", new Label("b"));
    dock->splitOff("Beta", DockSide::Right, 0.4f);
    h.relayout();
    h.frames(2);
    return screen.take();
}

void testGoldens()
{
    printf("-- the window of every preset, and a docked splitter, against goldens\n");
    for (int i = 0; i < kPresetCount; ++i)
    {
        Theme::instance() = Theme::byName(kPresets[i]);
        Harness h(1280, 720);
        EveryWidget w = buildEveryWidget(h, false);
        h.frames(3);
        uint32_t vertices = 0;
        const uint64_t hash = drawChecksum(&vertices);
        printf("  %s: %016llx (%u vertices)\n", kPresets[i], static_cast<unsigned long long>(hash), vertices);
        char what[140];
        snprintf(what, sizeof what, "%s: the draw list of the window is the golden one (kPresetGolden[%d])", kPresets[i], i);
        if (i == 0) check(hash == kDarkGolden, what);
        else if (kPresetGolden[i] != 0)
            check(hash == kPresetGolden[i], what);
    }
    const Shot dark = dockSplitShot("dark");
    const Shot mac = dockSplitShot("macos-dark");
    printf("  dock split pixels: dark %016llx, macos-dark %016llx\n", static_cast<unsigned long long>(pixelHash(dark)),
            static_cast<unsigned long long>(pixelHash(mac)));
    if (kDockSplitGoldenDark) check(pixelHash(dark) == kDockSplitGoldenDark, "a split dock in the dark preset draws the pixels it drew before round 9");
    if (kDockSplitGoldenMacDark) check(pixelHash(mac) == kDockSplitGoldenMacDark, "a split dock in macos-dark draws the golden pixels");
    // the splitter of the dark dock is the dock's own handle colour and the line round the leaves; nothing in the dock is a colour
    // that is not one of its fields (the app that saw a line of 54,54,59 and 41,41,46 was looking at a blend of the border at a
    // fractional position: none of those colours is drawn here)
    const Theme d = Theme::dark();
    bool handleSeen = false;
    for (size_t i = 0; i < dark.px.size(); ++i)
        if (dark.px[i] == (0xff000000u | (static_cast<uint32_t>(d.dock.handle.r) << 16) | (static_cast<uint32_t>(d.dock.handle.g) << 8) | d.dock.handle.b))
            handleSeen = true;
    check(handleSeen, "the dark dock draws its splitter handle in dock.handle (50,50,53)");
    Theme::instance() = Theme::dark();
}

void collectRects(const Widget* w, ct::Vector<Rect>& out)
{
    out.push_back(w->absoluteRect());
    for (size_t i = 0; i < w->children().size(); ++i) collectRects(w->children()[i], out);
}

bool sameRects(const ct::Vector<Rect>& a, const ct::Vector<Rect>& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].x != b[i].x || a[i].y != b[i].y || a[i].w != b[i].w || a[i].h != b[i].h) return false;
    return true;
}

void testThemeParts()
{
    printf("-- a preset's colours without its metrics\n");
    check(!Theme::macOS().sameMetricsAs(Theme::dark()), "(the macOS preset has metrics of its own, or this test says nothing)");
    Theme::instance() = Theme::dark();
    Harness h(1280, 720);
    EveryWidget w = buildEveryWidget(h, false);
    h.app.setFocused(nullptr);
    h.frames(3);
    ct::Vector<Rect> before;
    collectRects(h.root(), before);
    Counter changed;
    const auto slot = h.app.themeChanged.connect([&changed] { ++changed.n; });
    const Theme dark = Theme::dark();
    const Theme mac = Theme::macOS();

    int layouts = layoutCalls();
    check(h.app.setThemePreset("macos", ThemeParts::Colors), "a preset by name, colours only");
    h.frames(3);
    check(changed.n == 1, "themeChanged is emitted once");
    check(layoutCalls() == layouts, "no layout is done (nothing a layout reads changed)");
    ct::Vector<Rect> after;
    collectRects(h.root(), after);
    check(sameRects(before, after), "the rectangle of every widget is the same before and after");
    const Theme& now = Theme::instance();
    check(now.sameMetricsAs(dark), "the metrics are the dark theme's");
    check(now.bgColor.r == mac.bgColor.r && now.focusColor.r == mac.focusColor.r && now.textColor.r == mac.textColor.r &&
                    now.dock.accent.r == mac.dock.accent.r && now.switchOn.r == mac.switchOn.r,
            "the colours (the groups included) are macOS's");
    check(now.controlGradient == mac.controlGradient && now.progressBlocks == mac.progressBlocks, "and so are its looks (gradients, blocks)");
    uint32_t v = 0;
    const uint64_t coloursOnly = drawChecksum(&v);

    // whole: the metrics change, and with them some rectangle
    Theme::instance() = dark;
    h.app.notifyThemeChanged();
    h.frames(3);
    changed.n = 0;
    layouts = layoutCalls();
    h.app.setThemePreset("macos");
    h.frames(3);
    ct::Vector<Rect> whole;
    collectRects(h.root(), whole);
    check(changed.n == 1 && layoutCalls() > layouts, "the whole preset asks for the layout, as always");
    check(!sameRects(before, whole), "... and it moves a rectangle (the metrics of the preset)");
    check(drawChecksum(&v) != coloursOnly, "... so the window draws differently from the colours-only one");

    // metrics only: the colours stay
    Theme::instance() = dark;
    h.app.notifyThemeChanged();
    h.frames(3);
    changed.n = 0;
    h.app.setThemePreset("macos", ThemeParts::Metrics);
    h.frames(3);
    check(changed.n == 1 && Theme::instance().bgColor.r == dark.bgColor.r && Theme::instance().sameMetricsAs(mac), "ThemeParts::Metrics takes the sizes and keeps the colours");
    // colours only, again and from a hand-written theme: the same, with the unknown name refused
    check(!h.app.setThemePreset("no such", ThemeParts::Colors), "an unknown name is refused in every mode");
    Theme custom = Theme::light();
    custom.fontSize = 22.0f;
    Theme::instance() = dark;
    h.app.notifyThemeChanged();
    h.frames(2);
    changed.n = 0;
    h.app.setTheme(custom, ThemeParts::Colors);
    check(changed.n == 1 && Theme::instance().fontSize == dark.fontSize && Theme::instance().bgColor.r == custom.bgColor.r, "setTheme(theme, Colors): the colours of the theme, the sizes of the window");
    h.app.themeChanged.disconnect(slot);
    Theme::instance() = Theme::dark();
    h.frames(2);
}

float contrast(const Color& a, const Color& b)
{
    return contrastOf(luminance(a.r, a.g, a.b), luminance(b.r, b.g, b.b));
}

void testSwitchColours()
{
    printf("-- the Switch follows the theme\n");
    for (int i = 0; i < kPresetCount; ++i)
    {
        Theme::instance() = Theme::byName(kPresets[i]);
        const Theme& t = Theme::instance();
        char what[160];
        const float on = contrast(t.switchThumb, t.switchOn), off = contrast(t.switchThumb, t.switchOff);
        snprintf(what, sizeof what, "%s: the thumb against the track, on %.2f and off %.2f, is 3 or more (the dark on track keeps its 2.4)", kPresets[i], on, off);
        check(off >= 3.0f && (on >= 3.0f || i == 0), what);
        const float dr = static_cast<float>(t.switchOn.r) - t.switchOff.r, dg = static_cast<float>(t.switchOn.g) - t.switchOff.g,
                    db = static_cast<float>(t.switchOn.b) - t.switchOff.b;
        snprintf(what, sizeof what, "%s: the on and off tracks are told apart (colour distance %.0f)", kPresets[i], sqrtf(dr * dr + dg * dg + db * db));
        check(sqrtf(dr * dr + dg * dg + db * db) >= 80.0f, what);

        Harness h(300, 120);
        Widget* holder = h.root()->createChild<Widget>();
        holder->setRect({ 0, 0, 300, 120 });
        Switch* a = holder->createChild<Switch>("on");
        a->setOn(true);
        a->setRect({ 10, 10, 120, 24 });
        Switch* b = holder->createChild<Switch>("off");
        b->setRect({ 10, 50, 120, 24 });
        Switch* c = holder->createChild<Switch>("own");
        c->setOnColor(Color(200, 10, 10, 255));
        c->setOn(true);
        c->setRect({ 10, 90, 120, 24 });
        h.relayout();
        h.frames(2);
        const DrawList& dl = GetDrawList();
        auto seen = [&](const Color& col, float y0, float y1)
        {
            for (size_t k = 0; k < dl.vertices().size(); ++k)
            {
                const DrawVertex& v = dl.vertices()[k];
                if (v.y >= y0 && v.y <= y1 && v.x < 100 && v.color.r == col.r && v.color.g == col.g && v.color.b == col.b && v.color.a == 255) return true;
            }
            return false;
        };
        // (the track of a hovered switch is a little lighter: the pointer is away here)
        snprintf(what, sizeof what, "%s: an on switch has the theme's switchOn track", kPresets[i]);
        check(seen(t.switchOn, 10, 34), what);
        snprintf(what, sizeof what, "%s: an off switch has the theme's switchOff track", kPresets[i]);
        check(seen(t.switchOff, 50, 74), what);
        check(seen(Color(200, 10, 10, 255), 90, 114), "a colour set with setOnColor wins over the theme");
        Theme::instance() = Theme::dark();
    }
}

// the popups of the library, one at a time
struct PopupScene
{
    Harness h;
    Button* button = nullptr;
    ComboBox* combo = nullptr;
    DatePicker* date = nullptr;
    ColorButton* colour = nullptr;
    Label* context = nullptr;
    CodeEditor* code = nullptr;
    Menu* menu = nullptr;
    Menu* contextMenu = nullptr;
    PopupScene() : h(1280, 720)
    {
        Widget* holder = h.root()->createChild<Widget>();
        holder->setRect({ 0, 0, 1280, 720 });
        button = holder->createChild<Button>("A button with a tip");
        button->setRect({ 20, 20, 200, 28 });
        button->setTooltip("A tooltip, long enough to read");
        combo = holder->createChild<ComboBox>();
        for (int i = 0; i < 5; ++i) combo->addItem(String("combo item ") + String::number(i));
        combo->setSelectedIndex(1);
        combo->setRect({ 240, 20, 160, 26 });
        date = holder->createChild<DatePicker>();
        date->setRect({ 420, 20, 180, 26 });
        colour = holder->createChild<ColorButton>(Color(200, 80, 40, 255));
        colour->setRect({ 620, 20, 60, 26 });
        context = holder->createChild<Label>("A label with a context menu");
        context->setRect({ 700, 20, 260, 26 });
        contextMenu = new Menu();
        contextMenu->addAction("Cut");
        contextMenu->addAction("Copy")->setShortcut("Ctrl+C");
        contextMenu->addSeparator();
        contextMenu->addCheckable("Checked", true);
        context->setContextMenu(contextMenu);
        code = holder->createChild<CodeEditor>();
        code->setHighlighter<CppHighlighter>();
        code->setCompletionProvider(
                [](const CodeEditor::CompletionContext&, ct::Vector<CodeEditor::CompletionItem>& out)
                {
                    const char* names[] = { "printf", "println", "process" };
                    for (int i = 0; i < 3; ++i)
                    {
                        CodeEditor::CompletionItem item;
                        item.text = names[i];
                        item.detail = "int (const char*)";
                        item.doc = "Writes text to the output";
                        out.push_back(item);
                    }
                });
        code->setRect({ 20, 300, 500, 140 });
        menu = new Menu();
        menu->addAction("New");
        menu->addAction("Open")->setShortcut("Ctrl+O");
        menu->addSeparator();
        menu->addCheckable("Auto save", true);
        MenuAction* off = menu->addAction("Disabled entry");
        off->setEnabled(false);
        Menu* sub = menu->addSubmenu("Recent");
        sub->addAction("one.txt");
        h.relayout();
        h.frames(3);
    }
    ~PopupScene()
    {
        WidgetApp::instance().closePopup();
        h.frames(2);
        delete menu;
    }
    void closeAll()
    {
        WidgetApp::instance().closePopup();
        h.io().mouseX = 900.0f;
        h.io().mouseY = 600.0f;
        h.frames(14);
    }
};

struct PopupCase
{
    const char* name;
    Rect region; // where it was drawn
    bool opened;
};

// opens popup number n (0..6); the region of the popup, drawn after 12 frames
PopupCase openPopup(PopupScene& s, int n)
{
    PopupCase c = { "", { 0, 0, 0, 0 }, false };
    WidgetApp& app = WidgetApp::instance();
    switch (n)
    {
        case 0:
            c.name = "menu";
            s.menu->exec(30.0f, 70.0f);
            break;
        case 1:
            c.name = "combo box list";
            s.combo->openDropdown();
            break;
        case 2:
            c.name = "tooltip";
            s.h.moveTo(120.0f, 34.0f);
            s.h.frames(70);
            c.region = { 100, 30, 360, 60 };
            c.opened = true;
            return c;
        case 3:
            c.name = "colour popup";
            s.colour->openPicker();
            break;
        case 4:
            c.name = "date popup";
            s.date->openCalendar();
            break;
        case 5:
            c.name = "context menu";
            s.h.click(800.0f, 33.0f, 1);
            break;
        default:
            c.name = "completion list";
            app.setFocused(s.code);
            s.code->setText("printf print println\nprocess\n");
            s.h.clickWidget(s.code);
            s.h.key(ig::retained::Key::End, false, true); // Ctrl+End: the cursor at the end of the text
            s.h.type("pr");
            s.h.moveTo(900.0f, 650.0f); // (the pointer away: a row of the list under it is a row selected by hovering)
            s.code->requestCompletion();
            s.h.frames(12);
            c.region = { 20, 300, 500, 140 };
            c.opened = s.code->completionVisible();
            return c;
    }
    s.h.frames(14);
    if (Widget* popup = app.popup())
    {
        c.region = popup->absoluteRect();
        c.opened = true;
    }
    return c;
}

void testPopups()
{
    printf("-- the popups, in every preset\n");
    ct::Vector<Tri> tris;
    for (int i = 0; i < kPresetCount; ++i)
    {
        Theme::instance() = Theme::byName(kPresets[i]);
        const bool dark = i == 0 || i == 4;
        ct::Vector<Color> exempt;
        exemptions(i, exempt);
        ct::Vector<Color> content;
        content.push_back(Color(200, 80, 40, 255)); // the colour of the colour button
        content.push_back(Theme::shade(Theme::instance().focusColor, 0, 0.3f)); // the selected day of the calendar in the light presets (opaque, darkened)
        PopupScene s;
        for (int n = 0; n < 7; ++n)
        {
            const PopupCase c = openPopup(s, n);
            char what[220];
            snprintf(what, sizeof what, "%s: the %s opens", kPresets[i], c.name);
            check(c.opened, what);
            if (!c.opened || c.region.w <= 0.0f)
            {
                s.closeAll();
                continue;
            }
            trianglesOf(s.h, tris);
            // a popup is a region of its own over the window: its fills are the theme's; (the colour picker's gradients and swatches, the
            // calendar's today mark and the highlighted line of the code are content)
            if (!dark && n != 3 && n != 6)
            {
                Color first;
                const int dk = darkFills(tris, c.region, 90.0f, content, &first);
                snprintf(what, sizeof what, "%s: no dark fill in the %s (%d found, first %d,%d,%d,%d)", kPresets[i], c.name, dk, first.r, first.g, first.b, first.a);
                check(dk == 0, what);
            }
            // the date popup in the dark preset keeps its white day on the grey focus colour (as it always was)
            const float floor = (i == 0 && n == 4) ? 1.5f : 4.5f;
            const Legibility l = legibility(tris, c.region, floor, exempt);
            snprintf(what, sizeof what, "%s: text in the %s is legible (%d of %d glyph triangles under %.1f, worst %.2f: %d,%d,%d on %d,%d,%d; first at %.0f,%.0f)", kPresets[i], c.name,
                    l.failed, l.glyphs, floor, l.worst, l.worstText.r, l.worstText.g, l.worstText.b, l.worstBg.r, l.worstBg.g, l.worstBg.b, l.failX, l.failY);
            check(l.failed == 0 && l.glyphs > 0, what);
            s.closeAll();
        }
    }
    Theme::instance() = Theme::dark();
}

// a sheet of the popups of a preset, side by side (ROUND10_SHOTS)
void testPopupSheets(const char* dir)
{
    printf("-- a sheet of the popups of each preset, into %s\n", dir);
    for (int i = 0; i < kPresetCount; ++i)
    {
        Theme::instance() = Theme::byName(kPresets[i]);
        Shot sheet;
        sheet.w = 4 * 330;
        sheet.h = 2 * 190;
        sheet.px.resize(static_cast<size_t>(sheet.w) * static_cast<size_t>(sheet.h));
        for (size_t k = 0; k < sheet.px.size(); ++k) sheet.px[k] = 0xff000000u | (static_cast<uint32_t>(Theme::instance().bgColor.r) << 16) |
                                                                  (static_cast<uint32_t>(Theme::instance().bgColor.g) << 8) | Theme::instance().bgColor.b;
        // the shots need the rasteriser's window (Screen), whose harness is the scene's: a scene made on a Screen
        for (int n = 0; n < 7; ++n)
        {
            Screen screen(1280, 720);
            screen.useDynamicPage();
            Widget* holder = screen.h.root()->createChild<Widget>();
            holder->setRect({ 0, 0, 1280, 720 });
            Harness& h = screen.h;
            Button* button = holder->createChild<Button>("A button with a tip");
            button->setRect({ 20, 20, 200, 28 });
            button->setTooltip("A tooltip, long enough to read");
            ComboBox* combo = holder->createChild<ComboBox>();
            for (int k = 0; k < 5; ++k) combo->addItem(String("combo item ") + String::number(k));
            combo->setSelectedIndex(1);
            combo->setRect({ 240, 20, 160, 26 });
            DatePicker* date = holder->createChild<DatePicker>();
            date->setRect({ 420, 20, 180, 26 });
            ColorButton* colour = holder->createChild<ColorButton>(Color(200, 80, 40, 255));
            colour->setRect({ 620, 20, 60, 26 });
            Label* context = holder->createChild<Label>("A label with a context menu");
            context->setRect({ 700, 20, 260, 26 });
            Menu* contextMenu = new Menu();
            contextMenu->addAction("Cut");
            contextMenu->addAction("Copy")->setShortcut("Ctrl+C");
            contextMenu->addSeparator();
            contextMenu->addCheckable("Checked", true);
            context->setContextMenu(contextMenu);
            CodeEditor* code = holder->createChild<CodeEditor>();
            code->setHighlighter<CppHighlighter>();
            code->setCompletionProvider(
                    [](const CodeEditor::CompletionContext&, ct::Vector<CodeEditor::CompletionItem>& out)
                    {
                        const char* names[] = { "printf", "println", "process" };
                        for (int k = 0; k < 3; ++k)
                        {
                            CodeEditor::CompletionItem item;
                            item.text = names[k];
                            item.detail = "int (const char*)";
                            out.push_back(item);
                        }
                    });
            code->setRect({ 20, 300, 500, 140 });
            Menu* menu = new Menu();
            menu->addAction("New");
            menu->addAction("Open")->setShortcut("Ctrl+O");
            menu->addSeparator();
            menu->addCheckable("Auto save", true);
            menu->addAction("Disabled entry")->setEnabled(false);
            menu->addSubmenu("Recent")->addAction("one.txt");
            h.relayout();
            h.frames(3);
            Rect crop = { 0, 0, 0, 0 };
            switch (n)
            {
                case 0: menu->exec(30.0f, 70.0f); break;
                case 1: combo->openDropdown(); break;
                case 2: h.moveTo(120.0f, 34.0f); h.frames(70); crop = { 20, 10, 320, 90 }; break;
                case 3: colour->openPicker(); break;
                case 4: date->openCalendar(); break;
                case 5: h.click(800.0f, 33.0f, 1); break;
                default:
                    WidgetApp::instance().setFocused(code);
                    code->setText("printf print println\nprocess\n");
                    h.clickWidget(code);
                    h.key(ig::retained::Key::End, false, true);
                    h.type("pr");
                    h.moveTo(900.0f, 650.0f);
                    code->requestCompletion();
                    crop = { 20, 300, 320, 150 };
                    break;
            }
            h.frames(14);
            if (crop.w == 0.0f)
                if (Widget* popup = WidgetApp::instance().popup()) crop = popup->absoluteRect();
            if (crop.w == 0.0f) crop = { 0, 0, 320, 180 };
            const Shot shot = screen.take();
            const int cx = static_cast<int>(crop.x) - 4, cy = static_cast<int>(crop.y) - 4;
            const int ox = (n % 4) * 330, oy = (n / 4) * 190;
            for (int y = 0; y < 186 && cy + y < shot.h; ++y)
                for (int x = 0; x < 326 && cx + x < shot.w; ++x)
                    if (cx + x >= 0 && cy + y >= 0)
                        sheet.px[static_cast<size_t>(oy + y) * static_cast<size_t>(sheet.w) + static_cast<size_t>(ox + x)] =
                                shot.px[static_cast<size_t>(cy + y) * static_cast<size_t>(shot.w) + static_cast<size_t>(cx + x)];
            WidgetApp::instance().closePopup();
            h.frames(2);
            delete menu;
        }
        char name[64];
        snprintf(name, sizeof name, "popups-%s", kPresets[i]);
        writePicture(dir, name, sheet);
    }
    Theme::instance() = Theme::dark();
}

// the selected row of a list, before round 10 (the text colour over the selection) and now (selectionText), side by side, in each preset (ROUND10_SHOTS)
void testSelectionPictures(const char* dir)
{
    printf("-- the selected row of a list, before and after, into %s\n", dir);
    for (int i = 0; i < kPresetCount; ++i)
    {
        Shot sheet;
        sheet.w = 2 * 250;
        sheet.h = 120;
        sheet.px.resize(static_cast<size_t>(sheet.w) * static_cast<size_t>(sheet.h));
        for (int pass = 0; pass < 2; ++pass)
        {
            Theme::instance() = Theme::byName(kPresets[i]);
            if (pass == 0) Theme::instance().selectionText = Theme::instance().textColor; // what a list drew before
            Screen screen(1280, 720);
            screen.useDynamicPage();
            Widget* holder = screen.h.root()->createChild<Widget>();
            holder->setRect({ 0, 0, 1280, 720 });
            ListBox* list = holder->createChild<ListBox>();
            const char* names[] = { "Oscillator", "Filter cutoff", "Envelope", "Reverb send", "Master" };
            for (int k = 0; k < 5; ++k) list->addItem(names[k]);
            list->setSelectedIndex(1);
            list->setRect({ 10, 10, 230, 100 });
            screen.h.relayout();
            screen.h.frames(6);
            const Shot shot = screen.take();
            for (int y = 0; y < 120; ++y)
                for (int x = 0; x < 250; ++x)
                    sheet.px[static_cast<size_t>(y) * static_cast<size_t>(sheet.w) + static_cast<size_t>(pass * 250 + x)] =
                            shot.px[static_cast<size_t>(y) * static_cast<size_t>(shot.w) + static_cast<size_t>(x)];
        }
        char name[64];
        snprintf(name, sizeof name, "list-selection-%s", kPresets[i]);
        writePicture(dir, name, sheet);
    }
    Theme::instance() = Theme::dark();
}

} // namespace

int main()
{
    testGolden();
    testGoldens();
    testSwitching();
    testThemeParts();
    testSwitchColours();
    testPresetLooks();
    testPopups();
    testSelectedTextIsSeen();
    testScrollBarsAndIcons();
    if (const char* dir = getenv("ROUND9_SHOTS")) testPictures(dir);
    if (const char* dir = getenv("ROUND10_SHOTS"))
    {
        testPopupSheets(dir);
        testSelectionPictures(dir);
    }
    Theme::instance() = Theme::dark();
    shutdown();
    if (failures() == 0) printf("PASS\n");
    return failures() == 0 ? 0 : 1;
}
