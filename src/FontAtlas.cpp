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
static const uint16_t NoGlyph = 0xFFFFu;
static const uint32_t FirstArrowCodepoint = 0x2190u;
static const uint32_t LastArrowCodepoint = 0x2193u;

// Ranges esparsos: Latin-1, pontuação, símbolos, setas, matemática e formas.
// Evita reservar todos os codepoints entre U+0020 e U+27BF.
static const FontRange DefaultRanges[] = { FontRange(0x0020u, 0x00FFu), FontRange(0x2000u, 0x206Fu),
    FontRange(0x2100u, 0x214Fu), FontRange(0x2190u, 0x21FFu), FontRange(0x2200u, 0x22FFu),
    FontRange(0x2300u, 0x23FFu), FontRange(0x2500u, 0x257Fu), FontRange(0x2580u, 0x259Fu),
    FontRange(0x25A0u, 0x25FFu), FontRange(0x2600u, 0x26FFu), FontRange(0x2700u, 0x27BFu) };

bool rangeContains(Span<const FontRange> ranges, uint32_t codepoint)
{
    for (Span<const FontRange>::size_type i = 0u; i < ranges.size(); ++i)
        if (codepoint >= ranges[i].first && codepoint <= ranges[i].last) return true;
    return false;
}
} // namespace

Span<const FontRange> defaultFontRanges()
{
    return Span<const FontRange>(DefaultRanges,
            static_cast<Span<const FontRange>::size_type>(
                    sizeof(DefaultRanges) / sizeof(DefaultRanges[0])));
}

