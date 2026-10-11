#pragma once

#include <stdint.h>

#include <ct/vector.hpp>

#include "Backend.hpp"

struct stbtt_pack_context;

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

    // ── Mais do que uma face no mesmo atlas ──────────────────────────────────
    // Uma face é um tipo de letra (ou o mesmo noutro tamanho) empacotado no mesmo
    // atlas: os glifos ficam ao lado uns dos outros e o Context só carrega no
    // FontId (ver pushFont). As bytes e os ranges são copiados, por isso podem ser
    // largados depois da chamada. O tamanho de fabrico e os ranges são os desta
    // face (os ranges vazios são os do atlas).
    //
    // O atlas é reconstruído (todas as faces, do zero) para caber a nova: quem já
    // tem a textura tem de a voltar a subir com pixels() depois disto - é o que a
    // aplicação faz uma vez, ao carregar as fontes, antes de criar a textura.
    FontId loadFont(Span<const uint8_t> fontData, float bakedSize = 0.0f,
                    Span<const FontRange> ranges = Span<const FontRange>());
    uint32_t faceCount() const;
    // As métricas de uma face (as sem argumento são as da primeira).
    float bakedSize(FontId font) const override;
    float lineHeight(FontId font) const;
    float ascent(FontId font) const;

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
    // Fixed-size, per-face lookup cache. Indices remain valid when copied.
    struct GlyphCacheEntry {
        uint32_t codepoint = 0;
        int index = -1;
        bool occupied = false;
    };
    // Uma face empacotada: as bytes ficam guardadas porque o atlas é reconstruído
    // inteiro sempre que uma face nova entra (o empacotador do stb não tem estado
    // que se guarde entre chamadas).
    struct Face
    {
        ct::Vector<uint8_t> data;
        // Os ranges com que esta face foi pedida (uma cópia: os do chamador podem
        // ser temporários), e os glifos que deles saíram.
        ct::Vector<FontRange> ranges;
        ct::Vector<FontGlyph> glyphs;
        float ascent;
        float descent;
        float lineHeight;
        float bakedSize;
        // codepoint -> index into glyphs for every codepoint below lookupLimit
        // (one past the highest baked codepoint, at most U+FFFF): a text run does
        // one load per glyph instead of a search. A missing codepoint maps to the
        // replacement glyph. Codepoints at or above the limit use the cache.
        ct::Vector<uint16_t> lookup;
        uint32_t lookupLimit;
        mutable GlyphCacheEntry glyphCache[256];

        Face()
            : data(), ranges(), glyphs(), ascent(0.0f), descent(0.0f), lineHeight(0.0f),
              bakedSize(0.0f), lookup(), lookupLimit(0u), glyphCache() {}
    };

    ct::Vector<uint8_t> pixels_;
    ct::Vector<Face> faces_;
    Span<const FontRange> ranges_;
    uint32_t width_;
    uint32_t height_;
    TextureId texture_;
    bool valid_;
    // O índice da face de um FontId (0 se o id não for de nenhuma).
    uint32_t faceIndex(FontId font) const;
    const FontGlyph *lookupGlyph(const Face &face, uint32_t codepoint) const;
    // Common body of the constructors and of loadFont: packs every face into the
    // atlas the members describe (the pixels are cleared first).
    void rebuild();
    // Empacota uma face no contexto que o rebuild abriu (é o mesmo contexto para
    // todas: os glifos de uma ficam ao lado dos da outra). Devolve false se a face
    // não deu glifos nenhuns.
    bool packFace(Face &face, stbtt_pack_context &pack);
};

// Lê um codepoint UTF-8. Para uma sequência inválida devolve U+FFFD e avança um byte.
bool decodeUtf8(StringView text, StringView::size_type &offset, uint32_t &codepoint);

} // namespace ig
