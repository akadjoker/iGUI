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

#include "RasterScreen.hpp"
#include "Round9Window.hpp"

using namespace rh;

namespace
{

const char* const kPresets[] = { "dark", "light", "xp", "macos", "macos-dark" };
constexpr int kPresetCount = 5;

// The checksum of what the window draws in the dark preset, taken with the library of origin/main (0d86927), before any colour
// moved into the theme: the dark preset must go on drawing exactly this. (Updated, with the reason, when a later change to the
// geometry changes what is drawn.)
constexpr uint64_t kDarkGolden = 0x2710448eb3ee6e18ull;
constexpr uint32_t kDarkGoldenVertices = 7722;

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
    { "controls", { 646, 144, 630, 56 }, false, false }, // (a switch has a dark track in every preset)
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
                // the selected row of a list in the dark preset is the colour it always was: textColor over the selection (3.8)
                const float floor = (i == 0 && r == 8) ? 3.5f : 4.5f;
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

} // namespace

int main()
{
    testGolden();
    testSwitching();
    testPresetLooks();
    testSelectedTextIsSeen();
    testScrollBarsAndIcons();
    if (const char* dir = getenv("ROUND9_SHOTS")) testPictures(dir);
    Theme::instance() = Theme::dark();
    shutdown();
    if (failures() == 0) printf("PASS\n");
    return failures() == 0 ? 0 : 1;
}
