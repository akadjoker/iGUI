#include "igui/FontAtlas.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace ig
{
namespace detail
{
extern const uint8_t DefaultFontData[];
extern const uint32_t DefaultFontDataSize;
} // namespace detail

namespace
{
static const uint32_t FirstCodepoint = 32u;
static const uint32_t LastCodepoint = 255u;
static const uint32_t ReplacementCodepoint = 63u;
// O tema base usa 14 px; gerar o atlas nessa escala evita downsampling borrado.
static const float BakedSize = 14.0f;
}

bool decodeUtf8(StringView text, StringView::size_type &offset, uint32_t &codepoint)
{
    if (offset >= text.size())
        return false;

    const uint8_t first = static_cast<uint8_t>(text[offset++]);
    if (first < 0x80u)
    {
        codepoint = first;
        return true;
    }

    uint32_t value = 0u;
    uint32_t trailing = 0u;
    uint32_t minimum = 0u;
    if (first >= 0xC2u && first <= 0xDFu)
    {
        value = first & 0x1Fu;
        trailing = 1u;
        minimum = 0x80u;
    }
    else if (first >= 0xE0u && first <= 0xEFu)
    {
        value = first & 0x0Fu;
        trailing = 2u;
        minimum = 0x800u;
    }
    else if (first >= 0xF0u && first <= 0xF4u)
    {
        value = first & 0x07u;
        trailing = 3u;
        minimum = 0x10000u;
    }
    else
    {
        codepoint = 0xFFFDu;
        return true;
    }

    if (offset + trailing > text.size())
    {
        codepoint = 0xFFFDu;
        return true;
    }
    for (uint32_t i = 0; i < trailing; ++i)
    {
        const uint8_t next = static_cast<uint8_t>(text[offset + i]);
        if ((next & 0xC0u) != 0x80u)
        {
            codepoint = 0xFFFDu;
            return true;
        }
        value = (value << 6u) | (next & 0x3Fu);
    }
    offset += trailing;
    if (value < minimum || value > 0x10FFFFu || (value >= 0xD800u && value <= 0xDFFFu))
    {
        codepoint = 0xFFFDu;
        return true;
    }
    codepoint = value;
    return true;
}

FontAtlas::FontAtlas()
    : pixels_(), glyphs_(), ascent_(0.0f), descent_(0.0f), lineHeight_(0.0f),
      width_(512u), height_(512u), texture_(), valid_(false)
{
    pixels_.resize(static_cast<ct::Vector<uint8_t>::size_type>(width_) * height_, 0u);
    glyphs_.resize(LastCodepoint - FirstCodepoint + 1u);
    ct::Vector<stbtt_bakedchar> baked;
    baked.resize(glyphs_.size());
    const int bakedHeight = stbtt_BakeFontBitmap(detail::DefaultFontData, 0, BakedSize,
                                                 pixels_.data(), static_cast<int>(width_),
                                                 static_cast<int>(height_), static_cast<int>(FirstCodepoint),
                                                 static_cast<int>(glyphs_.size()), baked.data());
    if (bakedHeight <= 0)
        return;

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, detail::DefaultFontData, 0))
        return;
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    const float scale = stbtt_ScaleForPixelHeight(&info, BakedSize);
    ascent_ = static_cast<float>(ascent) * scale;
    descent_ = static_cast<float>(descent) * scale;
    lineHeight_ = static_cast<float>(ascent - descent + lineGap) * scale;

    for (ct::Vector<FontGlyph>::size_type i = 0; i < glyphs_.size(); ++i)
    {
        FontGlyph &glyphInfo = glyphs_[i];
        const stbtt_bakedchar &source = baked[i];
        glyphInfo.codepoint = FirstCodepoint + static_cast<uint32_t>(i);
        glyphInfo.x = static_cast<uint16_t>(source.x0);
        glyphInfo.y = static_cast<uint16_t>(source.y0);
        glyphInfo.width = static_cast<uint16_t>(source.x1 - source.x0);
        glyphInfo.height = static_cast<uint16_t>(source.y1 - source.y0);
        glyphInfo.offsetX = source.xoff;
        glyphInfo.offsetY = source.yoff;
        glyphInfo.advance = source.xadvance;
    }
    valid_ = true;
}

