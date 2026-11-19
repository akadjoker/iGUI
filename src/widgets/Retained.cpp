#include "pch.hpp"

#include "DejaVuSans_embedded.h"
#include "stb_truetype.h"

namespace ig
{
namespace retained
{

struct Context
{
    IO io;
    DrawList drawList;      // current / new-stage list
    DrawList transDrawList; // old-stage list (valid during transitions)
    DrawList overlayList;   // always rendered last, identity camera
    DrawData drawData;
};

namespace
{

Context* gCurrentContext = nullptr;
constexpr float Pi = 3.14159265358979323846f;

Context& current()
{
    if (!gCurrentContext) gCurrentContext = CreateContext();
    return *gCurrentContext;
}

int saneSegments(int segments)
{
    return std::max(3, segments);
}

int circleSegments(float radius, int requested)
{
    // enough segments that no chord deviates more than a tenth of a pixel
    float r = std::max(radius, 1.0f);
    float a = std::acos(std::max(-1.0f, 1.0f - 0.1f / r));
    int fromRadius = static_cast<int>(std::ceil(3.14159265f / std::max(a, 0.001f)));
    return std::max(std::max(12, saneSegments(requested)), std::min(fromRadius, 512));
}

// Segments for one 90 degree corner of a rounded rectangle: the fewest for which no chord of the arc strays more than
// kCornerTolerance pixels from it (a chord of angle t on radius r is r * (1 - cos(t / 2)) away from the arc at its middle), and
// never below the three steps of the fixed 30 degree table this used to draw with. A corner of radius 4 is three segments (0,14 px
// off, invisible) where it used to be five or six: a fifth of the points of every button, check box and text box of a window.
// The caller's hint, a segment count, only makes the corner coarser than the error asks for when it is lower.
constexpr float kCornerTolerance = 0.15f;

// floorf without the call: the glyph positions of a text are rounded down to a pixel two at a time per glyph. The same value as
// floorf for what a window holds (a float that fits an int, 2^31 and more is left to floorf).
inline float floorOf(float v)
{
    if (!(v > -1.0e9f && v < 1.0e9f)) return floorf(v);
    const float truncated = static_cast<float>(static_cast<int>(v));
    return truncated > v ? truncated - 1.0f : truncated;
}
int cornerSegments(float radius, int requested)
{
    static float lastRadius = -1.0f; // (the radii of a window are a handful: the last answer is nearly always the one asked for)
    static int lastSegments = 3;
    int fromError = lastSegments;
    if (radius != lastRadius)
    {
        fromError = 3;
        if (radius > kCornerTolerance)
        {
            const float chordAngle = 2.0f * acosf(1.0f - kCornerTolerance / radius);
            fromError = chordAngle > 0.0245f ? static_cast<int>(ceilf(1.57079633f / chordAngle)) : 64; // (a radius of millions of pixels: 64)
            if (fromError < 3) fromError = 3;
        }
        lastRadius = radius;
        lastSegments = fromError;
    }
    if (requested > 0 && requested < fromError) return requested > 3 ? requested : 3;
    return fromError;
}

// Appends one corner arc, both endpoints included, in the same order and
// winding the fixed table produced.
//
// A rounded rectangle is four of these with the same number of segments (and its fill and its border make eight), and a window has
// hundreds of them with the same few radii: the cosines and sines of an arc are worked out once per (corner, segments) and kept.
// They are the same expression as before, float for float, so the points do not change.
constexpr int kArcCacheMax = 32; // (a corner of a radius up to about 58 px; a bigger one is worked out as before)
struct ArcCache
{
    float cosv[kArcCacheMax + 1];
    float sinv[kArcCacheMax + 1];
    bool done; // (no initialiser: the table is zero at the start and needs no constructor to run)
};

const float kCornerStart[4] = { -Pi * 0.5f, 0.0f, Pi * 0.5f, Pi }; // top right, bottom right, bottom left, top left

// The cosines and sines of the arc of a corner (0 to 3, in the order above) in `segments` steps, worked out once; null for a corner
// of more steps than the cache has.
const ArcCache* arcTable(int corner, int segments)
{
    if (segments > kArcCacheMax) return nullptr;
    static ArcCache cache[4][kArcCacheMax + 1];
    ArcCache& c = cache[corner][segments];
    if (!c.done)
    {
        const float step = (Pi * 0.5f) / static_cast<float>(segments);
        for (int i = 0; i <= segments; ++i)
        {
            const float angle = kCornerStart[corner] + step * static_cast<float>(i);
            c.cosv[i] = cosf(angle); // (the float cosine and sine, as before)
            c.sinv[i] = sinf(angle);
        }
        c.done = true;
    }
    return &c;
}

void pushCornerArc(ct::Vector<Vec2>& path, Vec2 centre, float radius, float startAngle,
        int segments)
{
    const float step = (Pi * 0.5f) / static_cast<float>(segments);
    int corner = -1;
    if (startAngle == -Pi * 0.5f) corner = 0;
    else if (startAngle == 0.0f) corner = 1;
    else if (startAngle == Pi * 0.5f) corner = 2;
    else if (startAngle == Pi) corner = 3;
    if (corner >= 0 && segments <= kArcCacheMax)
    {
        const ArcCache& c = *arcTable(corner, segments);
        for (int i = 0; i <= segments; ++i)
            path.push_back({ centre.x + c.cosv[i] * radius, centre.y + c.sinv[i] * radius });
        return;
    }
    for (int i = 0; i <= segments; ++i)
    {
        const float angle = startAngle + step * static_cast<float>(i);
        path.push_back(
                { centre.x + std::cos(angle) * radius, centre.y + std::sin(angle) * radius });
    }
}

bool intersectRects(const Rect& a, const Rect& b, Rect& out)
{
    float x0 = std::max(a.x, b.x);
    float y0 = std::max(a.y, b.y);
    float x1 = std::min(a.x + a.w, b.x + b.w);
    float y1 = std::min(a.y + a.h, b.y + b.h);

    if (x1 <= x0 || y1 <= y0)
    {
        out = { x0, y0, 0.0f, 0.0f };
        return false;
    }

    out = { x0, y0, x1 - x0, y1 - y0 };
    return true;
}

// ── Arc lookup table (ImGui PathArcToFast-style) ──────────────────
// 12 samples: index i → angle i * 30°.  Index 12 wraps to 0.
// Y is positive-downward (screen coords).
constexpr float arcFastX[13] = {
    1.00000f, 0.86603f, 0.50000f, 0.00000f, -0.50000f, -0.86603f, -1.00000f, -0.86603f, -0.50000f,
    0.00000f, 0.50000f, 0.86603f,
    1.00000f // [12] == [0], convenient for closed ranges
};
constexpr float arcFastY[13] = { 0.00000f, 0.50000f, 0.86603f, 1.00000f, 0.86603f, 0.50000f,
    0.00000f, -0.50000f, -0.86603f, -1.00000f, -0.86603f, -0.50000f, 0.00000f };

uint32_t decodeUtf8(const char*& text)
{
    const unsigned char* s = reinterpret_cast<const unsigned char*>(text);

    // ASCII
    if (s[0] < 0x80)
    {
        ++text;
        return s[0];
    }

    // 2-byte sequence
    if ((s[0] & 0xE0) == 0xC0 && s[1] != '\0' && (s[1] & 0xC0) == 0x80)
    {
        text += 2;
        return static_cast<uint32_t>(((s[0] & 0x1F) << 6) | (s[1] & 0x3F));
    }

    // 3-byte sequence
    if ((s[0] & 0xF0) == 0xE0 && s[1] != '\0' && (s[1] & 0xC0) == 0x80 && s[2] != '\0' &&
            (s[2] & 0xC0) == 0x80)
    {
        text += 3;
        return static_cast<uint32_t>(((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F));
    }

    // 4-byte sequence
    if ((s[0] & 0xF8) == 0xF0 && s[1] != '\0' && (s[1] & 0xC0) == 0x80 && s[2] != '\0' &&
            (s[2] & 0xC0) == 0x80 && s[3] != '\0' && (s[3] & 0xC0) == 0x80)
    {
        text += 4;
        return static_cast<uint32_t>(((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) |
                                     ((s[2] & 0x3F) << 6) | (s[3] & 0x3F));
    }

    ++text;
    return 0xFFFD;
}

} // namespace

// ── The faces and the dynamic page ────────────────────────────────────
//
// build() packs the default sizes once. Beyond that the atlas keeps each
// face's tables and rasterises single glyphs at the exact size a widget asks
// for into a page of its own, one row after another; the adapter uploads the
// page again whenever it changed. A page that fills up is cleared and its
// glyphs are baked again as they are needed (rare: it holds many sizes).

struct FontAtlas::Impl
{
    struct Face
    {
        const unsigned char* data = nullptr;
        stbtt_fontinfo info = {};
        bool ok = false;
    };
    ct::Vector<Face*> faces;
    ct::Vector<Font*> sized; // the fonts atHeight() made, kept for good

    bool enabled = false;
    float pixelScale = 1.0f;
    TextureHandle texture;
    int width = 1024;
    int height = 1024;
    // The dynamic page: a byte of coverage for each pixel (it is white wherever it is drawn: a quarter of the memory of an RGBA page),
    // and the RGBA form of it, made only when something asks for it (dynamicPixels) and kept until the page changes.
    ct::Vector<unsigned char> alpha;
    ct::Vector<unsigned char> rgbaCache;
    bool rgbaFresh = false;
    bool dirty = false;
    int penX = 1, penY = 1, rowHeight = 0;

    ~Impl()
    {
        for (Face* f: faces) delete f;
        for (Font* f: sized) delete f;
    }
    void clearFaces()
    {
        for (Face* f: faces) delete f;
        faces.clear();
    }
};

FontAtlas::FontAtlas()
        : impl_(new Impl)
{
}

FontAtlas::~FontAtlas()
{
    // The manager caches the font of the last paint to measure text with: it must not outlive us.
    if (WidgetApp* app = WidgetApp::existing()) app->forgetAtlas(*this);
    delete impl_;
}

// The page's pixels, empty, so that the adapter has something to make a
// texture from; the page is not in use until enableDynamic gets the texture.
bool FontAtlas::prepareDynamic()
{
    if (!impl_ || impl_->faces.empty()) return false;
    impl_->alpha.assign(static_cast<size_t>(impl_->width) * static_cast<size_t>(impl_->height), 0);
    impl_->rgbaCache.clear();
    impl_->rgbaFresh = false;
    impl_->penX = impl_->penY = 1;
    impl_->rowHeight = 0;
    impl_->enabled = false;
    impl_->dirty = false;
    // pixel (0, 0), where the baked page has its white texel (whitePixelUV), is white here too (nothing is packed at it: the pen
    // starts at 1, 1): shapes can be drawn with this page as well (DrawList::setSolidAlternate)
    impl_->alpha[0] = 255;
    return true;
}

bool FontAtlas::enableDynamic(TextureHandle texture)
{
    if (!impl_ || impl_->alpha.empty() || !texture) return false;
    impl_->texture = texture;
    impl_->enabled = true;
    return true;
}

void FontAtlas::disableDynamic()
{
    if (impl_) impl_->enabled = false;
}

bool FontAtlas::dynamicEnabled() const
{
    return impl_ && impl_->enabled;
}
TextureHandle FontAtlas::dynamicTexture() const
{
    return impl_ ? impl_->texture : TextureHandle();
}
int FontAtlas::dynamicWidth() const
{
    return impl_ ? impl_->width : 0;
}
int FontAtlas::dynamicHeight() const
{
    return impl_ ? impl_->height : 0;
}
const unsigned char* FontAtlas::dynamicPixels() const
{
    if (!impl_ || impl_->alpha.empty()) return nullptr;
    if (!impl_->rgbaFresh)
    {
        // white with the coverage in alpha, for whoever makes the texture from RGBA (an adapter with dynamicAlpha() does not ask)
        impl_->rgbaCache.resize(impl_->alpha.size() * 4u);
        for (size_t i = 0; i < impl_->alpha.size(); ++i)
        {
            unsigned char* px = &impl_->rgbaCache[i * 4u];
            px[0] = px[1] = px[2] = 255;
            px[3] = impl_->alpha[i];
        }
        impl_->rgbaFresh = true;
    }
    return impl_->rgbaCache.data();
}
const unsigned char* FontAtlas::dynamicAlpha() const
{
    return impl_ && !impl_->alpha.empty() ? impl_->alpha.data() : nullptr;
}
bool FontAtlas::dynamicDirty() const
{
    return impl_ && impl_->dirty;
}
void FontAtlas::clearDynamicDirty()
{
    if (impl_) impl_->dirty = false;
}

const Font* FontAtlas::fontAtHeight(int face, float pixelHeight)
{
    if (!impl_ || face < 0 || face >= static_cast<int>(impl_->faces.size()) ||
            !impl_->faces[static_cast<size_t>(face)]->ok)
        return nullptr;
    // Heights are kept to an eighth of a pixel, so 14.4 and 14.4001 are one font.
    const float h = std::floor(pixelHeight * 8.0f + 0.5f) / 8.0f;
    for (Font* f: impl_->sized)
        if (f->face_ == face && f->pixelHeight_ == h) return f;

    const Impl::Face& fc = *impl_->faces[static_cast<size_t>(face)];
    Font* f = new Font;
    f->texture_ = impl_->texture;
    f->atlas_ = this;
    f->face_ = face;
    f->dynamic_ = true;
    f->pixelHeight_ = h;
    f->pixelScale_ = impl_->pixelScale;
    const float scale = stbtt_ScaleForPixelHeight(&fc.info, h);
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(&fc.info, &ascent, &descent, &lineGap);
    f->lineHeight_ = static_cast<float>(ascent - descent + lineGap) * scale;
    f->ascender_ = static_cast<float>(ascent) * scale;
    impl_->sized.push_back(f);
    return f;
}

void FontAtlas::setPixelScale(float scale)
{
    if (!impl_) return;
    if (!(scale >= 0.25f)) scale = 1.0f;
    if (scale > 8.0f) scale = 8.0f;
    if (scale == impl_->pixelScale) return;
    impl_->pixelScale = scale;
    // every size is baked again at the new scale
    if (!impl_->alpha.empty())
    {
        memset(impl_->alpha.data(), 0, impl_->alpha.size());
        impl_->alpha[0] = 255; // (the white texel stays)
        impl_->rgbaFresh = false;
        impl_->penX = impl_->penY = 1;
        impl_->rowHeight = 0;
        impl_->dirty = true;
    }
    for (Font* f: impl_->sized)
    {
        f->pixelScale_ = scale;
        f->clearGlyphs();
        f->kern_.clear();
    }
}

float FontAtlas::pixelScale() const
{
    return impl_ ? impl_->pixelScale : 1.0f;
}

bool FontAtlas::bakeGlyph(const Font& font, uint32_t codepoint)
{
    if (!impl_ || !impl_->enabled || font.face_ < 0) return false;
    const Impl::Face& fc = *impl_->faces[static_cast<size_t>(font.face_)];
    const float ps = font.pixelScale_ > 0.0f ? font.pixelScale_ : 1.0f;
    const float scale = stbtt_ScaleForPixelHeight(&fc.info, font.pixelHeight_ * ps);

    int gi = stbtt_FindGlyphIndex(&fc.info, static_cast<int>(codepoint));
    if (gi == 0 && codepoint != '?')
    {
        // Not in the face: what was always drawn, a question mark.
        const FontGlyph* q = font.findGlyph('?');
        if (!q) return false;
        FontGlyph copy = *q;
        copy.codepoint = codepoint;
        font.storeGlyph(codepoint, copy);
        return true;
    }

    int adv = 0, lsb = 0;
    stbtt_GetGlyphHMetrics(&fc.info, gi, &adv, &lsb);
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetGlyphBitmapBox(&fc.info, gi, scale, scale, &x0, &y0, &x1, &y1);
    const int gw = x1 - x0, gh = y1 - y0;

    FontGlyph glyph;
    glyph.codepoint = codepoint;
    // the bitmap is in device pixels, the glyph is measured in units
    glyph.advanceX = static_cast<float>(adv) * scale / ps;
    glyph.offset = { static_cast<float>(x0) / ps, static_cast<float>(y0) / ps };
    glyph.size = { static_cast<float>(gw > 0 ? gw : 0) / ps, static_cast<float>(gh > 0 ? gh : 0) / ps };

    if (gw > 0 && gh > 0)
    {
        if (gw + 2 > impl_->width || gh + 2 > impl_->height) return false;
        if (impl_->penX + gw + 1 > impl_->width)
        {
            impl_->penX = 1;
            impl_->penY += impl_->rowHeight + 1;
            impl_->rowHeight = 0;
        }
        if (impl_->penY + gh + 1 > impl_->height)
        {
            // Full: start the page again; every size bakes its glyphs anew.
            memset(impl_->alpha.data(), 0, impl_->alpha.size());
            impl_->alpha[0] = 255; // (the white texel stays)
            impl_->rgbaFresh = false;
            impl_->penX = impl_->penY = 1;
            impl_->rowHeight = 0;
            for (Font* f: impl_->sized) f->clearGlyphs();
        }
        ct::Vector<unsigned char> alpha(static_cast<size_t>(gw) * static_cast<size_t>(gh), 0);
        stbtt_MakeGlyphBitmap(&fc.info, alpha.data(), gw, gh, gw, scale, scale, gi);
        impl_->rgbaFresh = false;
        for (int y = 0; y < gh; ++y)
            for (int x = 0; x < gw; ++x)
                impl_->alpha[static_cast<size_t>(impl_->penY + y) * static_cast<size_t>(impl_->width) + static_cast<size_t>(impl_->penX + x)] =
                        alpha[static_cast<size_t>(y) * static_cast<size_t>(gw) + static_cast<size_t>(x)];
        glyph.uv = { static_cast<float>(impl_->penX) / static_cast<float>(impl_->width),
            static_cast<float>(impl_->penY) / static_cast<float>(impl_->height),
            static_cast<float>(gw) / static_cast<float>(impl_->width),
            static_cast<float>(gh) / static_cast<float>(impl_->height) };
        impl_->penX += gw + 1;
        impl_->rowHeight = std::max(impl_->rowHeight, gh);
        impl_->dirty = true;
    }
    font.storeGlyph(codepoint, glyph);
    return true;
}

float FontAtlas::kerningOf(const Font& font, uint32_t left, uint32_t right) const
{
    if (!impl_ || font.face_ < 0 || font.face_ >= static_cast<int>(impl_->faces.size()))
        return 0.0f;
    const Impl::Face& fc = *impl_->faces[static_cast<size_t>(font.face_)];
    if (!fc.ok || font.pixelHeight_ <= 0.0f) return 0.0f;
    // the kerning is in units: the height the face is scaled by is in units too
    const float scale = stbtt_ScaleForPixelHeight(&fc.info, font.pixelHeight_);
    return static_cast<float>(stbtt_GetCodepointKernAdvance(&fc.info, static_cast<int>(left),
                   static_cast<int>(right))) *
           scale;
}

const Font* Font::atHeight(float pixelHeight) const
{
    if (!atlas_ || !atlas_->dynamicEnabled() || pixelHeight <= 0.0f) return this;
    if (dynamic_ && std::fabs(pixelHeight - pixelHeight_) < 0.0625f) return this;
    const Font* f = atlas_->fontAtHeight(face_, pixelHeight);
    return f ? f : this;
}

float Font::kerning(uint32_t left, uint32_t right) const
{
    // Only the fonts made by atHeight(): code that adds the baked font's
    // advances by hand (the code editor) must keep agreeing with the drawing.
    if (!dynamic_) return 0.0f;
    return pairKerning(left, right);
}

float Font::pairKerningFromFace(uint64_t key, uint32_t left, uint32_t right) const
{
    const float v = atlas_->kerningOf(*this, left, right);
    kern_[key] = v;
    return v;
}

bool FontAtlas::faceHasGlyph(int face, uint32_t codepoint) const
{
    if (!impl_ || face < 0 || face >= static_cast<int>(impl_->faces.size())) return false;
    const Impl::Face& fc = *impl_->faces[static_cast<size_t>(face)];
    return fc.ok && stbtt_FindGlyphIndex(&fc.info, static_cast<int>(codepoint)) != 0;
}

bool Font::hasGlyph(uint32_t codepoint) const
{
    if (atlas_ && face_ >= 0) return atlas_->faceHasGlyph(face_, codepoint);
    return glyphs_.find(codepoint) != nullptr;
}

const FontGlyph* Font::findGlyph(uint32_t codepoint) const
{
    if (codepoint < 128 && asciiHas_[codepoint]) return &ascii_[codepoint];
    const FontGlyph* glyph = glyphs_.find(codepoint);
    if (glyph) return glyph;

    if (dynamic_ && atlas_ && atlas_->bakeGlyph(*this, codepoint))
    {
        glyph = glyphs_.find(codepoint);
        if (glyph) return glyph;
    }

    glyph = glyphs_.find('?');
    if (glyph) return glyph;

    return nullptr;
}

// ── Multi-font: addFont / build ──────────────────────────────────────

int FontAtlas::addFont(const FontConfig& cfg)
{
    pending_.push_back(cfg);
    return static_cast<int>(pending_.size()) - 1;
}

int FontAtlas::addFontDefault(float pixelHeight)
{
    FontConfig cfg;
    cfg.ttfData = nullptr; // signals "use embedded DejaVuSans"
    cfg.ttfSize = 0;
    cfg.pixelHeight = pixelHeight;
    return addFont(cfg);
}

bool FontAtlas::build()
{
    if (pending_.empty()) return false;

    bool anyBaked = false;
    for (const auto& cfg: pending_) anyBaked = anyBaked || cfg.baked;
    // only the white pixel is needed when nothing is baked
    width_ = anyBaked ? 2048 : 8;
    height_ = anyBaked ? 1024 : 8;
    ct::Vector<unsigned char> alpha(static_cast<size_t>(width_ * height_), 0);

    stbtt_pack_context pack = {};
    if (!stbtt_PackBegin(&pack, alpha.data(), width_, height_, 0, 1, nullptr)) return false;

    stbtt_PackSetOversampling(&pack, 2, 2);
    stbtt_PackSetSkipMissingCodepoints(&pack, 1);

    // ── Codepoint ranges (shared by all fonts) ───────────────────────
    struct Range
    {
        int first;
        int count;
    };
    const Range kRanges[] = {
        { 0x0020, 224 },
        { 0x2000, 112 },
        { 0x2100, 80 },
        { 0x2190, 112 },
        { 0x2200, 256 },
        { 0x2300, 256 },
        { 0x2500, 128 },
        { 0x2580, 32 },
        { 0x25A0, 96 },
        { 0x2600, 256 },
        { 0x2700, 192 },
    };
    constexpr int kRangeCount = sizeof(kRanges) / sizeof(kRanges[0]);

    // Per-font packed char data
    struct FontPack
    {
        const unsigned char* data;
        int dataSize;
        float pixelHeight;
        int faceIndex;
        bool baked;
        ct::Vector<ct::Vector<stbtt_packedchar>> rangePacked; // [range][char]
    };
    ct::Vector<FontPack> packs(pending_.size());

    // Build stb ranges for all fonts
    ct::Vector<stbtt_pack_range> allRanges;
    for (size_t fi = 0; fi < pending_.size(); ++fi)
    {
        auto& cfg = pending_[fi];
        auto& fp = packs[fi];
        fp.data = cfg.ttfData ? cfg.ttfData : DejaVuSans_ttf_data;
        fp.dataSize = cfg.ttfData ? cfg.ttfSize : static_cast<int>(DejaVuSans_ttf_size);
        fp.pixelHeight = cfg.pixelHeight;
        fp.faceIndex = cfg.faceIndex > 0 ? cfg.faceIndex : 0;
        fp.baked = cfg.baked;
        // a face a collection does not have: nothing is packed and the face stays unusable
        if (stbtt_GetFontOffsetForIndex(fp.data, fp.faceIndex) < 0) fp.baked = false;
        if (!fp.baked) continue;
        fp.rangePacked.resize(kRangeCount);
        for (int ri = 0; ri < kRangeCount; ++ri)
        {
            fp.rangePacked[ri].resize(static_cast<size_t>(kRanges[ri].count));
            stbtt_pack_range pr{};
            pr.font_size = fp.pixelHeight;
            pr.first_unicode_codepoint_in_range = kRanges[ri].first;
            pr.num_chars = kRanges[ri].count;
            pr.chardata_for_range = fp.rangePacked[ri].data();
            pr.array_of_unicode_codepoints = nullptr;
            allRanges.push_back(pr);
        }
        // For multiple fonts from different TTFs, pack each separately
    }

    // Pack fonts one by one (stb_truetype packs per-font)
    size_t rangeIdx = 0;
    for (size_t fi = 0; fi < packs.size(); ++fi)
    {
        if (!packs[fi].baked) continue;
        stbtt_PackFontRanges(&pack, packs[fi].data, packs[fi].faceIndex, &allRanges[rangeIdx],
                kRangeCount);
        rangeIdx += kRangeCount;
    }

    stbtt_PackEnd(&pack);

    // Crop the page to the rows the packer used (rounded up to a power of two, so that v = y / height and the sampler's
    // v * height stay exact and every glyph samples the same texels as in the full page). A face or two use a small part of the
    // 2048x1024 page: the rest was a megabyte of zeros per 128 rows held twice (here and in the backend's texture).
    if (anyBaked)
    {
        int used = 1;
        for (const auto& fp: packs)
            for (const auto& range: fp.rangePacked)
                for (const stbtt_packedchar& pc: range)
                    if (pc.y1 > used) used = pc.y1;
        int cropped = 64;
        while (cropped < used + 1 && cropped < height_) cropped *= 2;
        if (cropped < height_) height_ = cropped;
    }

    // Convert alpha → RGBA
    pixels_.assign(static_cast<size_t>(width_ * height_ * 4), 0);
    for (int i = 0; i < width_ * height_; ++i)
    {
        size_t dst = static_cast<size_t>(i) * 4;
        pixels_[dst + 0] = 255;
        pixels_[dst + 1] = 255;
        pixels_[dst + 2] = 255;
        pixels_[dst + 3] = alpha[static_cast<size_t>(i)];
    }

    // White pixel at [0,0]
    pixels_[0] = pixels_[1] = pixels_[2] = pixels_[3] = 255;
    whitePixelUV_ = { 0.5f / static_cast<float>(width_), 0.5f / static_cast<float>(height_) };

    // Build Font objects. The fonts of an earlier build go away: the manager must not keep them.
    if (WidgetApp* app = WidgetApp::existing()) app->forgetAtlas(*this);
    fonts_.clear();
    fonts_.resize(packs.size());
    for (Font* f: impl_->sized) delete f;
    impl_->sized.clear();
    impl_->clearFaces();
    impl_->enabled = false;
    for (size_t fi = 0; fi < packs.size(); ++fi)
    {
        auto& fp = packs[fi];
        auto& fnt = fonts_[fi];
        fnt.texture_ = texture_;
        fnt.atlas_ = this;
        fnt.face_ = static_cast<int>(fi);
        fnt.pixelHeight_ = fp.pixelHeight;

        Impl::Face* face = new Impl::Face;
        face->data = fp.data;
        impl_->faces.push_back(face);
        stbtt_fontinfo& info = face->info;
        const int offset = stbtt_GetFontOffsetForIndex(fp.data, fp.faceIndex);
        if (offset < 0 || !stbtt_InitFont(&info, fp.data, offset)) continue;
        face->ok = true;

        float scale = stbtt_ScaleForPixelHeight(&info, fp.pixelHeight);
        int ascent = 0, descent = 0, lineGap = 0;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
        fnt.lineHeight_ = static_cast<float>(ascent - descent + lineGap) * scale;
        fnt.ascender_ = static_cast<float>(ascent) * scale;

        for (int ri = 0; ri < kRangeCount && fp.baked; ++ri)
        {
            for (int ci = 0; ci < kRanges[ri].count; ++ci)
            {
                const stbtt_packedchar& pc = fp.rangePacked[ri][static_cast<size_t>(ci)];
                uint32_t cp = static_cast<uint32_t>(kRanges[ri].first + ci);
                if (pc.xadvance <= 0.0f && cp != ' ') continue;
                FontGlyph glyph;
                glyph.codepoint = cp;
                glyph.uv = { static_cast<float>(pc.x0) / static_cast<float>(width_),
                    static_cast<float>(pc.y0) / static_cast<float>(height_),
                    static_cast<float>(pc.x1 - pc.x0) / static_cast<float>(width_),
                    static_cast<float>(pc.y1 - pc.y0) / static_cast<float>(height_) };
                glyph.size = { pc.xoff2 - pc.xoff, pc.yoff2 - pc.yoff };
                glyph.offset = { pc.xoff, pc.yoff };
                glyph.advanceX = pc.xadvance;
                fnt.storeGlyph(cp, glyph);
            }
        }
    }

    pending_.clear();
    if (fonts_.empty()) return false;
    if (!anyBaked) return true;
    for (const auto& f: fonts_)
        if (!f.glyphs_.empty()) return true;
    return false;
}

// ── Legacy single-font API ───────────────────────────────────────────

bool FontAtlas::buildDefault()
{
    pending_.clear();
    fonts_.clear();
    addFontDefault(16.0f);
    return build();
}

bool FontAtlas::buildFromTTFMemory(const unsigned char* data, int dataSize, float pixelHeight)
{
    if (!data || dataSize <= 0 || pixelHeight <= 0.0f) return false;
    pending_.clear();
    fonts_.clear();
    FontConfig cfg;
    cfg.ttfData = data;
    cfg.ttfSize = dataSize;
    cfg.pixelHeight = pixelHeight;
    addFont(cfg);
    return build();
}

void GetDefaultFont(const unsigned char** data, int* size)
{
    *data = DejaVuSans_ttf_data;
    *size = static_cast<int>(DejaVuSans_ttf_size);
}

void FontAtlas::releasePixels()
{
    ct::Vector<unsigned char> none;
    pixels_.swap(none);
}

void FontAtlas::setTexture(TextureHandle texture)
{
    texture_ = texture;
    for (auto& f: fonts_) f.texture_ = texture;
}

void DrawList::clear()
{
    clipStack_.clear();
    transformStack_.clear();
    transform_[0] = 1.0f;
    transform_[1] = 0.0f;
    transform_[2] = 0.0f;
    transform_[3] = 1.0f;
    transform_[4] = 0.0f;
    transform_[5] = 0.0f;
    path_.clear();
    vertices_.clear();
    indices_.clear();
    commands_.clear();
    pushClipCount_ = 0;
    newVertices_ = 0;
    lastStart_ = 0;
    lastBoundsValid_ = false;
    lastSolidOnly_ = false;
    mergedClipValid_ = false;
}

void DrawList::pushTransform(const float matrix[6])
{
    TransformEntry saved;
    for (int i = 0; i < 6; ++i) saved.m[i] = transform_[i];
    transformStack_.push_back(saved);
    const float* o = saved.m;
    // new = current * given
    transform_[0] = o[0] * matrix[0] + o[2] * matrix[1];
    transform_[1] = o[1] * matrix[0] + o[3] * matrix[1];
    transform_[2] = o[0] * matrix[2] + o[2] * matrix[3];
    transform_[3] = o[1] * matrix[2] + o[3] * matrix[3];
    transform_[4] = o[0] * matrix[4] + o[2] * matrix[5] + o[4];
    transform_[5] = o[1] * matrix[4] + o[3] * matrix[5] + o[5];
}

void DrawList::popTransform()
{
    if (transformStack_.empty()) return;
    const TransformEntry& saved = transformStack_.back();
    for (int i = 0; i < 6; ++i) transform_[i] = saved.m[i];
    transformStack_.pop_back();
}

void DrawList::pushClip(const Rect& source)
{
    Rect rect = source;
    if (!transformStack_.empty())
    {
        const Vec2 p0 = mapPoint(source.x, source.y);
        const Vec2 p1 = mapPoint(source.x + source.w, source.y);
        const Vec2 p2 = mapPoint(source.x + source.w, source.y + source.h);
        const Vec2 p3 = mapPoint(source.x, source.y + source.h);
        float x0 = std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x));
        float x1 = std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x));
        float y0 = std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y));
        float y1 = std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y));
        rect = { x0, y0, x1 - x0, y1 - y0 };
    }
    pushClipScreen(rect);
}

