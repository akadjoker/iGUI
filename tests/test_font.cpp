#include <assert.h>
#include <math.h>

#include <igui/FontAtlas.hpp>

static void test_utf8_decoder()
{
    const ig::StringView text(u8"Olá");
    ig::StringView::size_type offset = 0u;
    uint32_t codepoint = 0u;
    assert(ig::decodeUtf8(text, offset, codepoint) && codepoint == static_cast<uint32_t>('O'));
    assert(ig::decodeUtf8(text, offset, codepoint) && codepoint == static_cast<uint32_t>('l'));
    assert(ig::decodeUtf8(text, offset, codepoint) && codepoint == 0xE1u);
    assert(!ig::decodeUtf8(text, offset, codepoint));
}

static void test_default_font()
{
    ig::FontAtlas atlas;
    assert(atlas.valid());
    assert(atlas.width() == 2048u);
    assert(atlas.height() == 1024u);
    assert(atlas.glyph(atlas.defaultFont(), 0xE1u));
    assert(atlas.glyph(atlas.defaultFont(), 0x2026u));
    assert(atlas.glyph(atlas.defaultFont(), 0x2026u)->codepoint == 0x2026u);
    assert(atlas.glyph(atlas.defaultFont(), 0x2190u));
    assert(atlas.glyph(atlas.defaultFont(), 0x2190u)->codepoint == 0x2190u);

    const ig::TextMetrics metrics = atlas.measureText(atlas.defaultFont(), u8"Olá", 16.0f);
    assert(metrics.width > 0.0f);
    assert(metrics.height > 0.0f);
    assert(atlas.measureText(atlas.defaultFont(), u8"← Menu …", 16.0f).width > 0.0f);

    atlas.setTexture(ig::TextureId(1u));
    ig::DrawList drawList;
    assert(atlas.appendText(drawList, atlas.defaultFont(), u8"Olá", ig::Vec2(0.0f, 0.0f),
                            14.0f, ig::Color(), ig::Rect(0.0f, 0.0f, 100.0f, 100.0f)));
    const ig::DrawData data = drawList.data(ig::Vec2(100.0f, 100.0f), 1.0f);
    assert(data.commands.size() > 0u);
    for (ig::Span<const ig::DrawCommand>::size_type i = 0; i < data.commands.size(); ++i)
    {
        assert(data.commands[i].type == ig::DrawCommandType::Geometry);
        assert(data.commands[i].payload.geometry.texture == ig::TextureId(1u));
    }
}

static void test_custom_ranges_and_atlas_size()
{
    const ig::FontRange ranges[] = {
        ig::FontRange(0x20u, 0x7Eu),
        ig::FontRange(0x00C0u, 0x00FFu)
    };
    ig::FontAtlas atlas(ig::Span<const ig::FontRange>(ranges, 2u), 512u, 256u, 18.0f);
    assert(atlas.valid());
    assert(atlas.width() == 512u);
    assert(atlas.height() == 256u);
    assert(atlas.bakedSize() == 18.0f);
    assert(atlas.glyph(atlas.defaultFont(), static_cast<uint32_t>('A')));
    assert(atlas.glyph(atlas.defaultFont(), 0xE1u));
}

static void test_high_resolution_atlas()
{
    const ig::Span<const ig::FontRange> ranges = ig::defaultFontRanges();
    ig::FontAtlas atlas(ranges, 2048u, 1024u, 28.0f);
    assert(atlas.valid());
    assert(atlas.bakedSize() == 28.0f);
    assert(atlas.glyph(atlas.defaultFont(), static_cast<uint32_t>('A')));
    assert(atlas.measureText(atlas.defaultFont(), "Sharp text", 14.0f).width > 0.0f);
}

static void test_glyph_cache()
{
    ig::FontAtlas atlas;
    const auto font = atlas.defaultFont();
    const uint32_t points[] = {'A', 'A'+256u, 'A', 0x10ffffu, '?', 0xE1u};
    for (uint32_t point : points) {
        const auto* first = atlas.glyph(font,point);
        assert(first == atlas.glyph(font,point));
    }
    assert(atlas.glyph(font,0x10ffffu) == atlas.glyph(font,'?'));
    assert(atlas.glyph(ig::FontId(9999),'A') == nullptr);
    ig::FontAtlas copied = atlas;
    assert(copied.glyph(font,'A')->codepoint == 'A');
    assert(copied.glyph(font,'A') != atlas.glyph(font,'A'));
}

