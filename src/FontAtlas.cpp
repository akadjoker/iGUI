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
static const uint32_t ReplacementCodepoint = 63u;
static const uint32_t FirstArrowCodepoint = 0x2190u;
static const uint32_t LastArrowCodepoint = 0x2193u;

// Ranges esparsos: Latin-1, pontuação, símbolos, setas, matemática e formas.
// Evita reservar todos os codepoints entre U+0020 e U+27BF.
static const FontRange DefaultRanges[] = {
    FontRange(0x0020u, 0x00FFu), FontRange(0x2000u, 0x206Fu),
    FontRange(0x2100u, 0x214Fu), FontRange(0x2190u, 0x21FFu),
    FontRange(0x2200u, 0x22FFu), FontRange(0x2300u, 0x23FFu),
    FontRange(0x2500u, 0x257Fu), FontRange(0x2580u, 0x259Fu),
    FontRange(0x25A0u, 0x25FFu), FontRange(0x2600u, 0x26FFu),
    FontRange(0x2700u, 0x27BFu)
};

bool rangeContains(Span<const FontRange> ranges, uint32_t codepoint)
{
    for (Span<const FontRange>::size_type i = 0u; i < ranges.size(); ++i)
        if (codepoint >= ranges[i].first && codepoint <= ranges[i].last)
            return true;
    return false;
}
}

Span<const FontRange> defaultFontRanges()
{
    return Span<const FontRange>(DefaultRanges,
        static_cast<Span<const FontRange>::size_type>(sizeof(DefaultRanges) / sizeof(DefaultRanges[0])));
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

FontAtlas::FontAtlas() : FontAtlas(defaultFontRanges(), 2048u, 1024u, 14.0f) {}

FontAtlas::FontAtlas(Span<const FontRange> requestedRanges, uint32_t atlasWidth,
                     uint32_t atlasHeight, float requestedBakedSize)
    : pixels_(), glyphs_(), ascent_(0.0f), descent_(0.0f), lineHeight_(0.0f),
      bakedSize_(requestedBakedSize), width_(atlasWidth), height_(atlasHeight),
      texture_(), valid_(false)
{
    if (width_ == 0u || height_ == 0u || bakedSize_ <= 0.0f || requestedRanges.empty())
        return;
    pixels_.resize(static_cast<ct::Vector<uint8_t>::size_type>(width_) * height_, 0u);

    uint32_t glyphCount = 0u;
    uint32_t rangeCount = 0u;
    for (Span<const FontRange>::size_type i = 0u; i < requestedRanges.size(); ++i)
    {
        if (requestedRanges[i].last < requestedRanges[i].first)
            continue;
        const uint32_t count = requestedRanges[i].last - requestedRanges[i].first + 1u;
        if (count > 0x7FFFFFFFu - glyphCount)
            return;
        glyphCount += count;
        ++rangeCount;
    }
    if (glyphCount == 0u || rangeCount == 0u)
        return;
    ct::Vector<stbtt_packedchar> packed(glyphCount);
    ct::Vector<stbtt_pack_range> ranges(rangeCount);
    ct::Vector<FontRange> acceptedRanges;
    acceptedRanges.reserve(rangeCount);
    uint32_t packedOffset = 0u;
    uint32_t rangeIndex = 0u;
    for (Span<const FontRange>::size_type i = 0u; i < requestedRanges.size(); ++i)
    {
        if (requestedRanges[i].last < requestedRanges[i].first)
            continue;
        const uint32_t count = requestedRanges[i].last - requestedRanges[i].first + 1u;
        ranges[rangeIndex].font_size = bakedSize_;
        ranges[rangeIndex].first_unicode_codepoint_in_range =
            static_cast<int>(requestedRanges[i].first);
        ranges[rangeIndex].array_of_unicode_codepoints = nullptr;
        ranges[rangeIndex].num_chars = static_cast<int>(count);
        ranges[rangeIndex].chardata_for_range = packed.data() + packedOffset;
        acceptedRanges.push_back(requestedRanges[i]);
        packedOffset += count;
        ++rangeIndex;
    }

    stbtt_pack_context pack;
    if (!stbtt_PackBegin(&pack, pixels_.data(), static_cast<int>(width_),
                         static_cast<int>(height_), 0, 1, nullptr))
        return;
    stbtt_PackSetOversampling(&pack, 1u, 1u);
    stbtt_PackSetSkipMissingCodepoints(&pack, 1);
    // O retorno também é zero quando um range contém um codepoint ausente.
    // Os restantes glifos continuam corretamente empacotados.
    stbtt_PackFontRanges(&pack, detail::DefaultFontData, 0,
                         ranges.data(), static_cast<int>(rangeCount));
    stbtt_PackEnd(&pack);

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, detail::DefaultFontData, 0))
        return;
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    const float scale = stbtt_ScaleForPixelHeight(&info, bakedSize_);
    ascent_ = static_cast<float>(ascent) * scale;
    descent_ = static_cast<float>(descent) * scale;
    lineHeight_ = static_cast<float>(ascent - descent + lineGap) * scale;

    glyphs_.reserve(glyphCount);
    packedOffset = 0u;
    for (uint32_t acceptedIndex = 0u; acceptedIndex < rangeCount; ++acceptedIndex)
    {
        const uint32_t count = acceptedRanges[acceptedIndex].last -
                               acceptedRanges[acceptedIndex].first + 1u;
        for (uint32_t item = 0u; item < count; ++item)
        {
            const stbtt_packedchar &source = packed[packedOffset + item];
            if (source.x0 == source.x1 && source.y0 == source.y1 && source.xadvance == 0.0f)
                continue;
            FontGlyph glyphInfo;
            glyphInfo.codepoint = acceptedRanges[acceptedIndex].first + item;
            glyphInfo.x = source.x0;
            glyphInfo.y = source.y0;
            glyphInfo.width = static_cast<uint16_t>(source.x1 - source.x0);
            glyphInfo.height = static_cast<uint16_t>(source.y1 - source.y0);
            glyphInfo.offsetX = source.xoff;
            glyphInfo.offsetY = source.yoff;
            glyphInfo.advance = source.xadvance;
            glyphs_.push_back(glyphInfo);
        }
        packedOffset += count;
    }

    // Directional arrows are essential UI symbols. Some otherwise complete
    // text fonts (including Roboto Regular) omit them, so retain them as
    // vector glyphs instead of silently substituting '?'.
    for (uint32_t arrow = FirstArrowCodepoint; arrow <= LastArrowCodepoint; ++arrow)
    {
        if (!rangeContains(requestedRanges, arrow)) continue;
        bool present = false;
        for (ct::Vector<FontGlyph>::size_type i = 0u; i < glyphs_.size(); ++i)
            if (glyphs_[i].codepoint == arrow) { present = true; break; }
        if (present) continue;
        FontGlyph glyphInfo;
        glyphInfo.codepoint = arrow;
        glyphInfo.x = glyphInfo.y = glyphInfo.width = glyphInfo.height = 0u;
        glyphInfo.offsetX = glyphInfo.offsetY = 0.0f;
        glyphInfo.advance = bakedSize_ * 0.85f;
        glyphs_.push_back(glyphInfo);
    }
    valid_ = !glyphs_.empty();
}

