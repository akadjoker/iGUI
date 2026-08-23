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
    assert(atlas.width() == 512u);
    assert(atlas.height() == 512u);
    assert(atlas.glyph(atlas.defaultFont(), 0xE1u));

    const ig::TextMetrics metrics = atlas.measureText(atlas.defaultFont(), u8"Olá", 16.0f);
    assert(metrics.width > 0.0f);
    assert(metrics.height > 0.0f);

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

int main()
{
    test_utf8_decoder();
    test_default_font();
    return 0;
}