bool decodeUtf8(StringView text, StringView::size_type& offset, uint32_t& codepoint)
{
    if (offset >= text.size()) return false;

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

namespace
{
uint32_t bigEndian32(const uint8_t* bytes)
{
    return (static_cast<uint32_t>(bytes[0]) << 24) | (static_cast<uint32_t>(bytes[1]) << 16) |
           (static_cast<uint32_t>(bytes[2]) << 8) | static_cast<uint32_t>(bytes[3]);
}

// A tabela está no directório? Quem chama já confirmou que o directório e todos
// os pedaços que ele aponta cabem nos bytes entregues.
bool hasTable(const uint8_t* bytes, uint32_t tableCount, const char* tag)
{
    for (uint32_t i = 0u; i < tableCount; ++i)
    {
        const uint8_t* record = bytes + 12u + static_cast<uint32_t>(i) * 16u;
        if (record[0] == static_cast<uint8_t>(tag[0]) &&
                record[1] == static_cast<uint8_t>(tag[1]) &&
                record[2] == static_cast<uint8_t>(tag[2]) &&
                record[3] == static_cast<uint8_t>(tag[3]))
            return true;
    }
    return false;
}

// stb_truetype não valida o que lhe dão: um ficheiro que não seja uma fonte, ou
// uma fonte truncada, fá-lo ler fora do buffer (segfault garantido num ficheiro
// escolhido pelo utilizador). O construtor e o loadFont levam bytes da aplicação,
// por isso confirma-se primeiro que cabe uma tabela de offsets, que o tipo é
// conhecido, que todas as tabelas cabem no que foi entregue e que estão lá as que
// o stb vai usar sem as procurar outra vez (o cmap sobretudo: sem ele o
// FindGlyphIndex segue o lixo que encontrar como se fosse uma tabela de
// segmentos). Só depois é que o stb lhe toca.
bool looksLikeFont(Span<const uint8_t> data)
{
    if (data.size() < 12u) return false;
    const uint8_t* bytes = data.data();
    const uint32_t tag = bigEndian32(bytes);
    if (tag == 0x74746366u) // 'ttcf': uma colecção; o stb resolve o índice 0 sozinho
        return data.size() >= 12u;
    const bool knownFormat = tag == 0x00010000u || // TrueType
                             tag == 0x74727565u || // 'true'
                             tag == 0x4F54544Fu;   // 'OTTO' (CFF/OpenType)
    if (!knownFormat) return false;
    const uint32_t tableCount =
            (static_cast<uint32_t>(bytes[4]) << 8) | static_cast<uint32_t>(bytes[5]);
    const uint64_t directoryEnd = 12u + static_cast<uint64_t>(tableCount) * 16u;
    if (directoryEnd > data.size()) return false;
    for (uint32_t i = 0u; i < tableCount; ++i)
    {
        const uint8_t* record = bytes + 12u + static_cast<uint32_t>(i) * 16u;
        const uint64_t offset = bigEndian32(record + 8);
        const uint64_t length = bigEndian32(record + 12);
        if (offset + length > data.size()) return false;
    }
    // As tabelas que o stb lê sem as validar: o cmap é o índice dos codepoints,
    // o head/hhea dão-lhe as métricas e o contorno vem do glyf (TrueType) ou do
    // CFF (OpenType). Sem qualquer uma delas o stb não sabe que não sabe: segfault
    // ou métricas a dividir por zero.
    const bool cff = tag == 0x4F54544Fu;
    return hasTable(bytes, tableCount, "cmap") && hasTable(bytes, tableCount, "head") &&
           hasTable(bytes, tableCount, "hhea") &&
           hasTable(bytes, tableCount, cff ? "CFF " : "glyf");
}

Span<const uint8_t> embeddedFontData()
{
    return Span<const uint8_t>(detail::DefaultFontData, detail::DefaultFontDataSize);
}
} // namespace

FontAtlas::FontAtlas()
        : FontAtlas(defaultFontRanges(), 2048u, 1024u, 16.0f)
{
}

FontAtlas::FontAtlas(Span<const FontRange> ranges, uint32_t atlasWidth, uint32_t atlasHeight,
        float requestedBakedSize)
        : FontAtlas(embeddedFontData(), ranges, atlasWidth, atlasHeight, requestedBakedSize)
{
}

FontAtlas::FontAtlas(Span<const uint8_t> fontData, Span<const FontRange> requestedRanges,
        uint32_t atlasWidth, uint32_t atlasHeight, float requestedBakedSize)
        : pixels_(),
          faces_(),
          ranges_(requestedRanges),
          width_(atlasWidth),
          height_(atlasHeight),
          texture_(),
          valid_(false)
{
    loadFont(fontData, requestedBakedSize, requestedRanges);
}

FontId FontAtlas::loadFont(Span<const uint8_t> fontData, float bakedSize,
        Span<const FontRange> requestedRanges)
{
    const float resolvedSize =
            bakedSize > 0.0f ? bakedSize : (faces_.empty() ? 16.0f : faces_[0].bakedSize);
    if (requestedRanges.empty()) requestedRanges = ranges_;
    if (requestedRanges.empty() || !looksLikeFont(fontData) || width_ == 0u || height_ == 0u ||
            resolvedSize <= 0.0f)
        return FontId(0u);
    Face face;
    face.data.assign(fontData.begin(), fontData.end());
    face.ranges.assign(requestedRanges.begin(), requestedRanges.end());
    face.bakedSize = resolvedSize;
    faces_.push_back(face);
    // O empacotador do stb não guarda estado entre chamadas, por isso uma face nova
    // reconstrói o atlas inteiro a partir das bytes que cada face guardou.
    rebuild();
    if (faces_.back().glyphs.empty())
    {
        // Uma face que não deu glifos nenhuns não fica no atlas nem ocupa um id: o
        // que já lá estava tem de ficar como estava (as uvs das outras foram
        // calculadas sobre uma imagem que esta podia ter mexido).
        faces_.pop_back();
        if (faces_.empty()) valid_ = false;
        else
            rebuild();
        return FontId(0u);
    }
    return FontId(static_cast<uint32_t>(faces_.size()));
}

uint32_t FontAtlas::faceCount() const
{
    return static_cast<uint32_t>(faces_.size());
}

uint32_t FontAtlas::faceIndex(FontId font) const
{
    // O id 0 não é de face nenhuma (o loadFont só devolve ids a partir de 1): um
    // texto com esse id não se desenha, em vez de sair na primeira face.
    if (font.value == 0u) return 0xFFFFFFFFu;
    const uint32_t index = static_cast<uint32_t>(font.value - 1u);
    return index < faces_.size() ? index : 0xFFFFFFFFu;
}

float FontAtlas::bakedSize(FontId font) const
{
    const uint32_t index = faceIndex(font);
    return index == 0xFFFFFFFFu ? 0.0f : faces_[index].bakedSize;
}

float FontAtlas::lineHeight(FontId font) const
{
    const uint32_t index = faceIndex(font);
    return index == 0xFFFFFFFFu ? 0.0f : faces_[index].lineHeight;
}

float FontAtlas::ascent(FontId font) const
{
    const uint32_t index = faceIndex(font);
    return index == 0xFFFFFFFFu ? 0.0f : faces_[index].ascent;
}

void FontAtlas::rebuild()
{
    if (width_ == 0u || height_ == 0u || faces_.empty()) return;
    // Two rows minimum: the first is reserved as opaque white (see below).
    if (width_ < 2u || height_ < 2u) return;
    pixels_.resize(static_cast<ct::Vector<uint8_t>::size_type>(width_) * height_, 0u);
    // The first row is opaque white. Solid UI geometry is drawn sampling that
    // texel with this atlas as its texture, so shapes and glyphs share one
    // texture and a backend can merge them into a single draw command (see
    // DrawList::setSolidStyle). The packer is then handed the bitmap one row in,
    // which is why every stored glyph row is offset by one.
    for (uint32_t x = 0u; x < width_; ++x) pixels_[x] = 255u;

    stbtt_pack_context pack;
    if (!stbtt_PackBegin(&pack, pixels_.data() + width_, static_cast<int>(width_),
                static_cast<int>(height_ - 1u), 0, 1, nullptr))
        return;
    stbtt_PackSetOversampling(&pack, 1u, 1u);
    stbtt_PackSetSkipMissingCodepoints(&pack, 1);
    bool any = false;
    for (ct::Vector<Face>::size_type faceIndex = 0u; faceIndex < faces_.size(); ++faceIndex)
        any = packFace(faces_[faceIndex], pack) || any;
    stbtt_PackEnd(&pack);
    valid_ = any;
}

bool FontAtlas::packFace(Face& face, stbtt_pack_context& pack)
{
    face.glyphs.clear();
    face.lookup.clear();
    face.lookupLimit = 0u;
    // Os ranges desta face (os do atlas se ela não tiver nenhuns).
    const Span<const FontRange> requestedRanges =
            face.ranges.empty() ? ranges_
                                : Span<const FontRange>(face.ranges.data(), face.ranges.size());
    if (face.data.empty() || requestedRanges.empty() || face.bakedSize <= 0.0f) return false;
    uint32_t glyphCount = 0u;
    uint32_t rangeCount = 0u;
    for (Span<const FontRange>::size_type i = 0u; i < requestedRanges.size(); ++i)
    {
        if (requestedRanges[i].last < requestedRanges[i].first) continue;
        const uint32_t count = requestedRanges[i].last - requestedRanges[i].first + 1u;
        if (count > 0x7FFFFFFFu - glyphCount) return false;
        glyphCount += count;
        ++rangeCount;
    }
    if (glyphCount == 0u || rangeCount == 0u) return false;
    ct::Vector<stbtt_packedchar> packed(glyphCount);
    ct::Vector<stbtt_pack_range> ranges(rangeCount);
    ct::Vector<FontRange> acceptedRanges;
    acceptedRanges.reserve(rangeCount);
    uint32_t packedOffset = 0u;
    uint32_t rangeIndex = 0u;
    for (Span<const FontRange>::size_type i = 0u; i < requestedRanges.size(); ++i)
    {
        if (requestedRanges[i].last < requestedRanges[i].first) continue;
        const uint32_t count = requestedRanges[i].last - requestedRanges[i].first + 1u;
        ranges[rangeIndex].font_size = face.bakedSize;
        ranges[rangeIndex].first_unicode_codepoint_in_range =
                static_cast<int>(requestedRanges[i].first);
        ranges[rangeIndex].array_of_unicode_codepoints = nullptr;
        ranges[rangeIndex].num_chars = static_cast<int>(count);
        ranges[rangeIndex].chardata_for_range = packed.data() + packedOffset;
        acceptedRanges.push_back(requestedRanges[i]);
        packedOffset += count;
        ++rangeIndex;
    }
    // O retorno também é zero quando um range contém um codepoint ausente.
    // Os restantes glifos continuam corretamente empacotados.
    stbtt_PackFontRanges(&pack, face.data.data(), 0, ranges.data(), static_cast<int>(rangeCount));
    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, face.data.data(), 0)) return false;
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    const float scale = stbtt_ScaleForPixelHeight(&info, face.bakedSize);
    face.ascent = static_cast<float>(ascent) * scale;
    face.descent = static_cast<float>(descent) * scale;
    face.lineHeight = static_cast<float>(ascent - descent + lineGap) * scale;

    face.glyphs.reserve(glyphCount);
    packedOffset = 0u;
    for (uint32_t acceptedIndex = 0u; acceptedIndex < rangeCount; ++acceptedIndex)
    {
        const uint32_t count =
                acceptedRanges[acceptedIndex].last - acceptedRanges[acceptedIndex].first + 1u;
        for (uint32_t item = 0u; item < count; ++item)
        {
            const stbtt_packedchar& source = packed[packedOffset + item];
            if (source.x0 == source.x1 && source.y0 == source.y1 && source.xadvance == 0.0f)
                continue;
            FontGlyph glyphInfo;
            glyphInfo.codepoint = acceptedRanges[acceptedIndex].first + item;
            glyphInfo.x = source.x0;
            glyphInfo.y = static_cast<uint16_t>(source.y0 + 1);
            glyphInfo.width = static_cast<uint16_t>(source.x1 - source.x0);
            glyphInfo.height = static_cast<uint16_t>(source.y1 - source.y0);
            glyphInfo.offsetX = source.xoff;
            glyphInfo.offsetY = source.yoff;
            glyphInfo.advance = source.xadvance;
            glyphInfo.u0 = glyphInfo.v0 = glyphInfo.u1 = glyphInfo.v1 = 0.0f; // set below
            face.glyphs.push_back(glyphInfo);
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
        for (ct::Vector<FontGlyph>::size_type i = 0u; i < face.glyphs.size(); ++i)
            if (face.glyphs[i].codepoint == arrow)
            {
                present = true;
                break;
            }
        if (present) continue;
        FontGlyph glyphInfo;
        glyphInfo.codepoint = arrow;
        glyphInfo.x = glyphInfo.y = glyphInfo.width = glyphInfo.height = 0u;
        glyphInfo.offsetX = glyphInfo.offsetY = 0.0f;
        glyphInfo.advance = face.bakedSize * 0.85f;
        glyphInfo.u0 = glyphInfo.v0 = glyphInfo.u1 = glyphInfo.v1 = 0.0f;
        face.glyphs.push_back(glyphInfo);
    }

    uint32_t highest = 0u;
    uint16_t replacement = NoGlyph;
    for (ct::Vector<FontGlyph>::size_type i = 0u; i < face.glyphs.size(); ++i)
    {
        FontGlyph& g = face.glyphs[i];
        g.u0 = static_cast<float>(g.x) / static_cast<float>(width_);
        g.v0 = static_cast<float>(g.y) / static_cast<float>(height_);
        g.u1 = static_cast<float>(g.x + g.width) / static_cast<float>(width_);
        g.v1 = static_cast<float>(g.y + g.height) / static_cast<float>(height_);
        if (g.codepoint > highest) highest = g.codepoint;
        if (g.codepoint == ReplacementCodepoint && replacement == NoGlyph)
            replacement = static_cast<uint16_t>(i);
    }
    if (!face.glyphs.empty() &&
            face.glyphs.size() < static_cast<ct::Vector<FontGlyph>::size_type>(NoGlyph))
    {
        face.lookupLimit = highest >= 0xFFFFu ? 0x10000u : highest + 1u;
        face.lookup.assign(face.lookupLimit, replacement);
        for (ct::Vector<FontGlyph>::size_type i = 0u; i < face.glyphs.size(); ++i)
            if (face.glyphs[i].codepoint < face.lookupLimit)
                face.lookup[face.glyphs[i].codepoint] = static_cast<uint16_t>(i);
    }
    return !face.glyphs.empty();
}