bool FontAtlas::valid() const { return valid_; }
FontId FontAtlas::defaultFont() const { return FontId(1u); }
uint32_t FontAtlas::width() const { return width_; }
uint32_t FontAtlas::height() const { return height_; }
float FontAtlas::bakedSize() const { return bakedSize_; }
float FontAtlas::lineHeight() const { return lineHeight_; }
float FontAtlas::ascent() const { return ascent_; }
Span<const uint8_t> FontAtlas::pixels() const { return Span<const uint8_t>(pixels_); }
void FontAtlas::setTexture(TextureId texture) { texture_ = texture; }
TextureId FontAtlas::texture() const { return texture_; }

const FontGlyph *FontAtlas::glyph(FontId font, uint32_t codepoint) const
{
    if (!valid_ || font != defaultFont())
        return nullptr;
    const FontGlyph *replacement = nullptr;
    for (ct::Vector<FontGlyph>::size_type i = 0u; i < glyphs_.size(); ++i)
    {
        if (glyphs_[i].codepoint == codepoint)
            return &glyphs_[i];
        if (glyphs_[i].codepoint == ReplacementCodepoint)
            replacement = &glyphs_[i];
    }
    return replacement;
}

TextMetrics FontAtlas::measureText(FontId font, StringView text, float logicalSize) const
{
    TextMetrics result;
    if (!valid_ || logicalSize <= 0.0f)
        return result;
    const float scale = logicalSize / bakedSize_;
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

    const float scale = logicalSize / bakedSize_;
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
        if (glyphInfo->codepoint >= FirstArrowCodepoint &&
            glyphInfo->codepoint <= LastArrowCodepoint &&
            glyphInfo->width == 0u && glyphInfo->height == 0u)
        {
            const float left = penX + logicalSize * 0.08f;
            const float right = penX + glyphInfo->advance * scale - logicalSize * 0.08f;
            const float top = penY - ascent_ * scale + logicalSize * 0.20f;
            const float bottom = top + logicalSize * 0.60f;
            const float centerX = (left + right) * 0.5f;
            const float centerY = (top + bottom) * 0.5f;
            const float thickness = logicalSize >= 12.0f ? 1.5f : 1.0f;
            if (glyphInfo->codepoint == 0x2190u || glyphInfo->codepoint == 0x2192u)
            {
                const bool pointsLeft = glyphInfo->codepoint == 0x2190u;
                const float tip = pointsLeft ? left : right;
                const float tail = pointsLeft ? right : left;
                const float head = pointsLeft ? left + logicalSize * 0.28f
                                              : right - logicalSize * 0.28f;
                drawList.addLine(Vec2(tip, centerY), Vec2(tail, centerY), color, clip, thickness);
                drawList.addLine(Vec2(tip, centerY), Vec2(head, top), color, clip, thickness);
                drawList.addLine(Vec2(tip, centerY), Vec2(head, bottom), color, clip, thickness);
            }
            else
            {
                const bool pointsUp = glyphInfo->codepoint == 0x2191u;
                const float tip = pointsUp ? top : bottom;
                const float tail = pointsUp ? bottom : top;
                const float head = pointsUp ? top + logicalSize * 0.28f
                                            : bottom - logicalSize * 0.28f;
                drawList.addLine(Vec2(centerX, tip), Vec2(centerX, tail), color, clip, thickness);
                drawList.addLine(Vec2(centerX, tip), Vec2(left, head), color, clip, thickness);
                drawList.addLine(Vec2(centerX, tip), Vec2(right, head), color, clip, thickness);
            }
            penX += glyphInfo->advance * scale;
            continue;
        }
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
