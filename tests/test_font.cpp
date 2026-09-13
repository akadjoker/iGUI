#include <assert.h>

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

int main()
{
    test_glyph_cache();
    test_utf8_decoder();
    test_default_font();
    test_custom_ranges_and_atlas_size();
    test_high_resolution_atlas();
    return 0;
}