bool FontAtlas::valid() const
{
    return valid_;
}
FontId FontAtlas::defaultFont() const
{
    return FontId(1u);
}
uint32_t FontAtlas::width() const
{
    return width_;
}
uint32_t FontAtlas::height() const
{
    return height_;
}
float FontAtlas::bakedSize() const
{
    return bakedSize(FontId(1u));
}
float FontAtlas::lineHeight() const
{
    return lineHeight(FontId(1u));
}
float FontAtlas::ascent() const
{
    return ascent(FontId(1u));
}
Span<const uint8_t> FontAtlas::pixels() const
{
    return Span<const uint8_t>(pixels_);
}
void FontAtlas::setTexture(TextureId texture)
{
    texture_ = texture;
}
TextureId FontAtlas::texture() const
{
    return texture_;
}

Vec2 FontAtlas::whiteUv() const
{
    // Centre of the reserved white texel: sampling the centre is what keeps a
    // linearly filtered atlas from bleeding the neighbouring glyph into a solid
    // quad.
    return Vec2(0.5f / static_cast<float>(width_), 0.5f / static_cast<float>(height_));
}

TextProvider::SolidStyle FontAtlas::solidStyle() const
{
    SolidStyle style;
    if (valid_ && texture_.value != 0u)
    {
        style.texture = texture_;
        style.uv = whiteUv();
    }
    return style;
}