void DrawList::pushClipScreen(const Rect& rect)
{
    ClipEntry entry;
    entry.rect = rect;
    if (!clipStack_.empty())
    {
        const ClipEntry& parent = clipStack_.back();
        intersectRects(parent.rect, rect, entry.rect);
        entry.roundCount = parent.roundCount;
        for (int i = 0; i < parent.roundCount; ++i) entry.rounds[i] = parent.rounds[i];
    }
    clipStack_.push_back(entry);
    ++pushClipCount_;
}

void DrawList::pushClipRoundedRect(const Rect& source, const float radii[4])
{
    Rect rect = source;
    float scale = 1.0f;
    bool keepRound = true;
    if (!transformStack_.empty())
    {
        // The rounded part survives only a translation / scale; the rectangle always scissors.
        keepRound = transform_[1] == 0.0f && transform_[2] == 0.0f;
        scale = std::min(std::fabs(transform_[0]), std::fabs(transform_[3]));
        const Vec2 p0 = mapPoint(source.x, source.y);
        const Vec2 p2 = mapPoint(source.x + source.w, source.y + source.h);
        rect = { std::min(p0.x, p2.x), std::min(p0.y, p2.y), std::fabs(p2.x - p0.x),
            std::fabs(p2.y - p0.y) };
    }
    RoundClip round;
    round.rect = rect;
    float r[4];
    bool any = false;
    for (int i = 0; i < 4; ++i)
    {
        r[i] = radii[i] > 0.0f ? radii[i] * scale : 0.0f;
        any = any || r[i] > 0.0f;
    }
    if (!keepRound) any = false;
    pushClipScreen(rect);
    if (!any || rect.w <= 0.0f || rect.h <= 0.0f) return;

    // CSS: when two radii on a side add up to more than the side, all shrink by the same factor.
    float factor = 1.0f;
    const float sums[4] = { r[0] + r[1], r[1] + r[2], r[3] + r[2], r[0] + r[3] };
    const float sides[4] = { rect.w, rect.h, rect.w, rect.h };
    for (int i = 0; i < 4; ++i)
        if (sums[i] > sides[i] && sides[i] / sums[i] < factor) factor = sides[i] / sums[i];
    for (int i = 0; i < 4; ++i) round.radii[i] = r[i] * factor;

    ClipEntry& entry = clipStack_.back();
    // The same shape twice clips nothing more.
    for (int i = 0; i < entry.roundCount; ++i)
    {
        const RoundClip& old = entry.rounds[i];
        bool same = old.rect.x == round.rect.x && old.rect.y == round.rect.y &&
                    old.rect.w == round.rect.w && old.rect.h == round.rect.h;
        for (int c = 0; c < 4; ++c) same = same && old.radii[c] == round.radii[c];
        if (same) return;
    }
    if (entry.roundCount == DrawCmd::MaxRoundClips)
    {
        for (int i = 1; i < entry.roundCount; ++i) entry.rounds[i - 1] = entry.rounds[i];
        --entry.roundCount;
    }
    entry.rounds[entry.roundCount++] = round;
}

