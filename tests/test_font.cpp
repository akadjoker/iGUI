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

// Um atlas a partir dos bytes da aplicação: os mesmos glifos do embutido quando
// lhe damos os mesmos bytes, e um atlas inválido (não a fingir que está bom)
// quando os bytes não são uma fonte.
static void test_atlas_from_font_bytes()
{
    // Sem bytes não há atlas, e não se lê nada fora do buffer.
    const ig::FontRange ranges[] = {ig::FontRange(0x20u, 0x7Eu)};
    const uint8_t notAFont[64] = {0u};
    ig::FontAtlas garbage(ig::Span<const uint8_t>(notAFont, sizeof(notAFont)),
                          ig::Span<const ig::FontRange>(ranges, 1u), 512u, 256u, 16.0f);
    assert(!garbage.valid());
    ig::FontAtlas empty(ig::Span<const uint8_t>(), ig::Span<const ig::FontRange>(ranges, 1u),
                        512u, 256u, 16.0f);
    assert(!empty.valid());
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

// A fonte embutida, para uma segunda face sair dos mesmos bytes (a API pública
// só a leva pelas mãos do construtor do atlas; o teste lê-a daqui).
namespace ig
{
namespace detail
{
extern const uint8_t DefaultFontData[];
extern const uint32_t DefaultFontDataSize;
} // namespace detail
} // namespace ig

// Mais do que uma face no mesmo atlas: o que o loadFont acrescenta (os glifos de
// cada uma ficam no mesmo atlas, com as métricas e os tamanhos delas) e o que um
// id sem face faz.
static void test_multiple_faces()
{
    ig::FontAtlas atlas(ig::defaultFontRanges(), 1024u, 512u, 13.0f);
    assert(atlas.valid() && atlas.faceCount() == 1u);
    assert(atlas.defaultFont() == ig::FontId(1u));
    const ig::FontId small = atlas.defaultFont();
    const float smallAscent = atlas.ascent(small);
    const float smallLine = atlas.lineHeight(small);

    const ig::Span<const uint8_t> bytes(ig::detail::DefaultFontData,
                                        ig::detail::DefaultFontDataSize);
    const ig::FontId big = atlas.loadFont(bytes, 32.0f);
    assert(big == ig::FontId(2u));
    assert(atlas.faceCount() == 2u);
    assert(atlas.valid());

    // Cada face tem as métricas dela no tamanho em que foi cozida.
    assert(atlas.bakedSize(small) == 13.0f && atlas.bakedSize(big) == 32.0f);
    assert(atlas.ascent(big) > smallAscent);
    assert(atlas.lineHeight(big) > smallLine);
    assert(atlas.lineHeight(big) > atlas.ascent(big));
    // As sem argumento são as da primeira face.
    assert(atlas.bakedSize() == 13.0f && atlas.ascent() == smallAscent);
    assert(atlas.lineHeight() == smallLine);

    // Os glifos das duas estão no mesmo atlas (a mesma textura) e são maiores na
    // face cozida maior.
    const ig::FontGlyph *smallA = atlas.glyph(small, static_cast<uint32_t>('A'));
    const ig::FontGlyph *bigA = atlas.glyph(big, static_cast<uint32_t>('A'));
    assert(smallA && bigA);
    assert(bigA->width > smallA->width && bigA->height > smallA->height);
    assert(bigA->advance > smallA->advance);
    assert(smallA->u1 <= 1.0f && bigA->v1 <= 1.0f);

    // E mede-se pelo tamanho de cada uma.
    const ig::TextMetrics smallText = atlas.measureText(small, "Hello", 13.0f);
    const ig::TextMetrics bigText = atlas.measureText(big, "Hello", 32.0f);
    assert(smallText.width > 0.0f && bigText.width > smallText.width);
    assert(bigText.height > smallText.height);

    // Um id que não é de face nenhuma não dá glifos nem medidas - e não se
    // confunde com a primeira face.
    const ig::FontId unknown(9u);
    assert(atlas.glyph(unknown, static_cast<uint32_t>('A')) == nullptr);
    assert(atlas.measureText(unknown, "Hello", 13.0f).width == 0.0f);
    assert(atlas.bakedSize(unknown) == 0.0f && atlas.lineHeight(unknown) == 0.0f);
    assert(atlas.bakedSize(ig::FontId(0u)) == 0.0f);

    // Bytes que não são uma fonte não entram no atlas.
    const uint8_t notAFont[64] = {0u};
    assert(atlas.loadFont(ig::Span<const uint8_t>(notAFont, sizeof(notAFont)), 20.0f) ==
           ig::FontId(0u));
    assert(atlas.faceCount() == 2u);
    assert(atlas.valid());

    // Uma fonte a sério pedida com um intervalo que ela não tem não entra como uma
    // face sem glifos: o id que ela teria não pode apontar para nada, e o que já
    // lá estava fica como estava (as uvs das outras foram calculadas sobre a
    // imagem que esta podia ter mexido).
    const ig::FontRange missingRanges[] = {ig::FontRange(0x1F300u, 0x1F5FFu)};
    assert(atlas.loadFont(bytes, 20.0f,
                          ig::Span<const ig::FontRange>(missingRanges, 1u)) ==
           ig::FontId(0u));
    assert(atlas.faceCount() == 2u);
    assert(atlas.valid());
    assert(atlas.glyph(big, static_cast<uint32_t>('A')));
    assert(atlas.measureText(small, "Hello", 13.0f).width == smallText.width);

    // Um ficheiro que só tem a assinatura e uma tabela 'glyf' (sem cmap, head nem
    // hhea) não chega ao stb, que lê essas tabelas sem as validar: era o que dava
    // o segfault, e é isto que o loadFont peneira antes de lhe tocar.
    uint8_t fakeFont[32] = {0u};
    fakeFont[1] = 0x01u; // sfnt: TrueType
    fakeFont[5] = 0x01u; // uma tabela
    fakeFont[12] = 'g';
    fakeFont[13] = 'l';
    fakeFont[14] = 'y';
    fakeFont[15] = 'f';
    fakeFont[23] = 28u; // offset da tabela (20-23, big endian) e
    fakeFont[27] = 4u;  // comprimento (24-27): 28+4 = 32, cabe nos bytes
    assert(atlas.loadFont(ig::Span<const uint8_t>(fakeFont, sizeof(fakeFont)), 20.0f) ==
           ig::FontId(0u));
    assert(atlas.faceCount() == 2u);
    assert(atlas.valid());

    // As duas faces desenham pelo mesmo atlas.
    atlas.setTexture(ig::TextureId(3u));
    ig::DrawList drawList;
    assert(atlas.appendText(drawList, small, "A", ig::Vec2(0.0f, 0.0f), 13.0f, ig::Color(),
                            ig::Rect(0.0f, 0.0f, 100.0f, 100.0f)));
    const ig::DrawData smallData = drawList.data(ig::Vec2(100.0f, 100.0f), 1.0f);
    ig::DrawList bigDrawList;
    assert(atlas.appendText(bigDrawList, big, "A", ig::Vec2(0.0f, 0.0f), 32.0f, ig::Color(),
                            ig::Rect(0.0f, 0.0f, 100.0f, 100.0f)));
    const ig::DrawData bigData = bigDrawList.data(ig::Vec2(100.0f, 100.0f), 1.0f);
    assert(smallData.vertices.size() == bigData.vertices.size());
    assert(bigData.vertices.size() > 0u);
    float smallTop = 0.0f;
    float smallBottom = 0.0f;
    float bigTop = 0.0f;
    float bigBottom = 0.0f;
    for (ig::Span<const ig::DrawVertex>::size_type i = 0u; i < smallData.vertices.size(); ++i)
    {
        const float smallY = smallData.vertices[i].position.y;
        const float bigY = bigData.vertices[i].position.y;
        if (i == 0u || smallY < smallTop)
            smallTop = smallY;
        if (i == 0u || smallY > smallBottom)
            smallBottom = smallY;
        if (i == 0u || bigY < bigTop)
            bigTop = bigY;
        if (i == 0u || bigY > bigBottom)
            bigBottom = bigY;
    }
    // O mesmo glifo desenhado ao tamanho de cada face: a face grande sai mais alta.
    assert(bigBottom - bigTop > smallBottom - smallTop);
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
        const float clipRight = clip.x + clip.width;
        const float clipBottom = clip.y + clip.height;
        size_t visible = 0u;
        // Glyph quads are 4 vertices each, emitted in order. What a clipped run
        // keeps for a glyph is the glyph itself or, when the clip cuts it, the
        // part of it inside the clip: the geometry is trimmed to the clip on the
        // CPU (see DrawList::QuadBatch::add), so the quad that comes out is the
        // intersection with the clip and not the whole glyph.
        for (size_t v = 0; v + 3u < full.vertices.size(); v += 4u)
        {
            const ig::Vec2 &a = full.vertices[v].position;
            const ig::Vec2 &b = full.vertices[v + 2u].position;
            const float expectedLeft = a.x > clip.x ? a.x : clip.x;
            const float expectedTop = a.y > clip.y ? a.y : clip.y;
            const float expectedRight = b.x < clipRight ? b.x : clipRight;
            const float expectedBottom = b.y < clipBottom ? b.y : clipBottom;
            if (expectedRight <= expectedLeft || expectedBottom <= expectedTop)
                continue; // o clip não deixa nada deste glifo
            ++visible;
            bool found = false;
            for (size_t k = 0; k + 3u < kept.vertices.size() && !found; k += 4u)
                found = kept.vertices[k].position.x == expectedLeft &&
                        kept.vertices[k].position.y == expectedTop &&
                        kept.vertices[k + 2u].position.x == expectedRight &&
                        kept.vertices[k + 2u].position.y == expectedBottom;
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
    test_atlas_from_font_bytes();
    test_custom_ranges_and_atlas_size();
    test_high_resolution_atlas();
    test_multiple_faces();
    test_append_text_skips_only_invisible_lines();
    return 0;
}