const FontGlyph* FontAtlas::glyph(FontId font, uint32_t codepoint) const
{
    const uint32_t index = faceIndex(font);
    if (!valid_ || index == 0xFFFFFFFFu) return nullptr;
    return lookupGlyph(faces_[index], codepoint);
}

const FontGlyph* FontAtlas::lookupGlyph(const Face& face, uint32_t codepoint) const
{
    if (codepoint < face.lookupLimit)
    {
        const uint16_t index = face.lookup[codepoint];
        return index != NoGlyph ? &face.glyphs[index] : nullptr;
    }
    GlyphCacheEntry& cached = face.glyphCache[codepoint % 256u];
    if (cached.occupied && cached.codepoint == codepoint)
        return cached.index >= 0 ? &face.glyphs[static_cast<size_t>(cached.index)] : nullptr;
    int found = -1;
    int replacement = -1;
    for (size_t i = 0u; i < face.glyphs.size(); ++i)
    {
        if (face.glyphs[i].codepoint == codepoint)
        {
            found = static_cast<int>(i);
            break;
        }
        if (face.glyphs[i].codepoint == ReplacementCodepoint) replacement = static_cast<int>(i);
    }
    cached.occupied = true;
    cached.codepoint = codepoint;
    cached.index = found >= 0 ? found : replacement;
    return cached.index >= 0 ? &face.glyphs[static_cast<size_t>(cached.index)] : nullptr;
}