void DrawList::popClip()
{
    if (!clipStack_.empty()) clipStack_.pop_back();
}

void DrawList::setWhitePixel(TextureHandle atlas, Vec2 whiteUV)
{
    fontTexture_ = atlas;
    whiteUV_ = whiteUV;
    altTexture_ = TextureHandle();
}

// ── Path builder API ──────────────────────────────────────────────────────

void DrawList::pathLineTo(Vec2 p)
{
    path_.push_back(p);
}

void DrawList::pathClear()
{
    path_.clear();
}

void DrawList::pathArcToFast(Vec2 center, float radius, int aMinOf12, int aMaxOf12)
{
    if (radius < 0.5f)
    {
        path_.push_back(center);
        return;
    }
    path_.reserve(path_.size() + static_cast<size_t>(aMaxOf12 - aMinOf12 + 1));
    for (int i = aMinOf12; i <= aMaxOf12; ++i)
    {
        int k = i % 12;
        path_.push_back({ center.x + arcFastX[k] * radius, center.y + arcFastY[k] * radius });
    }
}

void DrawList::pathFillConvex(const Color& color)
{
    fillConvexPath(nullptr, color);
}

void DrawList::addConvexPolyFilledMultiColor(const Vec2* points, const Color* colors, int count)
{
    path_.clear();
    if (!points || !colors || count < 3) return;
    for (int i = 0; i < count; ++i) path_.push_back(points[i]);
    fillConvexPath(colors, colors[0]);
}

// Fills path_; `colors` (one per path point) or, when null, `color` for every point.
void DrawList::fillConvexPath(const Color* colors, const Color& color)
{
    int n = static_cast<int>(path_.size());
    if (n < 3)
    {
        path_.clear();
        return;
    }

    // Inner polygon plus a one pixel fringe fading to transparent, so the
    // edge is anti-aliased whatever the renderer does with the triangles.
    const float fringe = antiAliasing_ ? 1.0f : 0.0f;
    uint32_t idxCount = static_cast<uint32_t>((n - 2) * 3 + (fringe > 0.0f ? n * 6 : 0));

    uint32_t offset = static_cast<uint32_t>(indices_.size());
    uint32_t base = static_cast<uint32_t>(vertices_.size());
    auto colorAt = [&](int i) { return colors ? colors[i] : color; };

    if (fringe <= 0.0f)
    {
        DrawVertex* vp = appendVertices(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i)
        {
            const Vec2& p = path_[static_cast<size_t>(i)];
            const Vec2 q = mapPoint(p.x, p.y);
            *vp++ = { q.x, q.y, whiteUV_.x, whiteUV_.y, colorAt(i) };
        }
        uint32_t* ix = appendIndices(idxCount);
        for (uint32_t i = 2; i < static_cast<uint32_t>(n); ++i)
        {
            *ix++ = base;
            *ix++ = base + i - 1;
            *ix++ = base + i;
        }
        addCmd(offset, idxCount, fontTexture_, true);
        path_.clear();
        return;
    }

    // Winding decides which side is outside.
    float area = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const Vec2 &a = path_[static_cast<size_t>(i)], &b = path_[static_cast<size_t>((i + 1) % n)];
        area += a.x * b.y - b.x * a.y;
    }
    const float side = area >= 0.0f ? 1.0f : -1.0f;

    // The unit direction of every edge, once (the edge from point i to the next one is the "next" edge of i and the "previous" of i + 1).
    edgeScratch_.resize(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        const Vec2& p = path_[static_cast<size_t>(i)];
        const Vec2& next = path_[static_cast<size_t>(i + 1 == n ? 0 : i + 1)];
        float dx = next.x - p.x, dy = next.y - p.y;
        const float l = sqrtf(dx * dx + dy * dy);
        if (l > 0.0001f)
        {
            dx /= l;
            dy /= l;
        }
        edgeScratch_[static_cast<size_t>(i)] = { dx, dy, l };
    }

    DrawVertex* vp = appendVertices(static_cast<size_t>(n) * 2);
    for (int i = 0; i < n; ++i)
    {
        const Vec2& p = path_[static_cast<size_t>(i)];
        const EdgeDir& e1 = edgeScratch_[static_cast<size_t>(i == 0 ? n - 1 : i - 1)];
        const EdgeDir& e2 = edgeScratch_[static_cast<size_t>(i)];
        const float dx1 = e1.x, dy1 = e1.y, dx2 = e2.x, dy2 = e2.y;
        float nx = (-dy1 - dy2) * side, ny = (dx1 + dx2) * side;
        float d2 = nx * nx + ny * ny;
        if (d2 < 0.000001f)
        {
            nx = -dy1 * side;
            ny = dx1 * side;
            d2 = 1.0f;
        }
        float inv = 1.0f / d2;
        // The mitre at an acute corner would run far past the vertex (a thin pie slice grew a
        // hair at its tip), so it is capped at twice the fringe: the join loses a little
        // anti-aliasing, not its shape. (1 / d2 is past 2 / sqrt(d2) only below d2 = 0.25: above 0.3 there is nothing to cap,
        // and the square root is not taken.)
        if (d2 < 0.3f)
        {
            const float maxInv = 2.0f / sqrtf(d2);
            if (inv > maxInv) inv = maxInv;
        }
        nx *= inv * 2.0f * fringe * 0.5f;
        ny *= inv * 2.0f * fringe * 0.5f;
        Color inner = colorAt(i);
        Color outer = inner;
        outer.a = 0;
        const Vec2 q0 = mapPoint(p.x - nx, p.y - ny);
        const Vec2 q1 = mapPoint(p.x + nx, p.y + ny);
        *vp++ = { q0.x, q0.y, whiteUV_.x, whiteUV_.y, inner };
        *vp++ = { q1.x, q1.y, whiteUV_.x, whiteUV_.y, outer };
    }

    uint32_t* ix = appendIndices(idxCount);
    for (uint32_t i = 2; i < static_cast<uint32_t>(n); ++i)
    {
        *ix++ = base;
        *ix++ = base + (i - 1) * 2;
        *ix++ = base + i * 2;
    }
    for (uint32_t i = 0; i < static_cast<uint32_t>(n); ++i)
    {
        uint32_t j = (i + 1) % static_cast<uint32_t>(n);
        *ix++ = base + i * 2;
        *ix++ = base + j * 2;
        *ix++ = base + j * 2 + 1;
        *ix++ = base + i * 2;
        *ix++ = base + j * 2 + 1;
        *ix++ = base + i * 2 + 1;
    }

    addCmd(offset, idxCount, fontTexture_, true);
    path_.clear();
}