// appendText skips lines outside the clip without decoding them. Every glyph
// quad the unclipped call produces that can reach the clip (rounded outward
// like a backend scissor) must still come out, byte for byte.
static void test_append_text_skips_only_invisible_lines()
{
    ig::FontAtlas atlas;
    atlas.setTexture(ig::TextureId(1u));
    ig::String text;
    for (int line = 0; line < 400; ++line)
    {
        text += u8"linha ção € ";
        for (int word = 0; word < (line % 7) * 6; ++word)
            text += u8"palavra ";
        text += "\n";
    }
    const ig::Rect unbounded(-1e6f, -1e6f, 2e6f, 2e6f);
    ct::Vector<ig::Rect> clips;
    clips.push_back(ig::Rect(0.0f, 3000.0f, 300.0f, 200.0f));   // middle of the text
    clips.push_back(ig::Rect(40.0f, 0.0f, 120.0f, 50.5f));      // first lines, fractional edge
    clips.push_back(ig::Rect(900.0f, 1000.0f, 200.0f, 300.0f)); // right part of the long lines
    clips.push_back(ig::Rect(0.0f, 7995.0f, 400.0f, 300.0f));   // last lines
    clips.push_back(ig::Rect(0.0f, 20000.0f, 400.0f, 300.0f));  // below all text
    // Top and bottom edges swept across a whole line in sub-pixel steps, so
    // some clip cuts every line at every height.
    for (int step = 0; step < 40; ++step)
    {
        clips.push_back(ig::Rect(0.0f, 1500.0f + static_cast<float>(step) * 0.75f, 300.0f, 40.0f));
        clips.push_back(ig::Rect(0.0f, 1500.0f, 300.0f, 10.0f + static_cast<float>(step) * 0.75f));
    }
    ig::DrawList reference;
    assert(atlas.appendText(reference, atlas.defaultFont(), text, ig::Vec2(10.0f, 5.0f), 16.0f,
                            ig::Color(), unbounded));
    const ig::DrawData full = reference.data(ig::Vec2(1.0f, 1.0f), 1.0f);
    for (size_t c = 0; c < clips.size(); ++c)
    {
        const ig::Rect &clip = clips[c];
        ig::DrawList list;
        assert(atlas.appendText(list, atlas.defaultFont(), text, ig::Vec2(10.0f, 5.0f), 16.0f,
                                ig::Color(), clip));
        const ig::DrawData kept = list.data(ig::Vec2(1.0f, 1.0f), 1.0f);
        const float left = floorf(clip.x);
        const float top = floorf(clip.y);
        const float right = ceilf(clip.x + clip.width);
        const float bottom = ceilf(clip.y + clip.height);
        size_t visible = 0u;
        // Glyph quads are 4 vertices each, emitted in order.
        for (size_t v = 0; v + 3u < full.vertices.size(); v += 4u)
        {
            const ig::Vec2 &a = full.vertices[v].position;
            const ig::Vec2 &b = full.vertices[v + 2u].position;
            if (b.x <= left || a.x >= right || b.y <= top || a.y >= bottom)
                continue;
            ++visible;
            bool found = false;
            for (size_t k = 0; k + 3u < kept.vertices.size() && !found; k += 4u)
                found = kept.vertices[k].position.x == a.x && kept.vertices[k].position.y == a.y &&
                        kept.vertices[k + 2u].position.x == b.x && kept.vertices[k + 2u].position.y == b.y;
            assert(found);
        }
        // Nothing far outside is emitted: at most a margin of extra glyphs.
        assert(kept.vertices.size() <= (visible + 64u) * 4u);
        if (c == 0u)
            assert(visible > 50u); // the check above actually saw glyphs
    }
}

int main()
{
    test_glyph_cache();
    test_utf8_decoder();
    test_default_font();
    test_custom_ranges_and_atlas_size();
    test_high_resolution_atlas();
    test_append_text_skips_only_invisible_lines();
    return 0;
}
