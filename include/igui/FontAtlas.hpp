#pragma once

#include <stdint.h>

#include <ct/vector.hpp>

#include "Backend.hpp"

namespace ig
{

struct FontGlyph
{
    uint32_t codepoint;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    float offsetX;
    float offsetY;
    float advance;
    // Atlas texture coordinates of the glyph box, computed once here rather
    // than once per glyph per frame.
    float u0;
    float v0;
    float u1;
    float v1;
};

struct FontRange
{
    uint32_t first;
    uint32_t last;

    FontRange(uint32_t firstCodepoint, uint32_t lastCodepoint)
        : first(firstCodepoint), last(lastCodepoint) {}
};

// Defaults úteis, não um limite: a aplicação pode fornecer quaisquer ranges.
Span<const FontRange> defaultFontRanges();

class FontAtlas : public TextProvider
{
public:
    FontAtlas();
    FontAtlas(Span<const FontRange> ranges, uint32_t width, uint32_t height,
              float bakedSize = 16.0f);
    // Same atlas, built from the application's own TTF/OTF bytes instead of the
    // embedded default (see IGUI_FONT_INPUT): what an application needs to pick a
    // font at run time, or to load one from a file it was given. The bytes are
    // read while the atlas is built - the packed bitmaps are copied into the
    // atlas - so the buffer does not have to outlive the call. Note that
    // bakedSize, like stb's, is the font's ascent-to-descent span and not the em
    // square: a font whose ascent+descent is wider than its em (Roboto, most text
    // fonts) bakes a little smaller than the number suggests.
    FontAtlas(Span<const uint8_t> fontData, Span<const FontRange> ranges, uint32_t width,
              uint32_t height, float bakedSize = 16.0f);

    bool valid() const;
    FontId defaultFont() const;
    uint32_t width() const;
    uint32_t height() const;
    float bakedSize() const;
    float lineHeight() const;
    float ascent() const;
    Span<const uint8_t> pixels() const;

    void setTexture(TextureId texture);
    TextureId texture() const;

    // UV of the opaque white texel reserved in the first atlas row. Solid
    // geometry drawn with this texel and texture() shares the texture the glyphs
    // come from, so a backend merges shapes and text into one draw command - see
    // TextProvider::SolidStyle.
    Vec2 whiteUv() const;
    TextProvider::SolidStyle solidStyle() const override;

    TextMetrics measureText(FontId font, StringView text, float logicalSize) const override;
    const FontGlyph *glyph(FontId font, uint32_t codepoint) const;
    bool appendText(DrawList &drawList, FontId font, StringView text,
                    const Vec2 &position, float logicalSize,
                    const Color &color, const Rect &clip) const override;

private:
    ct::Vector<uint8_t> pixels_;
    ct::Vector<FontGlyph> glyphs_;
    float ascent_;
    float descent_;
    float lineHeight_;
    float bakedSize_;
    uint32_t width_;
    uint32_t height_;
    TextureId texture_;
    bool valid_;
    // codepoint -> index into glyphs_ for every codepoint below lookupLimit_
    // (one past the highest baked codepoint, at most U+FFFF): a text run does
    // one load per glyph instead of a search. A missing codepoint maps to the
    // replacement glyph. Codepoints at or above the limit use the cache below.
    ct::Vector<uint16_t> lookup_;
    uint32_t lookupLimit_;
    const FontGlyph *lookupGlyph(uint32_t codepoint) const;
    // Common body of the constructors: packs fontData into the atlas the members
    // already describe.
    void build(Span<const uint8_t> fontData, Span<const FontRange> requestedRanges);
    // Fixed-size, per-atlas lookup cache. Indices remain valid when copied.
    struct GlyphCacheEntry {
        uint32_t codepoint = 0;
        int index = -1;
        bool occupied = false;
    };
    mutable GlyphCacheEntry glyphCache_[256];
};

// Lê um codepoint UTF-8. Para uma sequência inválida devolve U+FFFD e avança um byte.
bool decodeUtf8(StringView text, StringView::size_type &offset, uint32_t &codepoint);

} // namespace ig