void DrawList::pathStroke(const Color& color, float thickness, bool closed)
{
    int n = static_cast<int>(path_.size());
    if (n < 2)
    {
        path_.clear();
        return;
    }
    thickness = std::max(1.0f, thickness);

    // Miter-joined band of the full thickness, plus on each side a one
    // pixel fringe fading to transparent (anti-aliased edges).
    const bool aa = antiAliasing_;
    const float fringe = aa ? 1.0f : 0.0f;
    // One pixel of core, the rule the immediate-mode lines use: thinning a
    // hairline all the way to the fringe leaves a two pixel blur instead of
    // a crisp rule, which is what every 1px border and icon stroke gets.
    const float core = aa ? std::max(1.0f, thickness - fringe) : thickness;
    int segs = closed ? n : n - 1;
    const int perPoint = aa ? 4 : 2;
    uint32_t vtxCount = static_cast<uint32_t>(n * perPoint);
    uint32_t idxCount = static_cast<uint32_t>(segs * (aa ? 18 : 6));

    uint32_t offset = static_cast<uint32_t>(indices_.size());
    uint32_t base = static_cast<uint32_t>(vertices_.size());
    Color transparent = color;
    transparent.a = 0;
    DrawVertex* vp = appendVertices(vtxCount);
    auto addMapped = [&](float x, float y, const Color& c) {
        const Vec2 q = mapPoint(x, y);
        *vp++ = { q.x, q.y, whiteUV_.x, whiteUV_.y, c };
    };

    // The unit direction of every segment, once: segment i runs from point i to the next (the first of a closed path comes after the last).
    edgeScratch_.resize(static_cast<size_t>(segs));
    for (int i = 0; i < segs; ++i)
    {
        const Vec2& p = path_[static_cast<size_t>(i)];
        const Vec2& next = path_[static_cast<size_t>(i + 1 == n ? 0 : i + 1)];
        float dx = next.x - p.x, dy = next.y - p.y;
        const float l = sqrtf(dx * dx + dy * dy);
        if (l > 0.0001f)
        {
            dx /= l;
            dy /= l;
        }
        edgeScratch_[static_cast<size_t>(i)] = { dx, dy, l };
    }

    for (int i = 0; i < n; ++i)
    {
        const Vec2& p = path_[static_cast<size_t>(i)];
        // the segment that ends at this point and the one that starts at it; an end of an open path has only one of them (the other
        // is a zero vector of length zero, as the point minus itself was)
        const EdgeDir zero;
        const EdgeDir& prevEdge = closed ? edgeScratch_[static_cast<size_t>(i == 0 ? n - 1 : i - 1)] : (i == 0 ? zero : edgeScratch_[static_cast<size_t>(i - 1)]);
        const EdgeDir& nextEdge = (closed || i < n - 1) ? edgeScratch_[static_cast<size_t>(i)] : zero;
        const float dx1 = prevEdge.x, dy1 = prevEdge.y, l1 = prevEdge.len;
        const float dx2 = nextEdge.x, dy2 = nextEdge.y, l2 = nextEdge.len;
        float nx1 = -dy1, ny1 = dx1;
        float nx2 = -dy2, ny2 = dx2;
        // An end of an open path (or a repeated point) has no segment on one side: its normal is the other's. With a
        // zero there the miter offset came out twice as long, and the first and the last segment of every open
        // polyline were drawn twice as thick at their end (a tapering band, as if each were drawn twice).
        if (!(l1 > 0.0001f))
        {
            nx1 = nx2;
            ny1 = ny2;
        }
        if (!(l2 > 0.0001f))
        {
            nx2 = nx1;
            ny2 = ny1;
        }

        float mx = nx1 + nx2, my = ny1 + ny2;
        float d2 = mx * mx + my * my;
        if (d2 < 0.000001f)
        {
            mx = nx1;
            my = ny1;
            d2 = 1.0f;
        }
        float inv2 = 1.0f / d2;
        if (inv2 > 100.0f) inv2 = 100.0f; // miter limit
        // (n1+n2)/|n1+n2|^2 has length 1/cos(half angle): the miter offset for a unit half width
        float ux = mx * inv2 * 2.0f, uy = my * inv2 * 2.0f;

        if (aa)
        {
            float hw = core * 0.5f, ow = hw + fringe;
            addMapped(p.x + ux * ow, p.y + uy * ow, transparent);
            addMapped(p.x + ux * hw, p.y + uy * hw, color);
            addMapped(p.x - ux * hw, p.y - uy * hw, color);
            addMapped(p.x - ux * ow, p.y - uy * ow, transparent);
        }
        else
        {
            float hw = thickness * 0.5f;
            addMapped(p.x + ux * hw, p.y + uy * hw, color);
            addMapped(p.x - ux * hw, p.y - uy * hw, color);
        }
    }

    uint32_t* ix = appendIndices(idxCount);
    for (int i = 0; i < segs; ++i)
    {
        uint32_t a = base + static_cast<uint32_t>(i) * static_cast<uint32_t>(perPoint);
        uint32_t b = base + static_cast<uint32_t>((i + 1) % n) * static_cast<uint32_t>(perPoint);
        for (int k = 0; k + 1 < perPoint; ++k)
        {
            *ix++ = a + static_cast<uint32_t>(k);
            *ix++ = b + static_cast<uint32_t>(k);
            *ix++ = b + static_cast<uint32_t>(k) + 1;
            *ix++ = a + static_cast<uint32_t>(k);
            *ix++ = b + static_cast<uint32_t>(k) + 1;
            *ix++ = a + static_cast<uint32_t>(k) + 1;
        }
    }

    addCmd(offset, idxCount, fontTexture_, true);
    path_.clear();
}

void DrawList::addRect(const Rect& rect, const Color& color)
{
    uint32_t offset = static_cast<uint32_t>(indices_.size());
    const uint32_t v0 = static_cast<uint32_t>(vertices_.size()), v1 = v0 + 1, v2 = v0 + 2, v3 = v0 + 3;
    DrawVertex* vp = appendVertices(4);
    const Vec2 q0 = mapPoint(rect.x, rect.y), q1 = mapPoint(rect.x + rect.w, rect.y),
               q2 = mapPoint(rect.x + rect.w, rect.y + rect.h), q3 = mapPoint(rect.x, rect.y + rect.h);
    vp[0] = { q0.x, q0.y, whiteUV_.x, whiteUV_.y, color };
    vp[1] = { q1.x, q1.y, whiteUV_.x, whiteUV_.y, color };
    vp[2] = { q2.x, q2.y, whiteUV_.x, whiteUV_.y, color };
    vp[3] = { q3.x, q3.y, whiteUV_.x, whiteUV_.y, color };
    note(4, fminf(rect.x, rect.x + rect.w), fminf(rect.y, rect.y + rect.h), fmaxf(rect.x, rect.x + rect.w), fmaxf(rect.y, rect.y + rect.h));

    uint32_t* ix = appendIndices(6);
    ix[0] = v0;
    ix[1] = v1;
    ix[2] = v2;
    ix[3] = v0;
    ix[4] = v2;
    ix[5] = v3;
    addCmd(offset, 6, fontTexture_, true);
}
void DrawList::addRectOutline(const Rect& rect, const Color& color, float thickness)
{
    thickness = std::max(1.0f, thickness);

    addRect({ rect.x, rect.y, rect.w, thickness }, color);
    addRect({ rect.x, rect.y + rect.h - thickness, rect.w, thickness }, color);

    addRect({ rect.x, rect.y + thickness, thickness, rect.h - thickness * 2.0f }, color);
    addRect({ rect.x + rect.w - thickness, rect.y + thickness, thickness,
                rect.h - thickness * 2.0f },
            color);
}

namespace
{
// The directions of the outline of a rounded rectangle, for the tables of fillRoundRectFast and strokeRoundRectFast. The mitre
// offset of each point of the outline (the arc points of four corners, in the order of pushCornerArc) depends only on the angles
// of the arcs, not on the radius or the size of the rectangle (as long as the straight sides are longer than nothing), so it is
// worked out once for a unit corner, by the same expressions fillConvexPath and pathStroke use, and kept per number of steps.
struct RoundMitre
{
    float fx[4 * (kArcCacheMax + 1)], fy[4 * (kArcCacheMax + 1)]; // offset of the anti-aliasing fringe of a fill (towards outside)
    float ux[4 * (kArcCacheMax + 1)], uy[4 * (kArcCacheMax + 1)]; // mitre of a stroke for a half width of one
    bool done;
};

const RoundMitre& roundMitre(int steps)
{
    static RoundMitre tables[kArcCacheMax + 1];
    RoundMitre& m = tables[steps];
    if (m.done) return m;
    const int n = 4 * (steps + 1);
    Vec2 p[4 * (kArcCacheMax + 1)];
    float ex[4 * (kArcCacheMax + 1)], ey[4 * (kArcCacheMax + 1)];
    const float centreX[4] = { 5.0f, 5.0f, -5.0f, -5.0f }, centreY[4] = { -5.0f, 5.0f, 5.0f, -5.0f }; // a rectangle of 12 by 12, corners of radius 1
    int k = 0;
    for (int corner = 0; corner < 4; ++corner)
    {
        const ArcCache& c = *arcTable(corner, steps);
        for (int i = 0; i <= steps; ++i) p[k++] = { centreX[corner] + c.cosv[i], centreY[corner] + c.sinv[i] };
    }
    float area = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const Vec2 &a = p[i], &b = p[(i + 1) % n];
        area += a.x * b.y - b.x * a.y;
        float dx = b.x - a.x, dy = b.y - a.y;
        const float l = sqrtf(dx * dx + dy * dy);
        if (l > 0.0001f)
        {
            dx /= l;
            dy /= l;
        }
        ex[i] = dx;
        ey[i] = dy;
    }
    const float side = area >= 0.0f ? 1.0f : -1.0f;
    for (int i = 0; i < n; ++i)
    {
        const int before = i == 0 ? n - 1 : i - 1;
        const float dx1 = ex[before], dy1 = ey[before], dx2 = ex[i], dy2 = ey[i];
        // the fill: as fillConvexPath
        float nx = (-dy1 - dy2) * side, ny = (dx1 + dx2) * side;
        float d2 = nx * nx + ny * ny;
        if (d2 < 0.000001f)
        {
            nx = -dy1 * side;
            ny = dx1 * side;
            d2 = 1.0f;
        }
        float inv = 1.0f / d2;
        if (d2 < 0.3f)
        {
            const float maxInv = 2.0f / sqrtf(d2);
            if (inv > maxInv) inv = maxInv;
        }
        m.fx[i] = nx * inv * 2.0f * 0.5f;
        m.fy[i] = ny * inv * 2.0f * 0.5f;
        // the stroke: as pathStroke
        float mx = -dy1 + -dy2, my = dx1 + dx2;
        float e2 = mx * mx + my * my;
        if (e2 < 0.000001f)
        {
            mx = -dy1;
            my = dx1;
            e2 = 1.0f;
        }
        float inv2 = 1.0f / e2;
        if (inv2 > 100.0f) inv2 = 100.0f;
        m.ux[i] = mx * inv2 * 2.0f;
        m.uy[i] = my * inv2 * 2.0f;
    }
    m.done = true;
    return m;
}

// Whether a rectangle (and its corners) is one the tables cover: the sides between the corners are longer than nothing.
inline bool roundRectFits(bool aa, float x0, float y0, float x1, float y1, float radius, int steps)
{
    return aa && steps <= kArcCacheMax && x1 - x0 > radius * 2.0f + 0.01f && y1 - y0 > radius * 2.0f + 0.01f;
}
} // namespace

bool DrawList::fillRoundRectFast(float x0, float y0, float x1, float y1, float radius, int steps, const Color& color)
{
    if (!roundRectFits(antiAliasing_, x0, y0, x1, y1, radius, steps)) return false;
    const RoundMitre& m = roundMitre(steps);
    const uint32_t n = static_cast<uint32_t>(4 * (steps + 1));
    const uint32_t idxCount = (n - 2) * 3 + n * 6;
    const uint32_t offset = static_cast<uint32_t>(indices_.size());
    const uint32_t base = static_cast<uint32_t>(vertices_.size());
    DrawVertex* vp = appendVertices(static_cast<size_t>(n) * 2);
    const float centreX[4] = { x1 - radius, x1 - radius, x0 + radius, x0 + radius };
    const float centreY[4] = { y0 + radius, y1 - radius, y1 - radius, y0 + radius };
    Color outer = color;
    outer.a = 0;
    int k = 0;
    for (int corner = 0; corner < 4; ++corner)
    {
        const ArcCache& c = *arcTable(corner, steps);
        for (int i = 0; i <= steps; ++i, ++k)
        {
            const float px = centreX[corner] + c.cosv[i] * radius, py = centreY[corner] + c.sinv[i] * radius;
            const Vec2 q0 = mapPoint(px - m.fx[k], py - m.fy[k]);
            const Vec2 q1 = mapPoint(px + m.fx[k], py + m.fy[k]);
            *vp++ = { q0.x, q0.y, whiteUV_.x, whiteUV_.y, color };
            *vp++ = { q1.x, q1.y, whiteUV_.x, whiteUV_.y, outer };
        }
    }
    uint32_t* ix = appendIndices(idxCount);
    for (uint32_t i = 2; i < n; ++i)
    {
        *ix++ = base;
        *ix++ = base + (i - 1) * 2;
        *ix++ = base + i * 2;
    }
    for (uint32_t i = 0; i < n; ++i)
    {
        const uint32_t j = i + 1 == n ? 0 : i + 1;
        *ix++ = base + i * 2;
        *ix++ = base + j * 2;
        *ix++ = base + j * 2 + 1;
        *ix++ = base + i * 2;
        *ix++ = base + j * 2 + 1;
        *ix++ = base + i * 2 + 1;
    }
    note(static_cast<size_t>(n) * 2, x0 - 1.0f, y0 - 1.0f, x1 + 1.0f, y1 + 1.0f); // (the fringe is half a pixel out, a mitre at these corners 0.52)
    addCmd(offset, idxCount, fontTexture_, true);
    return true;
}