TextMetrics FontAtlas::measureText(FontId font, StringView text, float logicalSize) const
{
    TextMetrics result;
    const uint32_t faceIndexValue = faceIndex(font);
    if (!valid_ || logicalSize <= 0.0f || faceIndexValue == 0xFFFFFFFFu) return result;
    const Face& face = faces_[faceIndexValue];
    const float scale = logicalSize / face.bakedSize;
    const bool known = true;
    float lineWidth = 0.0f;
    uint32_t lines = 1u;
    const char* const data = text.data();
    const StringView::size_type size = text.size();
    StringView::size_type offset = 0u;
    while (offset < size)
    {
        // Most UI text is ASCII: one byte, no decoder.
        uint32_t codepoint = static_cast<uint8_t>(data[offset]);
        if (codepoint < 0x80u) ++offset;
        else
            decodeUtf8(text, offset, codepoint);
        if (codepoint == static_cast<uint32_t>('\n'))
        {
            result.width = lineWidth > result.width ? lineWidth : result.width;
            lineWidth = 0.0f;
            ++lines;
            continue;
        }
        if (!known) continue;
        const FontGlyph* glyphInfo = lookupGlyph(face, codepoint);
        if (glyphInfo) lineWidth += glyphInfo->advance * scale;
    }
    result.width = lineWidth > result.width ? lineWidth : result.width;
    result.height = face.lineHeight * scale * static_cast<float>(lines);
    result.ascent = face.ascent * scale;
    result.descent = -face.descent * scale;
    return result;
}