bool FontAtlas::valid() const { return valid_; }
FontId FontAtlas::defaultFont() const { return FontId(1u); }
uint32_t FontAtlas::width() const { return width_; }
uint32_t FontAtlas::height() const { return height_; }
float FontAtlas::bakedSize() const { return BakedSize; }
float FontAtlas::lineHeight() const { return lineHeight_; }
float FontAtlas::ascent() const { return ascent_; }
Span<const uint8_t> FontAtlas::pixels() const { return Span<const uint8_t>(pixels_); }
void FontAtlas::setTexture(TextureId texture) { texture_ = texture; }
TextureId FontAtlas::texture() const { return texture_; }

const FontGlyph *FontAtlas::glyph(FontId font, uint32_t codepoint) const
{
    if (!valid_ || font != defaultFont())
        return nullptr;
    if (codepoint < FirstCodepoint || codepoint > LastCodepoint)
        codepoint = ReplacementCodepoint;
    return &glyphs_[codepoint - FirstCodepoint];
}

TextMetrics FontAtlas::measureText(FontId font, StringView text, float logicalSize) const
{
    TextMetrics result;
    if (!valid_ || logicalSize <= 0.0f)
        return result;
    const float scale = logicalSize / BakedSize;
    float lineWidth = 0.0f;
    uint32_t lines = 1u;
    StringView::size_type offset = 0u;
    uint32_t codepoint = 0u;
    while (decodeUtf8(text, offset, codepoint))
    {
        if (codepoint == static_cast<uint32_t>('\n'))
        {
            result.width = lineWidth > result.width ? lineWidth : result.width;
            lineWidth = 0.0f;
            ++lines;
            continue;
        }
        const FontGlyph *glyphInfo = glyph(font, codepoint);
        if (glyphInfo)
            lineWidth += glyphInfo->advance * scale;
    }
    result.width = lineWidth > result.width ? lineWidth : result.width;
    result.height = lineHeight_ * scale * static_cast<float>(lines);
    result.ascent = ascent_ * scale;
    result.descent = -descent_ * scale;
    return result;
}

bool FontAtlas::appendText(DrawList &drawList, FontId font, StringView text,
                           const Vec2 &position, float logicalSize,
                           const Color &color, const Rect &clip) const
{
    if (!valid_ || texture_.value == 0u || logicalSize <= 0.0f)
        return false;

    const float scale = logicalSize / BakedSize;
    float penX = position.x;
    float penY = position.y + ascent_ * scale;
    StringView::size_type offset = 0u;
    uint32_t codepoint = 0u;
    while (decodeUtf8(text, offset, codepoint))
    {
        if (codepoint == static_cast<uint32_t>('\n'))
        {
            penX = position.x;
            penY += lineHeight_ * scale;
            continue;
        }
        const FontGlyph *glyphInfo = glyph(font, codepoint);
        if (!glyphInfo)
            continue;
        if (glyphInfo->width != 0u && glyphInfo->height != 0u)
        {
            const Rect rect(penX + glyphInfo->offsetX * scale,
                            penY + glyphInfo->offsetY * scale,
                            static_cast<float>(glyphInfo->width) * scale,
                            static_cast<float>(glyphInfo->height) * scale);
            const Vec2 uvMin(static_cast<float>(glyphInfo->x) / static_cast<float>(width_),
                             static_cast<float>(glyphInfo->y) / static_cast<float>(height_));
            const Vec2 uvMax(static_cast<float>(glyphInfo->x + glyphInfo->width) / static_cast<float>(width_),
                             static_cast<float>(glyphInfo->y + glyphInfo->height) / static_cast<float>(height_));
            drawList.addImage(texture_, rect, uvMin, uvMax, color, clip);
        }
        penX += glyphInfo->advance * scale;
    }
    return true;
}

} // namespace ig