bool DrawList::strokeRoundRectFast(float x0, float y0, float x1, float y1, float radius, int steps, const Color& color, float thickness)
{
    if (!roundRectFits(antiAliasing_, x0, y0, x1, y1, radius, steps)) return false;
    const RoundMitre& m = roundMitre(steps);
    thickness = fmaxf(1.0f, thickness);
    const float core = fmaxf(1.0f, thickness - 1.0f);
    const float hw = core * 0.5f, ow = hw + 1.0f;
    const uint32_t n = static_cast<uint32_t>(4 * (steps + 1));
    const uint32_t idxCount = n * 18;
    const uint32_t offset = static_cast<uint32_t>(indices_.size());
    const uint32_t base = static_cast<uint32_t>(vertices_.size());
    DrawVertex* vp = appendVertices(static_cast<size_t>(n) * 4);
    const float centreX[4] = { x1 - radius, x1 - radius, x0 + radius, x0 + radius };
    const float centreY[4] = { y0 + radius, y1 - radius, y1 - radius, y0 + radius };
    Color transparent = color;
    transparent.a = 0;
    int k = 0;
    for (int corner = 0; corner < 4; ++corner)
    {
        const ArcCache& c = *arcTable(corner, steps);
        for (int i = 0; i <= steps; ++i, ++k)
        {
            const float px = centreX[corner] + c.cosv[i] * radius, py = centreY[corner] + c.sinv[i] * radius;
            const float ux = m.ux[k], uy = m.uy[k];
            const Vec2 q0 = mapPoint(px + ux * ow, py + uy * ow), q1 = mapPoint(px + ux * hw, py + uy * hw);
            const Vec2 q2 = mapPoint(px - ux * hw, py - uy * hw), q3 = mapPoint(px - ux * ow, py - uy * ow);
            *vp++ = { q0.x, q0.y, whiteUV_.x, whiteUV_.y, transparent };
            *vp++ = { q1.x, q1.y, whiteUV_.x, whiteUV_.y, color };
            *vp++ = { q2.x, q2.y, whiteUV_.x, whiteUV_.y, color };
            *vp++ = { q3.x, q3.y, whiteUV_.x, whiteUV_.y, transparent };
        }
    }
    uint32_t* ix = appendIndices(idxCount);
    for (uint32_t i = 0; i < n; ++i)
    {
        const uint32_t a = base + i * 4;
        const uint32_t b = base + (i + 1 == n ? 0 : i + 1) * 4;
        for (uint32_t q = 0; q < 3; ++q)
        {
            *ix++ = a + q;
            *ix++ = b + q;
            *ix++ = b + q + 1;
            *ix++ = a + q;
            *ix++ = b + q + 1;
            *ix++ = a + q + 1;
        }
    }
    const float reach = ow * 1.1f + 0.01f; // (the mitre of the corners: at most 1 / cos(15 degrees) of the half width)
    note(static_cast<size_t>(n) * 4, x0 - reach, y0 - reach, x1 + reach, y1 + reach);
    addCmd(offset, idxCount, fontTexture_, true);
    return true;
}

void DrawList::addRoundRectFilled(const Rect& rect, float radius, const Color& color, int segments)
{
    if (rect.w <= 0.0f || rect.h <= 0.0f) return;
    radius = std::max(0.0f, std::min(radius, std::min(rect.w, rect.h) * 0.5f));

    if (radius < 0.5f)
    {
        addRect(rect, color);
        return;
    }

    float x0 = rect.x, y0 = rect.y, x1 = rect.x + rect.w, y1 = rect.y + rect.h;
    const int steps = cornerSegments(radius, segments);
    if (fillRoundRectFast(x0, y0, x1, y1, radius, steps, color)) return;
    // TR: 270°→360°  BR: 0°→90°  BL: 90°→180°  TL: 180°→270°
    pushCornerArc(path_, { x1 - radius, y0 + radius }, radius, -Pi * 0.5f, steps);
    pushCornerArc(path_, { x1 - radius, y1 - radius }, radius, 0.0f, steps);
    pushCornerArc(path_, { x0 + radius, y1 - radius }, radius, Pi * 0.5f, steps);
    pushCornerArc(path_, { x0 + radius, y0 + radius }, radius, Pi, steps);
    pathFillConvex(color);
}

void DrawList::addRoundRect(const Rect& rect, float radius, const Color& color, float thickness,
        int segments)
{
    if (rect.w <= 0.0f || rect.h <= 0.0f) return;
    // Inset by half the stroke thickness so the stroke centerline sits on
    // the fill boundary rather than half-outside, half-inside.
    float inset = thickness * 0.5f;
    Rect r2 = { rect.x + inset, rect.y + inset, rect.w - inset * 2.0f, rect.h - inset * 2.0f };
    radius = std::max(0.0f, std::min(radius, std::min(r2.w, r2.h) * 0.5f));

    if (radius < 0.5f)
    {
        addRectOutline(rect, color, thickness);
        return;
    }

    float x0 = r2.x, y0 = r2.y, x1 = r2.x + r2.w, y1 = r2.y + r2.h;
    const int steps = cornerSegments(radius, segments);
    if (strokeRoundRectFast(x0, y0, x1, y1, radius, steps, color, thickness)) return;
    pushCornerArc(path_, { x1 - radius, y0 + radius }, radius, -Pi * 0.5f, steps);
    pushCornerArc(path_, { x1 - radius, y1 - radius }, radius, 0.0f, steps);
    pushCornerArc(path_, { x0 + radius, y1 - radius }, radius, Pi * 0.5f, steps);
    pushCornerArc(path_, { x0 + radius, y0 + radius }, radius, Pi, steps);
    pathStroke(color, thickness, true);
}

void DrawList::addCircleFilled(Vec2 center, float radius, const Color& color, int segments)
{
    if (radius < 0.5f) return;
    segments = circleSegments(radius, segments);
    path_.reserve(static_cast<size_t>(segments));
    for (int i = 0; i < segments; ++i)
    {
        float a = (static_cast<float>(i) / static_cast<float>(segments)) * Pi * 2.0f;
        path_.push_back({ center.x + std::cos(a) * radius, center.y + std::sin(a) * radius });
    }
    pathFillConvex(color);
}

void DrawList::addCircle(Vec2 center, float radius, const Color& color, float thickness,
        int segments)
{
    if (radius < 0.5f) return;
    segments = circleSegments(radius, segments);
    path_.reserve(static_cast<size_t>(segments));
    for (int i = 0; i < segments; ++i)
    {
        float a = (static_cast<float>(i) / static_cast<float>(segments)) * Pi * 2.0f;
        path_.push_back({ center.x + std::cos(a) * radius, center.y + std::sin(a) * radius });
    }
    pathStroke(color, thickness, true);
}

void DrawList::addTriangleFilled(Vec2 a, Vec2 b, Vec2 c, const Color& color)
{
    path_.clear();
    path_.push_back(a);
    path_.push_back(b);
    path_.push_back(c);
    pathFillConvex(color);
}

void DrawList::addTriangle(Vec2 a, Vec2 b, Vec2 c, const Color& color, float thickness)
{
    addPolyline({ a, b, c }, color, thickness, true);
}

void DrawList::addImage(TextureHandle texture, const Rect& rect, const Rect& uv, const Color& color)
{
    if (!texture || rect.w <= 0.0f || rect.h <= 0.0f) return;

    uint32_t offset = static_cast<uint32_t>(indices_.size());
    uint32_t v0 = addVertex(rect.x, rect.y, uv.x, uv.y, color);
    uint32_t v1 = addVertex(rect.x + rect.w, rect.y, uv.x + uv.w, uv.y, color);
    uint32_t v2 = addVertex(rect.x + rect.w, rect.y + rect.h, uv.x + uv.w, uv.y + uv.h, color);
    uint32_t v3 = addVertex(rect.x, rect.y + rect.h, uv.x, uv.y + uv.h, color);

    uint32_t* ix = appendIndices(6);
    ix[0] = v0;
    ix[1] = v1;
    ix[2] = v2;
    ix[3] = v0;
    ix[4] = v2;
    ix[5] = v3;
    note(4, rect.x, rect.y, rect.x + rect.w, rect.y + rect.h);
    addCmd(offset, 6, texture);
}

void DrawList::addText(const Font& font, Vec2 pos, const Color& color, const char* text,
        float scale)
{
    if (!font.texture() || !text) return;

    uint32_t offset = static_cast<uint32_t>(indices_.size());
    Vec2 pen = pos;
    uint32_t previous = 0;
    const bool kern = kerns(font); // decided once for the text, not for every pair
    float bx0 = 1.0e30f, by0 = 1.0e30f, bx1 = -1.0e30f, by1 = -1.0e30f; // the bounds of the glyph quads, for addCmd
    size_t quads = 0;
    const bool plain = transformStack_.empty(); // (no transform: the points are not mapped)

    while (*text)
    {
        // (an ASCII letter without the call)
        const uint32_t codepoint = static_cast<unsigned char>(*text) < 0x80 ? static_cast<unsigned char>(*text++) : decodeUtf8(text);
        if (codepoint == '\n')
        {
            pen.x = pos.x;
            pen.y += font.lineHeight() * scale;
            previous = 0;
            continue;
        }

        const FontGlyph* glyph = font.findGlyph(codepoint);
        if (!glyph) continue;
        if (kern && previous) pen.x += font.pairKerning(previous, codepoint) * scale;
        previous = codepoint;

        if (glyph->size.x > 0.0f && glyph->size.y > 0.0f)
        {
            // each glyph lands on whole pixels: the pen keeps its fractional advance, but a
            // bitmap drawn between pixels blurs under linear filtering
            const float ps = font.pixelScale();
            Rect rect{ pen.x + glyph->offset.x * scale, pen.y + glyph->offset.y * scale,
                glyph->size.x * scale, glyph->size.y * scale };
            if (textSnapping_)
            {
                rect.x = floorOf(rect.x * ps + 0.5f) / ps;
                rect.y = floorOf(rect.y * ps + 0.5f) / ps;
            }
            Rect uv = glyph->uv;
            if (rect.x < bx0) bx0 = rect.x;
            if (rect.y < by0) by0 = rect.y;
            if (rect.x + rect.w > bx1) bx1 = rect.x + rect.w;
            if (rect.y + rect.h > by1) by1 = rect.y + rect.h;
            ++quads;

            const uint32_t v0 = static_cast<uint32_t>(vertices_.size()), v1 = v0 + 1, v2 = v0 + 2,
                           v3 = v0 + 3;
            DrawVertex* vp = appendVertices(4);
            const Vec2 q0 = plain ? Vec2{ rect.x, rect.y } : mapPoint(rect.x, rect.y),
                       q1 = plain ? Vec2{ rect.x + rect.w, rect.y } : mapPoint(rect.x + rect.w, rect.y),
                       q2 = plain ? Vec2{ rect.x + rect.w, rect.y + rect.h } : mapPoint(rect.x + rect.w, rect.y + rect.h),
                       q3 = plain ? Vec2{ rect.x, rect.y + rect.h } : mapPoint(rect.x, rect.y + rect.h);
            vp[0] = { q0.x, q0.y, uv.x, uv.y, color };
            vp[1] = { q1.x, q1.y, uv.x + uv.w, uv.y, color };
            vp[2] = { q2.x, q2.y, uv.x + uv.w, uv.y + uv.h, color };
            vp[3] = { q3.x, q3.y, uv.x, uv.y + uv.h, color };
            uint32_t* ix = appendIndices(6);
            ix[0] = v0;
            ix[1] = v1;
            ix[2] = v2;
            ix[3] = v0;
            ix[4] = v2;
            ix[5] = v3;
        }

        pen.x += glyph->advanceX * scale;
    }

    if (quads > 0) note(quads * 4, bx0, by0, bx1, by1);
    addCmd(offset, static_cast<uint32_t>(indices_.size()) - offset, font.texture());
}

Vec2 DrawList::calcTextSize(const Font& font, const char* text, float scale)
{
    if (!text) return { 0.0f, 0.0f };

    float maxLineWidth = 0.0f;
    float lineWidth = 0.0f;
    int lines = 1;
    const char* cursor = text;
    uint32_t previous = 0;
    const bool kern = kerns(font); // decided once for the text, not for every pair

    while (*cursor)
    {
        const uint32_t codepoint = static_cast<unsigned char>(*cursor) < 0x80 ? static_cast<unsigned char>(*cursor++) : decodeUtf8(cursor);
        if (codepoint == '\n')
        {
            maxLineWidth = std::max(maxLineWidth, lineWidth);
            lineWidth = 0.0f;
            ++lines;
            previous = 0;
            continue;
        }

        const FontGlyph* glyph = font.findGlyph(codepoint);
        if (glyph)
        {
            if (kern && previous) lineWidth += font.pairKerning(previous, codepoint) * scale;
            lineWidth += glyph->advanceX * scale;
            previous = codepoint;
        }
    }

    maxLineWidth = std::max(maxLineWidth, lineWidth);
    float height = font.lineHeight() * scale * static_cast<float>(lines);

    return { maxLineWidth, height };
}

void DrawList::addTextAligned(const Font& font, const Rect& bounds, const Color& color,
        const char* text, AlignX ax, AlignY ay)
{
    if (!font.texture() || !text) return;

    Vec2 size = calcTextSize(font, text);

    // -- resolve X --
    float x = bounds.x;
    switch (ax)
    {
        case AlignX::Center:
            x = bounds.x + (bounds.w - size.x) * 0.5f;
            break;
        case AlignX::Right:
            x = bounds.x + bounds.w - size.x;
            break;
        default:
            break; // Left
    }

    // -- resolve Y --
    float y = bounds.y;
    switch (ay)
    {
        case AlignY::Middle:
            y = bounds.y + (bounds.h - size.y) * 0.5f;
            break;
        case AlignY::Bottom:
            y = bounds.y + bounds.h - size.y;
            break;
        default:
            break; // Top
    }

    addText(font, { x, y }, color, text);
}