bool FontAtlas::appendText(DrawList& drawList, FontId font, StringView text, const Vec2& position,
        float logicalSize, const Color& color, const Rect& clip) const
{
    const uint32_t faceIndexValue = faceIndex(font);
    if (!valid_ || texture_.value == 0u || logicalSize <= 0.0f) return false;
    // An unknown font has no glyphs here; the text is consumed, not drawn.
    if (faceIndexValue == 0xFFFFFFFFu) return true;
    const Face& face = faces_[faceIndexValue];

    const float scale = logicalSize / face.bakedSize;
    float penX = position.x;
    float penY = position.y + face.ascent * scale;
    // Glyph quads share one clip rect and one texture, so they are written as a
    // single run: the whole text becomes one geometry command, and the vertices
    // go straight into the list instead of through one call per glyph.
    DrawList::QuadBatch quads(drawList, texture_, clip);
    // Worst case is one quad per byte; a run this long is not a UI label, and
    // the cap keeps the reservation from overflowing.
    const StringView::size_type runLength = text.size();
    quads.reserve(runLength > 1048576u ? 1048576u : static_cast<uint32_t>(runLength));
    // Lines wholly outside the clip are skipped without decoding their glyphs
    // (DrawList would cull every quad anyway, but a long multi-line text is
    // thousands of glyph lookups per frame for nothing). A line's box is
    // padded by half a line above and below: accents and descenders reach
    // past ascent/lineHeight, and a false "visible" only costs the lookups.
    const float line = face.lineHeight * scale;
    const float clipRight = clip.x + clip.width + 2.0f;
    const float clipBottom = clip.y + clip.height + 2.0f;
    const float clipTop = clip.y - 2.0f;
    const char* const data = text.data();
    const StringView::size_type size = text.size();
    StringView::size_type offset = 0u;
    while (offset < size)
    {
        // Per line, not per glyph: where this line sits against the clip.
        const float lineTop = penY - face.ascent * scale - line * 0.5f;
        if (lineTop > clipBottom) break; // text only moves down from here
        const bool lineVisible = lineTop + line * 2.0f >= clipTop;
        bool newline = false;
        while (offset < size && !newline)
        {
            if (!lineVisible || penX > clipRight)
            {
                // Rest of this line is invisible: jump to its '\n' (a byte that
                // never occurs inside a UTF-8 multi-byte sequence).
                while (offset < size && data[offset] != '\n') ++offset;
                if (offset == size) break;
            }
            uint32_t codepoint = static_cast<uint8_t>(data[offset]);
            if (codepoint < 0x80u) ++offset;
            else
                decodeUtf8(text, offset, codepoint);
            if (codepoint == static_cast<uint32_t>('\n'))
            {
                penX = position.x;
                penY += line;
                newline = true;
                break;
            }
            const FontGlyph* glyphInfo = lookupGlyph(face, codepoint);
            if (!glyphInfo) continue;
            if (glyphInfo->width != 0u && glyphInfo->height != 0u)
            {
                // A posição de cada glifo cai em coordenadas inteiras, como no
                // ImGui: com uma posição fracionária o filtro linear do atlas
                // interpola entre texels vizinhos, e o texto sai desfocado e mais
                // baço do que a cor pedida. O avanço continua fracionário, por
                // isso o espaçamento entre letras é que absorve o arredondamento.
                quads.add(Rect(truncf(penX + glyphInfo->offsetX * scale),
                                  truncf(penY + glyphInfo->offsetY * scale),
                                  static_cast<float>(glyphInfo->width) * scale,
                                  static_cast<float>(glyphInfo->height) * scale),
                        Vec2(glyphInfo->u0, glyphInfo->v0), Vec2(glyphInfo->u1, glyphInfo->v1),
                        color);
            }
            else if (glyphInfo->codepoint >= FirstArrowCodepoint &&
                     glyphInfo->codepoint <= LastArrowCodepoint && glyphInfo->width == 0u &&
                     glyphInfo->height == 0u)
            {
                const float left = penX + logicalSize * 0.08f;
                const float right = penX + glyphInfo->advance * scale - logicalSize * 0.08f;
                const float top = penY - face.ascent * scale + logicalSize * 0.20f;
                const float bottom = top + logicalSize * 0.60f;
                const float centerX = (left + right) * 0.5f;
                const float centerY = (top + bottom) * 0.5f;
                const float thickness = logicalSize >= 12.0f ? 1.5f : 1.0f;
                if (glyphInfo->codepoint == 0x2190u || glyphInfo->codepoint == 0x2192u)
                {
                    const bool pointsLeft = glyphInfo->codepoint == 0x2190u;
                    const float tip = pointsLeft ? left : right;
                    const float tail = pointsLeft ? right : left;
                    const float head =
                            pointsLeft ? left + logicalSize * 0.28f : right - logicalSize * 0.28f;
                    // The lines belong to this run's position in the list, so what
                    // was batched so far is written before them.
                    quads.flush();
                    drawList.addLine(Vec2(tip, centerY), Vec2(tail, centerY), color, clip,
                            thickness);
                    drawList.addLine(Vec2(tip, centerY), Vec2(head, top), color, clip, thickness);
                    drawList.addLine(Vec2(tip, centerY), Vec2(head, bottom), color, clip,
                            thickness);
                }
                else
                {
                    const bool pointsUp = glyphInfo->codepoint == 0x2191u;
                    const float tip = pointsUp ? top : bottom;
                    const float tail = pointsUp ? bottom : top;
                    const float head =
                            pointsUp ? top + logicalSize * 0.28f : bottom - logicalSize * 0.28f;
                    quads.flush();
                    drawList.addLine(Vec2(centerX, tip), Vec2(centerX, tail), color, clip,
                            thickness);
                    drawList.addLine(Vec2(centerX, tip), Vec2(left, head), color, clip, thickness);
                    drawList.addLine(Vec2(centerX, tip), Vec2(right, head), color, clip, thickness);
                }
            }
            penX += glyphInfo->advance * scale;
        }
    }
    quads.flush();
    return true;
}

} // namespace ig
