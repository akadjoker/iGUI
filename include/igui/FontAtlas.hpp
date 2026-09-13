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
              float bakedSize = 14.0f);

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