void DrawList::addIcon(IconId icon, const Rect& rect, const Color& color, float thickness)
{
    float x0 = rect.x;
    float y0 = rect.y;
    float x1 = rect.x + rect.w;
    float y1 = rect.y + rect.h;
    float cx = rect.x + rect.w * 0.5f;
    float cy = rect.y + rect.h * 0.5f;
    float s = std::min(rect.w, rect.h);
    float p = s * 0.22f;

    switch (icon)
    {
        case IconId::Check:
            addLine({ x0 + p, cy }, { cx - s * 0.08f, y1 - p }, color, thickness);
            addLine({ cx - s * 0.08f, y1 - p }, { x1 - p, y0 + p }, color, thickness);
            break;

        case IconId::Cross:
            addLine({ x0 + p, y0 + p }, { x1 - p, y1 - p }, color, thickness);
            addLine({ x1 - p, y0 + p }, { x0 + p, y1 - p }, color, thickness);
            break;

        case IconId::Plus:
            addLine({ cx, y0 + p }, { cx, y1 - p }, color, thickness);
            addLine({ x0 + p, cy }, { x1 - p, cy }, color, thickness);
            break;

        case IconId::Minus:
            addLine({ x0 + p, cy }, { x1 - p, cy }, color, thickness);
            break;

        case IconId::ArrowRight:
            addTriangleFilled({ x1 - p, cy }, { x0 + p, y0 + p }, { x0 + p, y1 - p }, color);
            break;

        case IconId::ArrowDown:
            addTriangleFilled({ cx, y1 - p }, { x0 + p, y0 + p }, { x1 - p, y0 + p }, color);
            break;

        case IconId::ArrowLeft:
            addTriangleFilled({ x0 + p, cy }, { x1 - p, y0 + p }, { x1 - p, y1 - p }, color);
            break;

        case IconId::ArrowUp:
            addTriangleFilled({ cx, y0 + p }, { x0 + p, y1 - p }, { x1 - p, y1 - p }, color);
            break;

        case IconId::Search:
            addCircle({ cx - s * 0.08f, cy - s * 0.08f }, s * 0.23f, color, thickness);
            addLine({ cx + s * 0.12f, cy + s * 0.12f }, { x1 - p, y1 - p }, color, thickness);
            break;

        case IconId::Folder:
        case IconId::FolderOpen:
        case IconId::FolderNew:
        {
            // tab on the top left, body below, filled lightly in the stroke colour
            float l = x0 + s * 0.10f, r = x1 - s * 0.10f, t = y0 + s * 0.22f, b = y1 - s * 0.18f;
            float tab = y0 + s * 0.12f;
            Color body = color;
            body.a = static_cast<uint8_t>(color.a * 0.35f);
            addRect({ l, t, r - l, b - t }, body);
            ct::Vector<Vec2>& outline = iconPoints_[0];
            outline.clear();
            outline.push_back({ l, b });
            outline.push_back({ l, tab });
            outline.push_back({ l + s * 0.30f, tab });
            outline.push_back({ l + s * 0.38f, t });
            outline.push_back({ r, t });
            outline.push_back({ r, b });
            addPolyline(outline, color, thickness, true);
            if (icon == IconId::FolderNew)
            {
                // a plus in the body
                float mx = (l + r) * 0.5f, my = (t + b) * 0.5f, u = s * 0.16f;
                addLine({ mx - u, my }, { mx + u, my }, color, thickness * 1.3f);
                addLine({ mx, my - u }, { mx, my + u }, color, thickness * 1.3f);
            }
            break;
        }

        case IconId::File:
        case IconId::FileCode:
        case IconId::FileImage:
        case IconId::FileArchive:
        {
            // page with a folded corner, and a mark for the kind
            float l = x0 + s * 0.20f, r = x1 - s * 0.20f, t = y0 + s * 0.08f, b = y1 - s * 0.08f;
            float fold = s * 0.22f;
            ct::Vector<Vec2>& page = iconPoints_[0];
            page.clear();
            page.push_back({ l, t });
            page.push_back({ r - fold, t });
            page.push_back({ r, t + fold });
            page.push_back({ r, b });
            page.push_back({ l, b });
            addPolyline(page, color, thickness, true);
            addLine({ r - fold, t }, { r - fold, t + fold }, color, thickness);
            addLine({ r - fold, t + fold }, { r, t + fold }, color, thickness);
            float mx = (l + r) * 0.5f, my = (t + fold + b) * 0.5f;
            float u = s * 0.11f;
            if (icon == IconId::FileCode)
            {
                addLine({ mx - u * 0.4f, my - u }, { mx - u * 1.3f, my }, color, thickness);
                addLine({ mx - u * 1.3f, my }, { mx - u * 0.4f, my + u }, color, thickness);
                addLine({ mx + u * 0.4f, my - u }, { mx + u * 1.3f, my }, color, thickness);
                addLine({ mx + u * 1.3f, my }, { mx + u * 0.4f, my + u }, color, thickness);
            }
            else if (icon == IconId::FileImage)
            {
                addTriangleFilled({ l + s * 0.06f, b - s * 0.06f }, { mx, my - u },
                        { r - s * 0.06f, b - s * 0.06f }, color);
                addCircleFilled({ r - s * 0.16f, t + fold + s * 0.08f }, s * 0.05f, color, 12);
            }
            else if (icon == IconId::FileArchive)
            {
                for (int i = 0; i < 3; ++i)
                    addRect({ mx - u * 0.5f, t + s * 0.06f + i * u * 1.6f, u, u * 0.8f }, color);
            }
            else
            {
                addLine({ l + s * 0.10f, my - u }, { r - s * 0.10f, my - u }, color, thickness);
                addLine({ l + s * 0.10f, my + u * 0.6f }, { r - s * 0.10f, my + u * 0.6f }, color,
                        thickness);
            }
            break;
        }

        case IconId::ViewDetail:
            // small square + line, three rows
            for (int i = 0; i < 3; ++i)
            {
                float yy = y0 + p + (s - 2 * p) * i / 2.0f;
                float d = s * 0.10f;
                addRect({ x0 + p, yy - d * 0.5f, d, d }, color);
                addLine({ x0 + p + s * 0.20f, yy }, { x1 - p, yy }, color, thickness);
            }
            break;

        case IconId::ViewList:
            for (int i = 0; i < 3; ++i)
            {
                float yy = y0 + p + (s - 2 * p) * i / 2.0f;
                addLine({ x0 + p, yy }, { x1 - p, yy }, color, thickness);
            }
            break;

        case IconId::ViewGrid:
        {
            float g = s * 0.12f, c = (s - 2 * p - g) * 0.5f;
            for (int j = 0; j < 2; ++j)
                for (int i = 0; i < 2; ++i)
                    addRectOutline({ x0 + p + i * (c + g), y0 + p + j * (c + g), c, c }, color,
                            thickness);
            break;
        }

        case IconId::Eye:
        case IconId::EyeOff:
        {
            // almond outline from two arcs, a pupil, and a strike for "off"
            ct::Vector<Vec2>& eye = iconPoints_[0];
            eye.clear();
            const int n = 10;
            float w = s * 0.5f - p * 0.6f, h = s * 0.26f;
            for (int i = 0; i <= n; ++i)
            {
                float a = 3.14159265f * i / n;
                eye.push_back({ cx - w * std::cos(a), cy - h * std::sin(a) });
            }
            for (int i = n - 1; i > 0; --i)
            {
                float a = 3.14159265f * i / n;
                eye.push_back({ cx - w * std::cos(a), cy + h * std::sin(a) });
            }
            addPolyline(eye, color, thickness, true);
            addCircleFilled({ cx, cy }, s * 0.11f, color, 16);
            if (icon == IconId::EyeOff)
                addLine({ x0 + p, y1 - p }, { x1 - p, y0 + p }, color, thickness);
            break;
        }

        case IconId::Play:
            addTriangleFilled({ cx - s * 0.18f, cy - s * 0.27f },
                    { cx - s * 0.18f, cy + s * 0.27f }, { cx + s * 0.30f, cy }, color);
            break;

        case IconId::Pause:
            addRect({ cx - s * 0.20f, cy - s * 0.25f, s * 0.14f, s * 0.50f }, color);
            addRect({ cx + s * 0.06f, cy - s * 0.25f, s * 0.14f, s * 0.50f }, color);
            break;

        case IconId::Stop:
            addRect({ cx - s * 0.22f, cy - s * 0.22f, s * 0.44f, s * 0.44f }, color);
            break;

        case IconId::StepForward:
            addTriangleFilled({ cx - s * 0.28f, cy - s * 0.25f },
                    { cx - s * 0.28f, cy + s * 0.25f }, { cx + s * 0.12f, cy }, color);
            addRect({ cx + s * 0.16f, cy - s * 0.25f, s * 0.11f, s * 0.50f }, color);
            break;

        case IconId::StepBack:
            addTriangleFilled({ cx + s * 0.28f, cy - s * 0.25f },
                    { cx + s * 0.28f, cy + s * 0.25f }, { cx - s * 0.12f, cy }, color);
            addRect({ cx - s * 0.27f, cy - s * 0.25f, s * 0.11f, s * 0.50f }, color);
            break;

        case IconId::Record:
            addCircleFilled({ cx, cy }, s * 0.26f, color, 24);
            break;

        case IconId::Trash:
        {
            float top = y0 + s * 0.30f, bot = y1 - s * 0.14f;
            addLine({ cx - s * 0.30f, top }, { cx + s * 0.30f, top }, color, thickness);
            ct::Vector<Vec2>& handle = iconPoints_[0];
            handle.clear();
            handle.push_back({ cx - s * 0.10f, top });
            handle.push_back({ cx - s * 0.10f, y0 + s * 0.18f });
            handle.push_back({ cx + s * 0.10f, y0 + s * 0.18f });
            handle.push_back({ cx + s * 0.10f, top });
            addPolyline(handle, color, thickness, false);
            ct::Vector<Vec2>& bin = iconPoints_[1];
            bin.clear();
            bin.push_back({ cx - s * 0.22f, top });
            bin.push_back({ cx - s * 0.18f, bot });
            bin.push_back({ cx + s * 0.18f, bot });
            bin.push_back({ cx + s * 0.22f, top });
            addPolyline(bin, color, thickness, false);
            addLine({ cx - s * 0.07f, top + s * 0.10f }, { cx - s * 0.06f, bot - s * 0.08f }, color,
                    thickness);
            addLine({ cx + s * 0.07f, top + s * 0.10f }, { cx + s * 0.06f, bot - s * 0.08f }, color,
                    thickness);
            break;
        }

        case IconId::Lock:
        case IconId::Unlock:
        {
            float body = cy + s * 0.02f, r = s * 0.17f;
            addRect({ cx - s * 0.25f, body, s * 0.50f, s * 0.34f }, color);
            float lift = icon == IconId::Unlock ? s * 0.10f : 0.0f;
            ct::Vector<Vec2>& shackle = iconPoints_[0];
            shackle.clear();
            shackle.push_back({ cx - r, body });
            shackle.push_back({ cx - r, body - s * 0.08f - lift });
            for (int i = 0; i <= 8; ++i)
            {
                float a = 3.14159265f * (1.0f - i / 8.0f);
                shackle.push_back(
                        { cx + r * std::cos(a), body - s * 0.08f - lift - r * std::sin(a) });
            }
            if (icon == IconId::Lock) shackle.push_back({ cx + r, body });
            addPolyline(shackle, color, thickness, false);
            break;
        }

        case IconId::Scissors:
        {
            // blades cross at the pivot and run to the finger rings
            addLine({ cx - s * 0.20f, y0 + s * 0.10f }, { cx + s * 0.20f, y1 - s * 0.34f }, color,
                    thickness);
            addLine({ cx + s * 0.20f, y0 + s * 0.10f }, { cx - s * 0.20f, y1 - s * 0.34f }, color,
                    thickness);
            addCircle({ cx - s * 0.20f, y1 - s * 0.21f }, s * 0.12f, color, thickness, 16);
            addCircle({ cx + s * 0.20f, y1 - s * 0.21f }, s * 0.12f, color, thickness, 16);
            break;
        }

        case IconId::Import:
        {
            addLine({ cx, y0 + s * 0.12f }, { cx, cy + s * 0.14f }, color, thickness * 1.2f);
            addLine({ cx, cy + s * 0.16f }, { cx - s * 0.17f, cy - s * 0.02f }, color,
                    thickness * 1.2f);
            addLine({ cx, cy + s * 0.16f }, { cx + s * 0.17f, cy - s * 0.02f }, color,
                    thickness * 1.2f);
            ct::Vector<Vec2>& tray = iconPoints_[0];
            tray.clear();
            tray.push_back({ cx - s * 0.32f, cy + s * 0.12f });
            tray.push_back({ cx - s * 0.32f, y1 - s * 0.14f });
            tray.push_back({ cx + s * 0.32f, y1 - s * 0.14f });
            tray.push_back({ cx + s * 0.32f, cy + s * 0.12f });
            addPolyline(tray, color, thickness, false);
            break;
        }

        case IconId::MarkIn:
        case IconId::MarkOut:
        {
            // a bracket: a tall line with short arms, opening towards the range
            const float sx = icon == IconId::MarkIn ? 1.0f : -1.0f;
            const float bx = icon == IconId::MarkIn ? x0 + p * 1.2f : x1 - p * 1.2f;
            const float arm = s * 0.28f;
            addLine({ bx, y0 + p * 0.8f }, { bx, y1 - p * 0.8f }, color, thickness * 1.4f);
            addLine({ bx, y0 + p * 0.8f }, { bx + sx * arm, y0 + p * 0.8f }, color,
                    thickness * 1.4f);
            addLine({ bx, y1 - p * 0.8f }, { bx + sx * arm, y1 - p * 0.8f }, color,
                    thickness * 1.4f);
            break;
        }

        case IconId::PaintBrush:
        {
            // a handle down to a ferrule, and the tuft below it
            addLine({ x1 - p * 0.9f, y0 + p * 0.9f }, { cx + s * 0.02f, cy - s * 0.02f }, color,
                    thickness * 1.3f);
            addLine({ cx + s * 0.04f, cy - s * 0.04f }, { cx - s * 0.06f, cy + s * 0.06f }, color,
                    thickness * 2.4f);
            addTriangleFilled({ cx - s * 0.13f, cy - s * 0.01f },
                    { cx + s * 0.01f, cy + s * 0.13f }, { x0 + p * 0.7f, y1 - p * 0.7f }, color);
            break;
        }

        case IconId::Eraser:
        {
            // a block lying at 45 degrees, its rubbing end marked, on a line
            const Vec2 d{ 0.7071f, -0.7071f }, n{ 0.7071f, 0.7071f };
            const float l = s * 0.30f, w = s * 0.14f;
            const Vec2 c{ cx, cy - s * 0.04f };
            ct::Vector<Vec2>& block = iconPoints_[0];
            block.clear();
            block.push_back({ c.x - d.x * l - n.x * w, c.y - d.y * l - n.y * w });
            block.push_back({ c.x + d.x * l - n.x * w, c.y + d.y * l - n.y * w });
            block.push_back({ c.x + d.x * l + n.x * w, c.y + d.y * l + n.y * w });
            block.push_back({ c.x - d.x * l + n.x * w, c.y - d.y * l + n.y * w });
            addPolyline(block, color, thickness, true);
            const float m = -l * 0.25f;
            addLine({ c.x + d.x * m - n.x * w, c.y + d.y * m - n.y * w },
                    { c.x + d.x * m + n.x * w, c.y + d.y * m + n.y * w }, color, thickness);
            addLine({ x0 + p * 0.7f, y1 - p * 0.55f }, { x1 - p * 0.7f, y1 - p * 0.55f }, color,
                    thickness);
            break;
        }

        case IconId::PaintBucket:
        {
            // a tipped pail with a drop falling from its lip
            ct::Vector<Vec2>& pail = iconPoints_[0];
            pail.clear();
            pail.push_back({ cx - s * 0.06f, y0 + p * 0.8f });
            pail.push_back({ x1 - p * 1.3f, cy - s * 0.04f });
            pail.push_back({ cx - s * 0.06f, y1 - p * 0.9f });
            pail.push_back({ x0 + p * 0.8f, cy - s * 0.04f });
            addPolyline(pail, color, thickness, true);
            addLine({ x1 - p * 1.3f, cy - s * 0.04f }, { x1 - p * 0.75f, cy + s * 0.12f }, color,
                    thickness);
            addCircleFilled({ x1 - p * 0.75f, y1 - p * 1.0f }, s * 0.075f, color, 12);
            break;
        }

        case IconId::SelectRect:
        {
            // a dashed outline
            const float l = x0 + p * 0.8f, t = y0 + p * 0.9f, r = x1 - p * 0.8f, b = y1 - p * 0.9f;
            const int dashes = 3;
            for (int i = 0; i < dashes; ++i)
            {
                const float a = static_cast<float>(i * 2) / (dashes * 2 - 1);
                const float e = static_cast<float>(i * 2 + 1) / (dashes * 2 - 1);
                addLine({ l + (r - l) * a, t }, { l + (r - l) * e, t }, color, thickness);
                addLine({ l + (r - l) * a, b }, { l + (r - l) * e, b }, color, thickness);
                addLine({ l, t + (b - t) * a }, { l, t + (b - t) * e }, color, thickness);
                addLine({ r, t + (b - t) * a }, { r, t + (b - t) * e }, color, thickness);
            }
            break;
        }

        case IconId::Eyedropper:
        {
            // a thin tube up to a collar and the bulb
            addLine({ x0 + p * 0.7f, y1 - p * 0.7f }, { cx + s * 0.06f, cy - s * 0.06f }, color,
                    thickness * 1.2f);
            addLine({ cx - s * 0.04f, cy - s * 0.14f }, { cx + s * 0.14f, cy + s * 0.04f }, color,
                    thickness * 1.6f);
            addCircleFilled({ x1 - p * 1.2f, y0 + p * 1.2f }, s * 0.13f, color, 16);
            break;
        }

        case IconId::Magnet:
        {
            float r = s * 0.27f, w = s * 0.14f, my = cy - s * 0.06f, bot = y1 - s * 0.14f;
            ct::Vector<Vec2>& outer = iconPoints_[0];
            ct::Vector<Vec2>& inner = iconPoints_[1];
            outer.clear();
            inner.clear();
            outer.push_back({ cx - r - w * 0.5f, bot });
            inner.push_back({ cx - r + w * 0.5f, bot });
            for (int i = 0; i <= 12; ++i)
            {
                float a = 3.14159265f * (1.0f - i / 12.0f);
                outer.push_back(
                        { cx + (r + w * 0.5f) * std::cos(a), my - (r + w * 0.5f) * std::sin(a) });
                inner.push_back(
                        { cx + (r - w * 0.5f) * std::cos(a), my - (r - w * 0.5f) * std::sin(a) });
            }
            outer.push_back({ cx + r + w * 0.5f, bot });
            inner.push_back({ cx + r - w * 0.5f, bot });
            addPolyline(outer, color, thickness, false);
            addPolyline(inner, color, thickness, false);
            // the two poles
            addRect({ cx - r - w * 0.5f, bot - s * 0.12f, w, s * 0.12f },
                    Color(225, 80, 80, color.a));
            addRect({ cx + r - w * 0.5f, bot - s * 0.12f, w, s * 0.12f },
                    Color(95, 145, 235, color.a));
            break;
        }

        default:
            break;
    }
}

