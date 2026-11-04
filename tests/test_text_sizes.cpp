// The retained text engine: glyphs rasterised at the exact size asked for,
// kerning, and a dynamic page that can run out. Headless: the "texture" is a
// handle, nothing is drawn.

#include <igui/widgets/Retained.hpp>
#include <igui/widgets/Theme.hpp>

#include <math.h>
#include <stdio.h>
#include <string.h>

using namespace ig::retained;

namespace
{

int failures = 0;

void check(bool ok, const char* what)
{
    if (!ok)
    {
        printf("  FAIL %s\n", what);
        ++failures;
    }
}

// How many pixels of the glyph's rectangle in the dynamic page have ink.
int ink(const FontAtlas& atlas, const FontGlyph* g)
{
    if (!g || g->size.x <= 0.0f) return 0;
    const int w = atlas.dynamicWidth(), h = atlas.dynamicHeight();
    const int x0 = static_cast<int>(g->uv.x * static_cast<float>(w) + 0.5f), y0 = static_cast<int>(g->uv.y * static_cast<float>(h) + 0.5f);
    const int gw = static_cast<int>(g->size.x), gh = static_cast<int>(g->size.y);
    int n = 0;
    for (int y = y0; y < y0 + gh && y < h; ++y)
        for (int x = x0; x < x0 + gw && x < w; ++x)
            if (atlas.dynamicPixels()[(static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 4 + 3] > 0) ++n;
    return n;
}

TextureHandle handle(uintptr_t v)
{
    TextureHandle h;
    h.value = v;
    return h;
}

} // namespace

int main()
{
    FontAtlas atlas;
    check(atlas.buildDefault(), "the default atlas builds");
    const Font& base = atlas.defaultFont();
    atlas.setTexture(handle(1));

    // Without a dynamic page nothing changes: the baked font, no kerning.
    check(base.atHeight(24.0f) == &base, "without a dynamic page atHeight gives the baked font back");
    check(base.kerning('A', 'V') == 0.0f, "and the baked font has no kerning, as before");

    check(atlas.prepareDynamic() && !atlas.dynamicEnabled(), "preparing the page does not enable it");
    check(atlas.dynamicPixels() != nullptr && atlas.dynamicWidth() > 0, "it has pixels to make a texture from");
    check(atlas.enableDynamic(handle(2)) && atlas.dynamicEnabled(), "the adapter's texture enables it");

    // Sizes: a font per height, cached, an eighth of a pixel apart is one.
    const Font* f16 = base.atHeight(16.0f);
    const Font* f24 = base.atHeight(24.0f);
    check(f16 != &base && f24 != nullptr && f16 != f24, "each height is a font of its own, and 16 is no longer the baked one");
    check(base.atHeight(24.0f) == f24 && f24->atHeight(24.0f) == f24, "asking again gives the same font");
    check(base.atHeight(14.4f) == base.atHeight(14.4001f), "14.4 and 14.4001 are one font");
    check(f24->pixelHeight() == 24.0f && f24->texture().value == 2, "a sized font knows its height and uses the dynamic page");
    check(f24->lineHeight() > f16->lineHeight() * 1.4f && f24->ascender() > f16->ascender() * 1.4f, "its metrics scale with the height");

    // The glyphs are made for that size, not stretched.
    atlas.clearDynamicDirty();
    const FontGlyph* h12 = base.atHeight(12.0f)->findGlyph('H');
    check(atlas.dynamicDirty(), "the first use of a glyph rasterises it and marks the page for upload");
    atlas.clearDynamicDirty();
    const FontGlyph* h24 = f24->findGlyph('H');
    check(ink(atlas, h24) > 20, "and its pixels are in the page, at the place the glyph says");
    check(h12 && h24 && h24->size.y > h12->size.y * 1.8f && h24->size.y < h12->size.y * 2.2f, "an H at 24 is about twice as tall as at 12");
    atlas.clearDynamicDirty();
    f24->findGlyph('H');
    check(!atlas.dynamicDirty(), "a glyph already made does not touch the page again");
    const float a12 = base.atHeight(12.0f)->findGlyph('M')->advanceX, a24 = f24->findGlyph('M')->advanceX;
    check(a24 > a12 * 1.9f && a24 < a12 * 2.1f, "an advance scales with the size, as a float");

    // Kerning.
    check(f24->kerning('A', 'V') < 0.0f, "A and V pull together");
    check(f24->kerning('A', 'V') == f24->kerning('A', 'V'), "and the same answer comes from the cache");
    DrawList list;
    const float wa = list.calcTextSize(*f24, "A").x, wv = list.calcTextSize(*f24, "V").x, wav = list.calcTextSize(*f24, "AV").x;
    check(wav < wa + wv - 0.5f, "AV is narrower than A plus V");
    const float wi = list.calcTextSize(*f24, "i").x;
    check(fabsf(list.calcTextSize(*f24, "ii").x - 2 * wi) < 0.01f, "a pair with no kerning is just the two advances");

    // Coverage beyond what build() packs, and what the face lacks.
    const FontGlyph* cyr = f24->findGlyph(0x0416); // Ж
    const FontGlyph* lstroke = f24->findGlyph(0x0142); // ł
    const FontGlyph* q = f24->findGlyph('?');
    check(cyr && cyr->size.x > 0 && cyr->advanceX != q->advanceX, "a Cyrillic letter has its own glyph");
    check(lstroke && lstroke->size.x > 0, "so does a Latin Extended one");
    const FontGlyph* missing = f24->findGlyph(0x4E2D); // a CJK character: not in DejaVu Sans
    check(missing && missing->advanceX == q->advanceX, "one the face lacks is drawn as a question mark");
    const FontGlyph* sp = f24->findGlyph(' ');
    check(sp && sp->size.x == 0 && sp->advanceX > 0, "a space advances and has no bitmap");

    // A page that runs out starts again, and lookups keep working.
    bool allFound = true;
    for (int i = 0; i < 400; ++i)
    {
        const Font* f = base.atHeight(8.0f + 0.25f * static_cast<float>(i));
        for (const char* s = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"; *s; ++s)
        {
            const FontGlyph* g = f->findGlyph(static_cast<unsigned char>(*s));
            if (!g || g->advanceX <= 0.0f) allFound = false;
        }
    }
    const FontGlyph* again = f24->findGlyph('H');
    check(again && ink(atlas, again) > 20, "after a reset the glyph that is made again has its pixels where it says");
    check(allFound, "four hundred sizes of 62 glyphs fill the page more than once and every glyph still comes back");
    check(f24->findGlyph('H') != nullptr && f24->findGlyph('H')->size.y > 0, "and an old font bakes again after a reset");

    if (failures)
    {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("text sizes: ok\n");
    return 0;
}
