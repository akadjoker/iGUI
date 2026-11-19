// Round 9, part 2 (docs/ROUND9.md): the retained DrawList.
//   1. The corners of a rounded rectangle take the number of segments the error asks for (no chord more than 0.15 px from the arc).
//   2. The fast paths of the rounded rectangle (fill and border) and of a line make the vertices and indices that the generic path
//      (pathLineTo / pathFillConvex / pathStroke) makes of the same points, to a thousandth of a pixel.
//   3. Commands that can be one are one: the counts in the cases that matter, and the picture of the software rasteriser with merging
//      on and off is the same, pixel by pixel, in a few hundred random scenes (shapes under random clips, texts, rounded clips, the
//      two pages of the font atlas).
//   4. The dynamic page of the font atlas has the white texel at (0, 0), where the baked page has it, and keeps it.
// No standard library: ct and C only.

#include "RetainedHarness.hpp"

#include <igui_zen/SoftRaster.hpp>

using namespace rh;

namespace
{

// ── 1 and 2 ─────────────────────────────────────────────────────────────────────────────────────────────────────────

const float kPi = 3.14159265358979323846f;

// The generic path of a rounded rectangle: the points of the four corners in the order the library uses, as a path.
void roundPath(DrawList& list, float x0, float y0, float x1, float y1, float r, int steps)
{
    const float cx[4] = { x1 - r, x1 - r, x0 + r, x0 + r };
    const float cy[4] = { y0 + r, y1 - r, y1 - r, y0 + r };
    const float start[4] = { -kPi * 0.5f, 0.0f, kPi * 0.5f, kPi };
    const float step = (kPi * 0.5f) / static_cast<float>(steps);
    for (int c = 0; c < 4; ++c)
        for (int i = 0; i <= steps; ++i)
        {
            const float a = start[c] + step * static_cast<float>(i);
            list.pathLineTo({ cx[c] + cosf(a) * r, cy[c] + sinf(a) * r });
        }
}

bool sameGeometry(const DrawList& a, const DrawList& b, float tolerance, char* why, size_t whySize)
{
    if (a.vertices().size() != b.vertices().size())
    {
        snprintf(why, whySize, "%zu vertices against %zu", a.vertices().size(), b.vertices().size());
        return false;
    }
    if (a.indices().size() != b.indices().size())
    {
        snprintf(why, whySize, "%zu indices against %zu", a.indices().size(), b.indices().size());
        return false;
    }
    for (size_t i = 0; i < a.vertices().size(); ++i)
    {
        const DrawVertex &p = a.vertices()[i], &q = b.vertices()[i];
        if (fabsf(p.x - q.x) > tolerance || fabsf(p.y - q.y) > tolerance || p.color.r != q.color.r || p.color.g != q.color.g ||
                p.color.b != q.color.b || p.color.a != q.color.a)
        {
            snprintf(why, whySize, "vertex %zu: (%.4f, %.4f) %d,%d,%d,%d against (%.4f, %.4f) %d,%d,%d,%d", i, p.x, p.y, p.color.r, p.color.g,
                    p.color.b, p.color.a, q.x, q.y, q.color.r, q.color.g, q.color.b, q.color.a);
            return false;
        }
    }
    for (size_t i = 0; i < a.indices().size(); ++i)
        if (a.indices()[i] != b.indices()[i])
        {
            snprintf(why, whySize, "index %zu: %u against %u", i, a.indices()[i], b.indices()[i]);
            return false;
        }
    return true;
}

void testCornerSegments()
{
    printf("-- the segments of a corner follow the error\n");
    const float radii[] = { 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 12.0f, 20.0f, 40.0f, 100.0f };
    for (size_t i = 0; i < sizeof radii / sizeof radii[0]; ++i)
    {
        const float r = radii[i];
        DrawList list;
        list.addRoundRectFilled({ 0, 0, 400, 300 }, r, Color(255, 255, 255, 255));
        if (r < 0.5f) continue;
        const int points = static_cast<int>(list.vertices().size()) / 8; // 4 corners, 2 vertices a point
        const int steps = points - 1;
        const float halfStep = kPi * 0.25f / static_cast<float>(steps);
        char what[120];
        snprintf(what, sizeof what, "radius %.0f: %d steps, a chord %.3f px from the arc (at most 0.15)", r, steps, r * (1.0f - cosf(halfStep)));
        check(steps >= 3 && r * (1.0f - cosf(halfStep)) <= 0.1501f, what);
        if (steps > 3)
        {
            // one step fewer would be too coarse: the error asks for this many
            const float fewer = kPi * 0.25f / static_cast<float>(steps - 1);
            snprintf(what, sizeof what, "radius %.0f: one step fewer would be coarser than 0.15 px", r);
            check(r * (1.0f - cosf(fewer)) > 0.1499f, what);
        }
    }
    DrawList small;
    small.addRoundRectFilled({ 0, 0, 100, 40 }, 4.0f, Color(255, 255, 255, 255));
    check(small.vertices().size() == 2u * 4u * 4u, "a radius of 4 is three segments a corner (it was five or six)");
    DrawList hinted;
    hinted.addRoundRectFilled({ 0, 0, 100, 40 }, 12.0f, Color(255, 255, 255, 255), 3);
    check(hinted.vertices().size() == 2u * 4u * 4u, "a hint lower than the error asks for is kept (three segments for a radius of 12)");
}

void testFastPathsMatchThePath()
{
    printf("-- the fast paths of the rounded rectangle and the line make what the generic path makes\n");
    struct Case
    {
        float w, h, r, thickness;
    } cases[] = { { 100, 40, 4, 1 }, { 120, 26, 4, 1 }, { 60, 60, 6, 2 }, { 300, 200, 12, 1 }, { 31.5f, 17.25f, 3, 1 }, { 400, 300, 40, 3 }, { 20, 20, 4, 1 } };
    for (size_t k = 0; k < sizeof cases / sizeof cases[0]; ++k)
    {
        const Case& c = cases[k];
        for (int transformed = 0; transformed < 2; ++transformed)
        {
            const float m[6] = { 1.0f, 0.0f, 0.0f, 1.0f, 7.0f, -3.0f };
            const Rect rect = { 10.25f, 20.5f, c.w, c.h };
            const Color color(200, 120, 40, 255);
            char what[200], why[200];

            // fill
            DrawList fast, generic;
            if (transformed)
            {
                fast.pushTransform(m);
                generic.pushTransform(m);
            }
            fast.addRoundRectFilled(rect, c.r, color);
            const int steps = static_cast<int>(fast.vertices().size()) / 8 - 1;
            roundPath(generic, rect.x, rect.y, rect.x + rect.w, rect.y + rect.h, c.r, steps);
            generic.pathFillConvex(color);
            snprintf(what, sizeof what, "fill %.0fx%.0f radius %.0f%s equals the path (%s)", c.w, c.h, c.r, transformed ? " transformed" : "",
                    sameGeometry(fast, generic, 0.001f, why, sizeof why) ? "same" : why);
            check(sameGeometry(fast, generic, 0.001f, why, sizeof why), what);

            // border (inset by half the thickness, as addRoundRect does)
            DrawList fastB, genericB;
            if (transformed)
            {
                fastB.pushTransform(m);
                genericB.pushTransform(m);
            }
            fastB.addRoundRect(rect, c.r, color, c.thickness);
            const float inset = c.thickness * 0.5f;
            const Rect r2 = { rect.x + inset, rect.y + inset, rect.w - inset * 2.0f, rect.h - inset * 2.0f };
            const float radius2 = maxf(0.0f, minf(c.r, minf(r2.w, r2.h) * 0.5f));
            const int stepsB = static_cast<int>(fastB.vertices().size()) / 16 - 1;
            roundPath(genericB, r2.x, r2.y, r2.x + r2.w, r2.y + r2.h, radius2, stepsB);
            genericB.pathStroke(color, c.thickness, true);
            snprintf(what, sizeof what, "border %.0fx%.0f radius %.0f thickness %.0f%s equals the path (%s)", c.w, c.h, c.r, c.thickness,
                    transformed ? " transformed" : "", sameGeometry(fastB, genericB, 0.001f, why, sizeof why) ? "same" : why);
            check(sameGeometry(fastB, genericB, 0.001f, why, sizeof why), what);
        }
    }
    // a pill (the radius is half the height): no tables, the same as before, and still a closed shape
    {
        DrawList pill;
        pill.addRoundRectFilled({ 0, 0, 80, 20 }, 10.0f, Color(255, 255, 255, 255));
        check(pill.vertices().size() > 0 && pill.indices().size() > 0, "a pill is drawn");
    }
    // lines
    const Vec2 lines[][2] = { { { 0, 0 }, { 10, 0 } }, { { 5, 5 }, { 5, 40 } }, { { 1.5f, 2.5f }, { 30.25f, 17.75f } }, { { 100, 100 }, { 99, 130 } } };
    const float thicknesses[] = { 1.0f, 2.0f, 3.5f };
    for (size_t l = 0; l < sizeof lines / sizeof lines[0]; ++l)
        for (size_t t = 0; t < 3; ++t)
        {
            DrawList fast, generic;
            const Color color(10, 200, 90, 255);
            fast.addLine(lines[l][0], lines[l][1], color, thicknesses[t]);
            generic.pathLineTo(lines[l][0]);
            generic.pathLineTo(lines[l][1]);
            generic.pathStroke(color, thicknesses[t], false);
            char what[160], why[200];
            snprintf(what, sizeof what, "line %zu thickness %.1f equals the path (%s)", l, thicknesses[t],
                    sameGeometry(fast, generic, 0.001f, why, sizeof why) ? "same" : why);
            check(sameGeometry(fast, generic, 0.001f, why, sizeof why), what);
        }
}

// ── 3 ───────────────────────────────────────────────────────────────────────────────────────────────────────────────

// Two pages of one atlas as two textures of the rasteriser, each with the white texel at (0, 0).
struct Pages
{
    FontAtlas atlas;
    ig::zen::SoftRaster raster;
    TextureHandle page1, page2;
    bool ok = false;
    Vec2 uv2() const
    {
        return { 0.5f / static_cast<float>(atlas.dynamicWidth()), 0.5f / static_cast<float>(atlas.dynamicHeight()) };
    }
    Pages()
    {
        raster.setSamples(1);
        if (!atlas.buildDefault()) return;
        page1 = raster.createTexture(atlas.width(), atlas.height(), atlas.pixels());
        atlas.setTexture(page1);
        if (!atlas.prepareDynamic()) return;
        page2 = raster.createTexture(atlas.dynamicWidth(), atlas.dynamicHeight(), atlas.dynamicPixels());
        atlas.enableDynamic(page2);
        ok = true;
    }
    ~Pages()
    {
        raster.destroyTexture(page1);
        raster.destroyTexture(page2);
    }
};

uint32_t gSeed = 1;
uint32_t rnd()
{
    gSeed ^= gSeed << 13;
    gSeed ^= gSeed >> 17;
    gSeed ^= gSeed << 5;
    return gSeed;
}
float unit()
{
    return static_cast<float>(rnd() & 0xffffu) / 65536.0f;
}

// A random scene on `list`: shapes under random clips (some of them rounded), texts of both pages. Most shapes are put inside the
// clip in force (as widgets are), some anywhere (as a widget that is scrolled is), so that clips that do not matter and clips that do
// both come up.
void randomScene(DrawList& list, Pages& pages, int operations, bool withSecondPage)
{
    list.setWhitePixel(pages.page1, pages.atlas.whitePixelUV());
    if (withSecondPage) list.setSolidAlternate(pages.page2, pages.uv2());
    const Font& baked = pages.atlas.defaultFont();
    const Font* dynamicFont = pages.atlas.defaultFont().atHeight(15.0f);
    ct::Vector<Rect> clips;
    list.addRect({ 0, 0, 400, 300 }, Color(30, 30, 34, 255));
    for (int i = 0; i < operations; ++i)
    {
        const Rect cur = clips.empty() ? Rect{ 0, 0, 400, 300 } : clips.back();
        const uint32_t what = rnd() % 15;
        float x, y, w, h;
        if (rnd() % 100 < 70)
        {
            w = 4.0f + unit() * (cur.w * 0.5f);
            h = 4.0f + unit() * (cur.h * 0.5f);
            x = cur.x + unit() * maxf(0.0f, cur.w - w);
            y = cur.y + unit() * maxf(0.0f, cur.h - h);
        }
        else
        {
            x = unit() * 380.0f - 20.0f;
            y = unit() * 280.0f - 10.0f;
            w = 4.0f + unit() * 120.0f;
            h = 4.0f + unit() * 60.0f;
        }
        const Color c(static_cast<unsigned char>(40 + rnd() % 200), static_cast<unsigned char>(40 + rnd() % 200), static_cast<unsigned char>(40 + rnd() % 200),
                static_cast<unsigned char>(rnd() % 3 == 0 ? 120 : 255));
        switch (what)
        {
            case 0:
            case 1:
                list.addRect({ x, y, w, h }, c);
                break;
            case 2:
            case 3:
                list.addRoundRectFilled({ x, y, w, h }, 4.0f + unit() * 8.0f, c);
                break;
            case 4:
                list.addRoundRect({ x, y, w, h }, 4.0f, c, 1.0f);
                break;
            case 5:
                list.addLine({ x, y }, { x + w, y + h * 0.5f }, c, 1.0f + unit() * 2.0f);
                break;
            case 6:
                list.addCircleFilled({ x + 8.0f, y + 8.0f }, 3.0f + unit() * 5.0f, c);
                break;
            case 7:
            case 8:
                list.addText(baked, { x, y + 10.0f }, c, "Baked text", 1.0f);
                break;
            case 9:
            case 10:
                if (dynamicFont) list.addText(*dynamicFont, { x, y + 12.0f }, c, "Dynamic page 123", 1.0f);
                break;
            case 11:
            case 12: // a clip over the next shapes
                list.pushClip({ x, y, w, h });
                clips.push_back({ maxf(x, cur.x), maxf(y, cur.y), minf(w, cur.w), minf(h, cur.h) });
                break;
            case 13:
                if (!clips.empty())
                {
                    list.popClip();
                    clips.pop_back();
                }
                break;
            case 14: // a rounded clip
            {
                const float radii[4] = { 6.0f, 6.0f, 6.0f, 6.0f };
                list.pushClipRoundedRect({ x, y, w, h }, radii);
                clips.push_back({ maxf(x, cur.x), maxf(y, cur.y), minf(w, cur.w), minf(h, cur.h) });
                break;
            }
        }
    }
    while (!clips.empty())
    {
        list.popClip();
        clips.pop_back();
    }
}

void render(Pages& pages, DrawList& list, ct::Vector<uint32_t>& pixels)
{
    // (the rasteriser keeps the picture of its last call and gives it back for the same draw data: an empty frame in between
    // makes sure that this one is drawn)
    {
        DrawList empty;
        DrawData blank;
        blank.passes.push_back({ &empty, Camera2D{} });
        blank.displayWidth = 400.0f;
        blank.displayHeight = 300.0f;
        ct::Vector<uint32_t> scrap;
        scrap.resize(400u * 300u);
        pages.raster.render(blank, scrap.data(), 400, 300, 400, 0xff123456u);
    }
    DrawData data;
    data.passes.push_back({ &list, Camera2D{} });
    data.displayWidth = 400.0f;
    data.displayHeight = 300.0f;
    pixels.resize(400u * 300u);
    pages.raster.render(data, pixels.data(), 400, 300, 400, 0xff000000u);
}

void testMergingChangesNoPixel()
{
    printf("-- merged commands draw the same pixels\n");
    Pages pages;
    check(pages.ok, "the atlas and its pages are made");
    if (!pages.ok) return;
    int scenes = 0, differing = 0, fewer = 0, notMore = 0;
    uint32_t commandsMerged = 0, commandsApart = 0;
    for (int second = 0; second < 2; ++second)
    {
        for (int s = 0; s < 160; ++s)
        {
            gSeed = 0x9e3779b9u + static_cast<uint32_t>(s * 7919 + second * 104729);
            DrawList on, off;
            off.setCommandMerging(false);
            const uint32_t seed = gSeed;
            randomScene(on, pages, 60 + s % 80, second != 0);
            gSeed = seed;
            randomScene(off, pages, 60 + s % 80, second != 0);
            ++scenes;
            commandsMerged += static_cast<uint32_t>(on.commands().size());
            commandsApart += static_cast<uint32_t>(off.commands().size());
            if (on.commands().size() < off.commands().size()) ++fewer;
            if (on.commands().size() <= off.commands().size()) ++notMore;
            ct::Vector<uint32_t> a, b;
            render(pages, on, a);
            render(pages, off, b);
            bool same = a.size() == b.size();
            for (size_t i = 0; same && i < a.size(); ++i) same = a[i] == b[i];
            if (!same)
            {
                ++differing;
                if (differing <= 3) printf("  scene %d (second page %d): the pictures differ\n", s, second);
            }
        }
    }
    printf("  %d scenes: %u commands merged against %u apart; fewer in %d scenes, never more in %d\n", scenes, commandsMerged, commandsApart, fewer, notMore);
    check(differing == 0, "no random scene has a pixel that merging changed");
    check(notMore == scenes, "merging never makes more commands");
    check(commandsMerged * 10 < commandsApart * 9, "merging makes at least a tenth fewer commands in all");
}

// The sample buffer is a band of the window: the picture is the same whatever the band is, with the samples of every setting, and
// a texture that is white is kept as its alpha alone and gives the same pixels as the whole one.
void testBandsAndAlphaTextures()
{
    printf("-- bands of the sample buffer, alpha-only textures\n");
    Pages pages;
    if (!pages.ok) return;
    int scenes = 0, differing = 0;
    for (int samples = 2; samples <= 3; ++samples)
    {
        for (int s = 0; s < 40; ++s)
        {
            gSeed = 0x2545f491u + static_cast<uint32_t>(s * 6151 + samples * 977);
            DrawList list;
            randomScene(list, pages, 40 + s % 60, s % 2 == 0);
            pages.raster.setSamples(samples);
            ct::Vector<uint32_t> whole, banded, tiny;
            pages.raster.setBandRows(100000);
            render(pages, list, whole);
            pages.raster.setBandRows(128);
            pages.raster.setSamples(samples + 0); // (a new signature is not needed: the cache holds the picture of the last call, so ask for a different size first)
            render(pages, list, banded);
            pages.raster.setBandRows(8);
            render(pages, list, tiny);
            ++scenes;
            if (whole.size() != banded.size() || whole.size() != tiny.size() ||
                    memcmp(whole.data(), banded.data(), whole.size() * sizeof(uint32_t)) != 0 || memcmp(whole.data(), tiny.data(), whole.size() * sizeof(uint32_t)) != 0)
                ++differing;
        }
    }
    pages.raster.setSamples(1);
    char what[100];
    snprintf(what, sizeof what, "%d scenes drawn with one band, with 128 rows and with 8 give the same pixels (%d differ)", scenes, differing);
    check(differing == 0, what);

    // an alpha-only texture and a whole one with the same pixels draw the same
    {
        DrawList list;
        list.setWhitePixel(pages.page1, pages.atlas.whitePixelUV());
        list.addRect({ 0, 0, 400, 300 }, Color(30, 30, 34, 255));
        list.addText(pages.atlas.defaultFont(), { 10, 40 }, Color(240, 200, 100, 255), "Same pixels, a quarter of the memory", 1.0f);
        ct::Vector<uint32_t> a, b;
        render(pages, list, a);
        // the same atlas page with one pixel that is not white (a texel no glyph uses): a whole texture
        ct::Vector<unsigned char> page(static_cast<size_t>(pages.atlas.width()) * static_cast<size_t>(pages.atlas.height()) * 4u);
        memcpy(page.data(), pages.atlas.pixels(), page.size());
        const size_t last = page.size() - 4u;
        page[last] = 200; // (its rgb is not white now)
        page[last + 3] = 0;
        TextureHandle whole = pages.raster.createTexture(pages.atlas.width(), pages.atlas.height(), page.data());
        DrawList list2;
        list2.setWhitePixel(whole, pages.atlas.whitePixelUV());
        list2.addRect({ 0, 0, 400, 300 }, Color(30, 30, 34, 255));
        list2.addText(pages.atlas.defaultFont(), { 10, 40 }, Color(240, 200, 100, 255), "Same pixels, a quarter of the memory", 1.0f);
        render(pages, list2, b);
        check(a.size() == b.size() && memcmp(a.data(), b.data(), a.size() * sizeof(uint32_t)) == 0, "an alpha-only texture gives the pixels of the whole one");
        pages.raster.destroyTexture(whole);
    }
}

void testMergedCounts()
{
    printf("-- how many commands\n");
    Pages pages;
    if (!pages.ok) return;
    const Color c(200, 200, 200, 255);
    const Font& baked = pages.atlas.defaultFont();
    const Font* dyn = baked.atHeight(15.0f);
    check(dyn != nullptr, "a dynamic font");
    if (!dyn) return;

    // shapes and texts on the two pages: one command when the second page is the shapes' alternate, three or more when it is not
    {
        DrawList list;
        list.setWhitePixel(pages.page1, pages.atlas.whitePixelUV());
        list.setSolidAlternate(pages.page2, pages.uv2());
        list.addRect({ 0, 0, 50, 20 }, c);
        list.addText(*dyn, { 2, 14 }, c, "label");
        list.addRoundRectFilled({ 0, 30, 80, 24 }, 4.0f, c);
        list.addText(*dyn, { 4, 46 }, c, "button");
        list.addRoundRect({ 0, 30, 80, 24 }, 4.0f, c);
        list.addLine({ 0, 60 }, { 40, 60 }, c);
        check(list.commands().size() == 1, "shapes and texts of the second page are one command when it is the alternate");
        check(list.commands()[0].texture.value == pages.page2.value, "... on that page");
    }
    {
        DrawList list;
        list.setWhitePixel(pages.page1, pages.atlas.whitePixelUV());
        list.addRect({ 0, 0, 50, 20 }, c);
        list.addText(*dyn, { 2, 14 }, c, "label");
        list.addRect({ 0, 30, 50, 20 }, c);
        check(list.commands().size() == 3, "... and three when it is not (the old way)");
    }
    {
        DrawList list;
        list.setWhitePixel(pages.page1, pages.atlas.whitePixelUV());
        list.setSolidAlternate(pages.page2, pages.uv2());
        list.addRect({ 0, 0, 50, 20 }, c);
        list.addText(baked, { 2, 14 }, c, "baked");
        list.addText(*dyn, { 2, 34 }, c, "dynamic");
        check(list.commands().size() == 2, "a text of the first page and a text of the second are two commands (their pixels are on two pages)");
    }
    // clips: widgets that clip their own content inside the window clip
    {
        DrawList list;
        list.setWhitePixel(pages.page1, pages.atlas.whitePixelUV());
        list.setSolidAlternate(pages.page2, pages.uv2());
        for (int i = 0; i < 10; ++i)
        {
            list.addRoundRectFilled({ 10.0f, 10.0f + 30.0f * i, 100, 24 }, 4.0f, c);
            list.pushClip({ 14.0f, 10.0f + 30.0f * i, 92, 24 });
            list.addText(*dyn, { 16.0f, 26.0f + 30.0f * i }, c, "inside");
            list.popClip();
        }
        check(list.commands().size() == 1, "texts inside their own clip, between shapes with no clip, are one command with them");
    }
    {
        DrawList list;
        list.setWhitePixel(pages.page1, pages.atlas.whitePixelUV());
        list.setSolidAlternate(pages.page2, pages.uv2());
        list.pushClip({ 0, 0, 40, 40 });
        list.addRect({ 0, 0, 100, 100 }, c); // needs its clip: wider than it
        list.popClip();
        list.addRect({ 60, 60, 20, 20 }, c);
        check(list.commands().size() == 2, "a shape that needs its clip is not merged under a clip that does not cut it");
    }
    {
        // two windows: each draws inside its own clip; the two are one command under the union of the clips
        DrawList list;
        list.setWhitePixel(pages.page1, pages.atlas.whitePixelUV());
        list.pushClip({ 0, 0, 100, 100 });
        list.addRect({ 10, 10, 50, 50 }, c);
        list.popClip();
        list.pushClip({ 120, 0, 100, 100 });
        list.addRect({ 130, 10, 50, 50 }, c);
        list.popClip();
        check(list.commands().size() == 1, "two shapes, each inside its own clip, are one command");
        check(list.commands()[0].clip.x == 0 && list.commands()[0].clip.w == 220, "... under the union of the two clips");
    }
}

// ── 4 ───────────────────────────────────────────────────────────────────────────────────────────────────────────────

void testWhiteTexelOfThePages()
{
    printf("-- the white texel of the pages\n");
    FontAtlas atlas;
    check(atlas.buildDefault() && atlas.prepareDynamic(), "an atlas with a dynamic page");
    const Vec2 uv = atlas.whitePixelUV();
    check(uv.x == 0.5f / static_cast<float>(atlas.width()) && uv.y == 0.5f / static_cast<float>(atlas.height()), "the white texel of the baked page is at its pixel (0, 0)");
    const unsigned char* px = atlas.dynamicPixels();
    check(px[0] == 255 && px[1] == 255 && px[2] == 255 && px[3] == 255, "the dynamic page is white at (0, 0)");
    const Font* font = atlas.defaultFont().atHeight(15.0f);
    check(font != nullptr, "a font at a size the atlas has not baked");
    if (font)
    {
        // glyphs are rasterised into the page: the texel stays white, and no glyph is put on it
        for (uint32_t cp = 32; cp < 400; ++cp) font->findGlyph(cp);
        px = atlas.dynamicPixels();
        check(px[0] == 255 && px[1] == 255 && px[2] == 255 && px[3] == 255, "the texel is still white after 370 glyphs were rasterised");
        atlas.setPixelScale(2.0f); // every size is baked again at the new scale: the page is cleared
        px = atlas.dynamicPixels();
        check(px[0] == 255 && px[1] == 255 && px[2] == 255 && px[3] == 255, "... and after the page was cleared for a new pixel scale");
    }
}

} // namespace

int main()
{
    testCornerSegments();
    testFastPathsMatchThePath();
    testWhiteTexelOfThePages();
    testMergedCounts();
    testMergingChangesNoPixel();
    testBandsAndAlphaTextures();
    if (failures() == 0) printf("PASS\n");
    return failures() == 0 ? 0 : 1;
}