bool DrawList::hasVectorIcon(IconId icon)
{
    switch (icon)
    {
        case IconId::Check:
        case IconId::Cross:
        case IconId::Plus:
        case IconId::Minus:
        case IconId::ArrowRight:
        case IconId::ArrowDown:
        case IconId::ArrowLeft:
        case IconId::ArrowUp:
        case IconId::Search:
        case IconId::Folder:
        case IconId::FolderOpen:
        case IconId::FolderNew:
        case IconId::File:
        case IconId::FileCode:
        case IconId::FileImage:
        case IconId::FileArchive:
        case IconId::ViewDetail:
        case IconId::ViewList:
        case IconId::ViewGrid:
        case IconId::Eye:
        case IconId::EyeOff:
        case IconId::Play:
        case IconId::Pause:
        case IconId::Stop:
        case IconId::StepForward:
        case IconId::StepBack:
        case IconId::Record:
        case IconId::Trash:
        case IconId::Lock:
        case IconId::Unlock:
        case IconId::Scissors:
        case IconId::Import:
        case IconId::Magnet:
        case IconId::MarkIn:
        case IconId::MarkOut:
        case IconId::PaintBrush:
        case IconId::Eraser:
        case IconId::PaintBucket:
        case IconId::SelectRect:
        case IconId::Eyedropper:
            return true;
        default:
            return false;
    }
}

void DrawList::addLine(Vec2 a, Vec2 b, const Color& color, float thickness)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    if (dx * dx + dy * dy <= 0.00000001f) return;
    const float length = sqrtf(dx * dx + dy * dy);
    if (antiAliasing_ && length > 0.0001f)
    {
        // a segment of two points (what pathStroke makes of it, without the tables of directions it works out for a polyline): the
        // normal at both ends is the one of the segment, a band of the core width and a fringe of one pixel on each side
        thickness = fmaxf(1.0f, thickness);
        const float hw = fmaxf(1.0f, thickness - 1.0f) * 0.5f, ow = hw + 1.0f;
        const float ux = -(dy / length), uy = dx / length;
        const uint32_t offset = static_cast<uint32_t>(indices_.size());
        const uint32_t base = static_cast<uint32_t>(vertices_.size());
        Color transparent = color;
        transparent.a = 0;
        DrawVertex* vp = appendVertices(8);
        const Vec2 ends[2] = { a, b };
        for (int e = 0; e < 2; ++e)
        {
            const Vec2 q0 = mapPoint(ends[e].x + ux * ow, ends[e].y + uy * ow), q1 = mapPoint(ends[e].x + ux * hw, ends[e].y + uy * hw);
            const Vec2 q2 = mapPoint(ends[e].x - ux * hw, ends[e].y - uy * hw), q3 = mapPoint(ends[e].x - ux * ow, ends[e].y - uy * ow);
            *vp++ = { q0.x, q0.y, whiteUV_.x, whiteUV_.y, transparent };
            *vp++ = { q1.x, q1.y, whiteUV_.x, whiteUV_.y, color };
            *vp++ = { q2.x, q2.y, whiteUV_.x, whiteUV_.y, color };
            *vp++ = { q3.x, q3.y, whiteUV_.x, whiteUV_.y, transparent };
        }
        uint32_t* ix = appendIndices(18);
        for (uint32_t q = 0; q < 3; ++q)
        {
            *ix++ = base + q;
            *ix++ = base + 4 + q;
            *ix++ = base + 4 + q + 1;
            *ix++ = base + q;
            *ix++ = base + 4 + q + 1;
            *ix++ = base + q + 1;
        }
        note(8, fminf(a.x, b.x) - ow, fminf(a.y, b.y) - ow, fmaxf(a.x, b.x) + ow, fmaxf(a.y, b.y) + ow);
        addCmd(offset, 18, fontTexture_, true);
        return;
    }
    path_.clear();
    path_.push_back(a);
    path_.push_back(b);
    pathStroke(color, thickness, false);
}

void DrawList::addConvexPolyFilled(const ct::Vector<Vec2>& points, const Color& color)
{
    for (ct::Vector<Vec2>::const_iterator it = points.begin(); it != points.end(); ++it)
        path_.push_back(*it);
    pathFillConvex(color);
}

void DrawList::addPolyline(const ct::Vector<Vec2>& points, const Color& color, float thickness,
        bool closed)
{
    for (ct::Vector<Vec2>::const_iterator it = points.begin(); it != points.end(); ++it)
        path_.push_back(*it);
    pathStroke(color, thickness, closed);
}

Rect DrawList::currentClip() const
{
    if (!clipStack_.empty()) return clipStack_.back().rect;
    return { 0.0f, 0.0f, 100000.0f, 100000.0f };
}

namespace
{
inline bool rectHolds(const Rect& outer, const Rect& inner)
{
    return inner.x >= outer.x && inner.y >= outer.y && inner.x + inner.w <= outer.x + outer.w && inner.y + inner.h <= outer.y + outer.h;
}

inline Rect rectUnion(const Rect& a, const Rect& b)
{
    const float x0 = fminf(a.x, b.x), y0 = fminf(a.y, b.y);
    const float x1 = fmaxf(a.x + a.w, b.x + b.w), y1 = fmaxf(a.y + a.h, b.y + b.h);
    return { x0, y0, x1 - x0, y1 - y0 };
}

// A command this big has its bounds worked out only when it is cheap to say they are not needed (see addCmd).
constexpr size_t kMergeScanMax = 4096;
} // namespace

bool DrawList::boundsOf(size_t from, size_t to, Rect& out) const
{
    if (to <= from || to > vertices_.size()) return false;
    float x0 = vertices_[from].x, y0 = vertices_[from].y, x1 = x0, y1 = y0;
    if (x0 - x0 != 0.0f || y0 - y0 != 0.0f) return false; // (a first vertex that is not a number: not known)
    for (size_t i = from + 1; i < to; ++i)
    {
        const float x = vertices_[i].x, y = vertices_[i].y;
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
    }
    out = { x0, y0, x1 - x0, y1 - y0 };
    return true;
}

void DrawList::addCmd(uint32_t indexOffset, uint32_t indexCount, TextureHandle texture, bool solid)
{
    const size_t from = newVertices_; // the vertices of this command (or of nothing, if it is dropped) are from..here
    newVertices_ = vertices_.size();
    // the bounds the shapes of this command noted, when they noted all of its vertices
    Rect nb = { 0, 0, 0, 0 };
    bool haveNb = noted_ > 0 && noted_ == newVertices_ - from;
    if (haveNb) nb = { pendBounds_[0], pendBounds_[1], pendBounds_[2] - pendBounds_[0], pendBounds_[3] - pendBounds_[1] };
    noted_ = 0;
    if (indexCount == 0) return;

    const Rect clip = clipStack_.empty() ? Rect{ 0.0f, 0.0f, 100000.0f, 100000.0f } : clipStack_.back().rect;
    if (clip.w <= 0.0f || clip.h <= 0.0f) return;
    const int roundCount = clipStack_.empty() ? 0 : clipStack_.back().roundCount;

    // The common case, a shape or a text like the one before it (same texture, same clip, no rounded clip): it is part of it.
    if (!commands_.empty())
    {
        DrawCmd& last = commands_.back();
        if (roundCount == 0 && last.roundClipCount == 0 && last.texture.value == texture.value && last.indexOffset + last.indexCount == indexOffset &&
                last.clip.x == clip.x && last.clip.y == clip.y && last.clip.w == clip.w && last.clip.h == clip.h)
        {
            last.indexCount += indexCount;
            if (lastBoundsValid_ && haveNb) lastBounds_ = rectUnion(lastBounds_, nb);
            else
                lastBoundsValid_ = false;
            lastSolidOnly_ = lastSolidOnly_ && solid;
            return;
        }
        // The next of a run under a clip that did not matter to the first of the run (see below): the same decision, for the
        // same clip, by the bounds alone.
        if (mergedClipValid_ && haveNb && roundCount == 0 && last.roundClipCount == 0 && last.texture.value == texture.value &&
                last.indexOffset + last.indexCount == indexOffset && mergedClip_.x == clip.x && mergedClip_.y == clip.y &&
                mergedClip_.w == clip.w && mergedClip_.h == clip.h && rectHolds(clip, nb) && rectHolds(last.clip, nb))
        {
            last.indexCount += indexCount;
            if (lastBoundsValid_) lastBounds_ = rectUnion(lastBounds_, nb);
            lastSolidOnly_ = lastSolidOnly_ && solid;
            return;
        }
    }
    const RoundClip* rounds = clipStack_.empty() ? nullptr : clipStack_.back().rounds;

    if (!commands_.empty())
    {
        DrawCmd& prev = commands_.back();
        bool sameTexture = (prev.texture.value == texture.value);
        if (!sameTexture && mergeCommands_ && altTexture_.value != 0)
        {
            // shapes have a white texel on both pages of the atlas: shapes that come after text of the other page ride on that
            // page, and the shapes before text of the other page move to it (they are shapes alone, whatever page they were put on)
            if (solid && texture.value == fontTexture_.value && prev.texture.value == altTexture_.value)
            {
                for (size_t i = from; i < newVertices_; ++i)
                {
                    vertices_[i].u = altUV_.x;
                    vertices_[i].v = altUV_.y;
                }
                texture = altTexture_;
                sameTexture = true;
            }
            else if (!solid && texture.value == altTexture_.value && lastSolidOnly_ && prev.texture.value == fontTexture_.value)
            {
                for (size_t i = lastStart_; i < from; ++i)
                {
                    vertices_[i].u = altUV_.x;
                    vertices_[i].v = altUV_.y;
                }
                prev.texture = altTexture_;
                sameTexture = true;
            }
        }
        const bool sameClip = (prev.clip.x == clip.x && prev.clip.y == clip.y && prev.clip.w == clip.w && prev.clip.h == clip.h);
        // Só faz merge se os índices forem mesmo contíguos
        const bool contiguous = (prev.indexOffset + prev.indexCount == indexOffset);

        // Commands under different rounded clips must stay apart: the shape is per command.
        bool sameRounds = prev.roundClipCount == roundCount;
        for (int i = 0; sameRounds && i < roundCount; ++i)
        {
            const RoundClip &a = prev.roundClips[i], &b = rounds[i];
            sameRounds = a.rect.x == b.rect.x && a.rect.y == b.rect.y && a.rect.w == b.rect.w &&
                         a.rect.h == b.rect.h && a.radii[0] == b.radii[0] &&
                         a.radii[1] == b.radii[1] && a.radii[2] == b.radii[2] &&
                         a.radii[3] == b.radii[3];
        }

        if (sameTexture && sameRounds && contiguous && (sameClip || mergeCommands_))
        {
            if (sameClip)
            {
                prev.indexCount += indexCount;
                if (lastBoundsValid_ && haveNb) lastBounds_ = rectUnion(lastBounds_, nb);
                else
                    lastBoundsValid_ = false; // (the vertices of the new part are not known)
                lastSolidOnly_ = lastSolidOnly_ && solid;
                return;
            }
            // A different clip: the two can be one command when the clip does not matter to what is drawn. The vertices of the new
            // command are looked at (and those of the one before, when they are not many, or already known).
            if (!haveNb && newVertices_ - from <= kMergeScanMax) haveNb = boundsOf(from, newVertices_, nb);
            if (haveNb)
            {
                const bool newClean = rectHolds(clip, nb);
                bool merged = false;
                Rect united = nb;
                bool unitedKnown = false;
                if (newClean && rectHolds(prev.clip, nb))
                {
                    merged = true; // the new vertices are inside both clips: the old clip serves
                    mergedClip_ = clip;
                    mergedClipValid_ = true;
                    if (lastBoundsValid_)
                    {
                        united = rectUnion(lastBounds_, nb);
                        unitedKnown = true;
                    }
                }
                else
                {
                    Rect pb = lastBounds_;
                    bool havePb = lastBoundsValid_;
                    if (!havePb && from > lastStart_ && from - lastStart_ <= kMergeScanMax) havePb = boundsOf(lastStart_, from, pb);
                    if (havePb && rectHolds(prev.clip, pb))
                    {
                        if (newClean)
                        {
                            prev.clip = rectUnion(prev.clip, clip); // both inside their own: any clip that holds both will do
                            merged = true;
                        }
                        else if (rectHolds(clip, pb))
                        {
                            prev.clip = clip; // the old vertices are inside the new clip: the new clip serves
                            merged = true;
                        }
                        if (merged)
                        {
                            united = rectUnion(pb, nb);
                            unitedKnown = true;
                        }
                    }
                }
                if (merged)
                {
                    prev.indexCount += indexCount;
                    lastBounds_ = united;
                    lastBoundsValid_ = unitedKnown;
                    lastSolidOnly_ = lastSolidOnly_ && solid;
                    return;
                }
            }
        }
    }

    lastStart_ = from;
    lastBounds_ = nb;
    lastBoundsValid_ = haveNb;
    lastSolidOnly_ = solid;
    mergedClipValid_ = false;
    DrawCmd command;
    command.texture = texture;
    command.clip = clip;
    command.indexOffset = indexOffset;
    command.indexCount = indexCount;
    command.roundClipCount = roundCount;
    for (int i = 0; i < roundCount; ++i) command.roundClips[i] = rounds[i];
    commands_.push_back(command);
}
namespace
{

struct ClipVertex
{
    float x, y, u, v, r, g, b, a;
};

// The outline of a rounded rectangle, a convex polygon, corners top-left, top-right, bottom-right,
// bottom-left, each arc flat to within a tenth of a pixel.
void roundClipPolygon(const RoundClip& clip, ct::Vector<Vec2>& out)
{
    const Rect& r = clip.rect;
    const Vec2 centres[4] = { { r.x + clip.radii[0], r.y + clip.radii[0] },
        { r.x + r.w - clip.radii[1], r.y + clip.radii[1] },
        { r.x + r.w - clip.radii[2], r.y + r.h - clip.radii[2] },
        { r.x + clip.radii[3], r.y + r.h - clip.radii[3] } };
    const float starts[4] = { Pi, Pi * 1.5f, 0.0f, Pi * 0.5f };
    for (int corner = 0; corner < 4; ++corner)
    {
        const float radius = clip.radii[corner];
        int segments = 1;
        if (radius > 0.0f)
        {
            const float step = 2.0f * std::acos(std::max(-1.0f, 1.0f - 0.1f / radius));
            segments = std::min(64,
                    std::max(2, static_cast<int>(std::ceil(Pi * 0.5f / std::max(step, 0.01f)))));
        }
        for (int i = 0; i <= segments; ++i)
        {
            const float angle = starts[corner] +
                                Pi * 0.5f * static_cast<float>(i) / static_cast<float>(segments);
            const Vec2 p = { centres[corner].x + radius * std::cos(angle),
                centres[corner].y + radius * std::sin(angle) };
            // Corners that meet share a point; a repeated one is a zero length edge.
            if (!out.empty() && std::fabs(out.back().x - p.x) < 0.001f &&
                    std::fabs(out.back().y - p.y) < 0.001f)
                continue;
            out.push_back(p);
        }
    }
}

// `subject` cut to the half plane left of (or on) the edge a->b, `sign` being the polygon's
// winding.
void clipHalfPlane(const ct::Vector<ClipVertex>& subject, Vec2 a, Vec2 b, float sign,
        ct::Vector<ClipVertex>& result)
{
    result.clear();
    const size_t count = subject.size();
    for (size_t i = 0; i < count; ++i)
    {
        const ClipVertex& cur = subject[i];
        const ClipVertex& prev = subject[(i + count - 1) % count];
        const float curSide = sign * ((b.x - a.x) * (cur.y - a.y) - (b.y - a.y) * (cur.x - a.x));
        const float prevSide = sign * ((b.x - a.x) * (prev.y - a.y) - (b.y - a.y) * (prev.x - a.x));
        if ((curSide >= 0.0f) != (prevSide >= 0.0f))
        {
            const float t = prevSide / (prevSide - curSide);
            ClipVertex cut;
            cut.x = prev.x + (cur.x - prev.x) * t;
            cut.y = prev.y + (cur.y - prev.y) * t;
            cut.u = prev.u + (cur.u - prev.u) * t;
            cut.v = prev.v + (cur.v - prev.v) * t;
            cut.r = prev.r + (cur.r - prev.r) * t;
            cut.g = prev.g + (cur.g - prev.g) * t;
            cut.b = prev.b + (cur.b - prev.b) * t;
            cut.a = prev.a + (cur.a - prev.a) * t;
            result.push_back(cut);
        }
        if (curSide >= 0.0f) result.push_back(cur);
    }
}

} // namespace

void ClipToRoundClips(const DrawVertex* vertices, size_t vertexCount, const uint32_t* indices,
        size_t indexCount, const DrawCmd& command, ClippedGeometry& out)
{
    if (command.indexOffset + command.indexCount > indexCount) return;
    ct::Vector<Vec2> polygons[DrawCmd::MaxRoundClips];
    float signs[DrawCmd::MaxRoundClips];
    Rect inner[DrawCmd::MaxRoundClips]; // the rectangle no corner reaches into
    const int clips = std::min<int>(command.roundClipCount, DrawCmd::MaxRoundClips);
    for (int c = 0; c < clips; ++c)
    {
        const RoundClip& clip = command.roundClips[c];
        roundClipPolygon(clip, polygons[c]);
        float area = 0.0f;
        const ct::Vector<Vec2>& pts = polygons[c];
        for (size_t i = 0; i < pts.size(); ++i)
        {
            const Vec2 &a = pts[i], &b = pts[(i + 1) % pts.size()];
            area += a.x * b.y - b.x * a.y;
        }
        signs[c] = area >= 0.0f ? 1.0f : -1.0f;
        const float top = std::max(clip.radii[0], clip.radii[1]);
        const float bottom = std::max(clip.radii[2], clip.radii[3]);
        const float left = std::max(clip.radii[0], clip.radii[3]);
        const float right = std::max(clip.radii[1], clip.radii[2]);
        inner[c] = { clip.rect.x + left, clip.rect.y + top,
            std::max(0.0f, clip.rect.w - left - right),
            std::max(0.0f, clip.rect.h - top - bottom) };
        if (inner[c].w <= 0.0f || inner[c].h <= 0.0f) inner[c] = { 0.0f, 0.0f, 0.0f, 0.0f };
    }

    ct::Vector<ClipVertex> polygon, scratch;
    for (uint32_t i = 0; i + 2 < command.indexCount; i += 3)
    {
        const uint32_t ids[3] = { indices[command.indexOffset + i],
            indices[command.indexOffset + i + 1], indices[command.indexOffset + i + 2] };
        if (ids[0] >= vertexCount || ids[1] >= vertexCount || ids[2] >= vertexCount) continue;
        const DrawVertex* tri[3] = { &vertices[ids[0]], &vertices[ids[1]], &vertices[ids[2]] };
        const float minX = std::min(tri[0]->x, std::min(tri[1]->x, tri[2]->x));
        const float maxX = std::max(tri[0]->x, std::max(tri[1]->x, tri[2]->x));
        const float minY = std::min(tri[0]->y, std::min(tri[1]->y, tri[2]->y));
        const float maxY = std::max(tri[0]->y, std::max(tri[1]->y, tri[2]->y));

        bool outside = false, inside = true;
        for (int c = 0; c < clips; ++c)
        {
            const Rect& r = command.roundClips[c].rect;
            if (maxX <= r.x || minX >= r.x + r.w || maxY <= r.y || minY >= r.y + r.h)
                outside = true;
            if (!(minX >= inner[c].x && maxX <= inner[c].x + inner[c].w && minY >= inner[c].y &&
                        maxY <= inner[c].y + inner[c].h))
                inside = false;
        }
        if (outside) continue;
        if (inside)
        {
            const uint32_t base = static_cast<uint32_t>(out.vertices.size());
            for (int k = 0; k < 3; ++k)
            {
                out.vertices.push_back(*tri[k]);
                out.indices.push_back(base + static_cast<uint32_t>(k));
            }
            continue;
        }

        polygon.clear();
        for (int k = 0; k < 3; ++k)
            polygon.push_back({ tri[k]->x, tri[k]->y, tri[k]->u, tri[k]->v,
                static_cast<float>(tri[k]->color.r), static_cast<float>(tri[k]->color.g),
                static_cast<float>(tri[k]->color.b), static_cast<float>(tri[k]->color.a) });
        for (int c = 0; c < clips && polygon.size() >= 3; ++c)
        {
            const ct::Vector<Vec2>& pts = polygons[c];
            for (size_t e = 0; e < pts.size() && polygon.size() >= 3; ++e)
            {
                clipHalfPlane(polygon, pts[e], pts[(e + 1) % pts.size()], signs[c], scratch);
                polygon = scratch;
            }
        }
        if (polygon.size() < 3) continue;
        const uint32_t base = static_cast<uint32_t>(out.vertices.size());
        for (size_t k = 0; k < polygon.size(); ++k)
        {
            const ClipVertex& cv = polygon[k];
            auto byte = [](float value)
            { return static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, value + 0.5f))); };
            DrawVertex vertex;
            vertex.x = cv.x;
            vertex.y = cv.y;
            vertex.u = cv.u;
            vertex.v = cv.v;
            vertex.color = Color(byte(cv.r), byte(cv.g), byte(cv.b), byte(cv.a));
            out.vertices.push_back(vertex);
        }
        for (size_t k = 1; k + 1 < polygon.size(); ++k)
        {
            out.indices.push_back(base);
            out.indices.push_back(base + static_cast<uint32_t>(k));
            out.indices.push_back(base + static_cast<uint32_t>(k + 1));
        }
    }
}

// Room for `count` more indices, for the caller to write: one growth check for all of them instead
// of one for each (a frame writes twenty thousand).
DrawVertex* DrawList::appendVertices(size_t count)
{
    const size_t at = vertices_.size();
    vertices_.resize(at + count);
    return vertices_.data() + at;
}

uint32_t* DrawList::appendIndices(size_t count)
{
    const size_t at = indices_.size();
    indices_.resize(at + count);
    return indices_.data() + at;
}

uint32_t DrawList::addVertex(float x, float y, const Color& color)
{
    return addVertex(x, y, whiteUV_.x, whiteUV_.y, color);
}

uint32_t DrawList::addVertex(float x, float y, float u, float v, const Color& color)
{
    uint32_t index = static_cast<uint32_t>(vertices_.size());
    const Vec2 q = mapPoint(x, y);
    vertices_.push_back({ q.x, q.y, u, v, color });
    return index;
}

Context* CreateContext()
{
    return new Context();
}

void DestroyContext(Context* ctx)
{
    if (gCurrentContext == ctx) gCurrentContext = nullptr;
    // The manager caches the draw list of the last paint to measure text with.
    if (ctx)
        if (WidgetApp* app = WidgetApp::existing()) app->forgetDrawList(&ctx->drawList);
    delete ctx;
}

void SetCurrentContext(Context* ctx)
{
    gCurrentContext = ctx;
}

Context* GetCurrentContext()
{
    return gCurrentContext;
}

IO& GetIO()
{
    return current().io;
}

DrawList& GetDrawList()
{
    return current().drawList;
}

DrawList& GetTransDrawList()
{
    return current().transDrawList;
}

DrawList& GetOverlayDrawList()
{
    return current().overlayList;
}

void SetSolidAlternate(TextureHandle texture, Vec2 whiteUV)
{
    current().drawList.setSolidAlternate(texture, whiteUV);
    current().transDrawList.setSolidAlternate(texture, whiteUV);
    current().overlayList.setSolidAlternate(texture, whiteUV);
}

void SetWhitePixel(TextureHandle atlas, Vec2 whiteUV)
{
    current().drawList.setWhitePixel(atlas, whiteUV);
    current().transDrawList.setWhitePixel(atlas, whiteUV);
    current().overlayList.setWhitePixel(atlas, whiteUV);
}

void NewFrame()
{
    auto& ctx = current();
    ctx.drawList.clear();
    ctx.transDrawList.clear();
    ctx.overlayList.clear();
    ctx.drawData.passes.clear();
}

void Render()
{
    auto& ctx = current();
    ctx.io.mouseWheelX = 0.0f;
    ctx.io.mouseWheelY = 0.0f;
    ctx.io.inputChars.clear();
    ctx.io.keyEvents.clear();
    ctx.io.mouseEvents.clear();
    ctx.io.dropEvents.clear();
    // passes are already populated by WidgetApp::paint();
    // append the overlay as the last pass (identity camera, always on top).
    if (!ctx.overlayList.commands().empty())
        ctx.drawData.passes.push_back({ &ctx.overlayList, Camera2D{} });
    ctx.drawData.displayWidth = ctx.io.displayWidth;
    ctx.drawData.displayHeight = ctx.io.displayHeight;
    ctx.drawData.framebufferScaleX = ctx.io.framebufferScaleX;
    ctx.drawData.framebufferScaleY = ctx.io.framebufferScaleY;
}

DrawData* GetDrawData()
{
    return &current().drawData;
}

} // namespace retained
} // namespace ig
