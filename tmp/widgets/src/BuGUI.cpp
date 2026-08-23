#include "pch.hpp"

// ── Color static constants ────────────────────────────────────────────────────
const Color Color::WHITE      (255, 255, 255);
const Color Color::GRAY       (128, 128, 128);
const Color Color::BLACK      (  0,   0,   0);
const Color Color::RED        (255,   0,   0);
const Color Color::GREEN      (  0, 255,   0);
const Color Color::BLUE       (  0,   0, 255);
const Color Color::CYAN       (  0, 255, 255);
const Color Color::MAGENTA    (255,   0, 255);
const Color Color::YELLOW     (255, 255,   0);
const Color Color::TRANSPARENT(  0,   0,   0, 0);

#include "DejaVuSans_embedded.h"
#include "stb_truetype.h"

namespace BuGUI
{

    struct Context
    {
        IO       io;
        DrawList drawList;       // current / new-stage list
        DrawList transDrawList;  // old-stage list (valid during transitions)
        DrawList overlayList;    // always rendered last, identity camera
        DrawData drawData;
    };

    namespace
    {

        Context *gCurrentContext = nullptr;
        constexpr float Pi = 3.14159265358979323846f;

        Context &current()
        {
            if (!gCurrentContext)
                gCurrentContext = CreateContext();
            return *gCurrentContext;
        }

        int saneSegments(int segments)
        {
            return std::max(3, segments);
        }

        bool intersectRects(const Rect &a, const Rect &b, Rect &out)
        {
            float x0 = std::max(a.x, b.x);
            float y0 = std::max(a.y, b.y);
            float x1 = std::min(a.x + a.w, b.x + b.w);
            float y1 = std::min(a.y + a.h, b.y + b.h);

            if (x1 <= x0 || y1 <= y0)
            {
                out = {x0, y0, 0.0f, 0.0f};
                return false;
            }

            out = {x0, y0, x1 - x0, y1 - y0};
            return true;
        }

        // ── Arc lookup table (ImGui PathArcToFast-style) ──────────────────
        // 12 samples: index i → angle i * 30°.  Index 12 wraps to 0.
        // Y is positive-downward (screen coords).
        constexpr float arcFastX[13] = {
             1.00000f,  0.86603f,  0.50000f,  0.00000f,
            -0.50000f, -0.86603f, -1.00000f, -0.86603f,
            -0.50000f,  0.00000f,  0.50000f,  0.86603f,
             1.00000f   // [12] == [0], convenient for closed ranges
        };
        constexpr float arcFastY[13] = {
             0.00000f,  0.50000f,  0.86603f,  1.00000f,
             0.86603f,  0.50000f,  0.00000f, -0.50000f,
            -0.86603f, -1.00000f, -0.86603f, -0.50000f,
             0.00000f
        };

        uint32_t decodeUtf8(const char *&text)
        {
            const unsigned char *s = reinterpret_cast<const unsigned char *>(text);

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
            if ((s[0] & 0xF0) == 0xE0 &&
                s[1] != '\0' && (s[1] & 0xC0) == 0x80 &&
                s[2] != '\0' && (s[2] & 0xC0) == 0x80)
            {
                text += 3;
                return static_cast<uint32_t>(((s[0] & 0x0F) << 12) |
                                             ((s[1] & 0x3F) << 6) |
                                             (s[2] & 0x3F));
            }

            // 4-byte sequence
            if ((s[0] & 0xF8) == 0xF0 &&
                s[1] != '\0' && (s[1] & 0xC0) == 0x80 &&
                s[2] != '\0' && (s[2] & 0xC0) == 0x80 &&
                s[3] != '\0' && (s[3] & 0xC0) == 0x80)
            {
                text += 4;
                return static_cast<uint32_t>(((s[0] & 0x07) << 18) |
                                             ((s[1] & 0x3F) << 12) |
                                             ((s[2] & 0x3F) << 6) |
                                             (s[3] & 0x3F));
            }

            ++text;
            return 0xFFFD;
        }

    } // namespace

    const FontGlyph *Font::findGlyph(uint32_t codepoint) const
    {
        auto it = glyphs_.find(codepoint);
        if (it != glyphs_.end())
            return &it->second;

        it = glyphs_.find('?');
        if (it != glyphs_.end())
            return &it->second;

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
        cfg.ttfData     = nullptr;  // signals "use embedded DejaVuSans"
        cfg.ttfSize     = 0;
        cfg.pixelHeight = pixelHeight;
        return addFont(cfg);
    }

    bool FontAtlas::build()
    {
        if (pending_.empty()) return false;

        width_  = 2048;
        height_ = 1024;
        std::vector<unsigned char> alpha(static_cast<size_t>(width_ * height_), 0);

        stbtt_pack_context pack = {};
        if (!stbtt_PackBegin(&pack, alpha.data(), width_, height_, 0, 1, nullptr))
            return false;

        stbtt_PackSetOversampling(&pack, 2, 2);
        stbtt_PackSetSkipMissingCodepoints(&pack, 1);

        // ── Codepoint ranges (shared by all fonts) ───────────────────────
        struct Range {
            int first;
            int count;
        };
        const Range kRanges[] = {
            {0x0020, 224}, {0x2000, 112}, {0x2100,  80}, {0x2190, 112},
            {0x2200, 256}, {0x2300, 256}, {0x2500, 128}, {0x2580,  32},
            {0x25A0,  96}, {0x2600, 256}, {0x2700, 192},
        };
        constexpr int kRangeCount = sizeof(kRanges) / sizeof(kRanges[0]);

        // Per-font packed char data
        struct FontPack {
            const unsigned char* data;
            int   dataSize;
            float pixelHeight;
            std::vector<std::vector<stbtt_packedchar>> rangePacked; // [range][char]
        };
        std::vector<FontPack> packs(pending_.size());

        // Build stb ranges for all fonts
        std::vector<stbtt_pack_range> allRanges;
        for (size_t fi = 0; fi < pending_.size(); ++fi) {
            auto& cfg = pending_[fi];
            auto& fp  = packs[fi];
            fp.data       = cfg.ttfData ? cfg.ttfData : DejaVuSans_ttf_data;
            fp.dataSize   = cfg.ttfData ? cfg.ttfSize : static_cast<int>(DejaVuSans_ttf_size);
            fp.pixelHeight= cfg.pixelHeight;
            fp.rangePacked.resize(kRangeCount);
            for (int ri = 0; ri < kRangeCount; ++ri) {
                fp.rangePacked[ri].resize(static_cast<size_t>(kRanges[ri].count));
                stbtt_pack_range pr{};
                pr.font_size                        = fp.pixelHeight;
                pr.first_unicode_codepoint_in_range = kRanges[ri].first;
                pr.num_chars                        = kRanges[ri].count;
                pr.chardata_for_range               = fp.rangePacked[ri].data();
                pr.array_of_unicode_codepoints      = nullptr;
                allRanges.push_back(pr);
            }
            // For multiple fonts from different TTFs, pack each separately
        }

        // Pack fonts one by one (stb_truetype packs per-font)
        size_t rangeIdx = 0;
        for (size_t fi = 0; fi < packs.size(); ++fi) {
            stbtt_PackFontRanges(&pack, packs[fi].data, 0,
                                 &allRanges[rangeIdx], kRangeCount);
            rangeIdx += kRangeCount;
        }

        stbtt_PackEnd(&pack);

        // Convert alpha → RGBA
        pixels_.assign(static_cast<size_t>(width_ * height_ * 4), 0);
        for (int i = 0; i < width_ * height_; ++i) {
            size_t dst = static_cast<size_t>(i) * 4;
            pixels_[dst + 0] = 255;
            pixels_[dst + 1] = 255;
            pixels_[dst + 2] = 255;
            pixels_[dst + 3] = alpha[static_cast<size_t>(i)];
        }

        // White pixel at [0,0]
        pixels_[0] = pixels_[1] = pixels_[2] = pixels_[3] = 255;
        whitePixelUV_ = {0.5f / static_cast<float>(width_),
                         0.5f / static_cast<float>(height_)};

        // Build Font objects
        fonts_.clear();
        fonts_.resize(packs.size());
        for (size_t fi = 0; fi < packs.size(); ++fi) {
            auto& fp = packs[fi];
            auto& fnt = fonts_[fi];
            fnt.texture_ = texture_;

            stbtt_fontinfo info = {};
            if (!stbtt_InitFont(&info, fp.data, stbtt_GetFontOffsetForIndex(fp.data, 0)))
                continue;

            float scale = stbtt_ScaleForPixelHeight(&info, fp.pixelHeight);
            int ascent = 0, descent = 0, lineGap = 0;
            stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
            fnt.lineHeight_ = static_cast<float>(ascent - descent + lineGap) * scale;
            fnt.ascender_   = static_cast<float>(ascent) * scale;

            for (int ri = 0; ri < kRangeCount; ++ri) {
                for (int ci = 0; ci < kRanges[ri].count; ++ci) {
                    const stbtt_packedchar& pc = fp.rangePacked[ri][static_cast<size_t>(ci)];
                    uint32_t cp = static_cast<uint32_t>(kRanges[ri].first + ci);
                    if (pc.xadvance <= 0.0f && cp != ' ') continue;
                    FontGlyph glyph;
                    glyph.codepoint = cp;
                    glyph.uv = {
                        static_cast<float>(pc.x0) / static_cast<float>(width_),
                        static_cast<float>(pc.y0) / static_cast<float>(height_),
                        static_cast<float>(pc.x1 - pc.x0) / static_cast<float>(width_),
                        static_cast<float>(pc.y1 - pc.y0) / static_cast<float>(height_)};
                    glyph.size     = {pc.xoff2 - pc.xoff, pc.yoff2 - pc.yoff};
                    glyph.offset   = {pc.xoff, pc.yoff};
                    glyph.advanceX = pc.xadvance;
                    fnt.glyphs_[cp] = glyph;
                }
            }
        }

        pending_.clear();
        return !fonts_.empty() && !fonts_[0].glyphs_.empty();
    }

    // ── Legacy single-font API ───────────────────────────────────────────

    bool FontAtlas::buildDefault()
    {
        pending_.clear();
        fonts_.clear();
        addFontDefault(16.0f);
        return build();
    }

    bool FontAtlas::buildFromTTFMemory(const unsigned char *data, int dataSize, float pixelHeight)
    {
        if (!data || dataSize <= 0 || pixelHeight <= 0.0f)
            return false;
        pending_.clear();
        fonts_.clear();
        FontConfig cfg;
        cfg.ttfData     = data;
        cfg.ttfSize     = dataSize;
        cfg.pixelHeight = pixelHeight;
        addFont(cfg);
        return build();
    }

    void FontAtlas::setTexture(TextureHandle texture)
    {
        texture_ = texture;
        for (auto& f : fonts_)
            f.texture_ = texture;
    }

    void DrawList::clear()
    {
        clipStack_.clear();
        path_.clear();
        vertices_.clear();
        indices_.clear();
        commands_.clear();
        pushClipCount_ = 0;
    }

    void DrawList::pushClip(const Rect &rect)
    {
        Rect clipped = rect;
        if (!clipStack_.empty())
            intersectRects(clipStack_.back(), rect, clipped);
        clipStack_.push_back(clipped);
        ++pushClipCount_;
    }

    void DrawList::popClip()
    {
        if (!clipStack_.empty())
            clipStack_.pop_back();
    }

    void DrawList::setWhitePixel(TextureHandle atlas, Vec2 whiteUV)
    {
        fontTexture_ = atlas;
        whiteUV_     = whiteUV;
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
        if (radius < 0.5f) { path_.push_back(center); return; }
        path_.reserve(path_.size() + static_cast<size_t>(aMaxOf12 - aMinOf12 + 1));
        for (int i = aMinOf12; i <= aMaxOf12; ++i)
        {
            int k = i % 12;
            path_.push_back({ center.x + arcFastX[k] * radius,
                              center.y + arcFastY[k] * radius });
        }
    }

    void DrawList::pathFillConvex(const Color &color)
    {
        int n = static_cast<int>(path_.size());
        if (n < 3) { path_.clear(); return; }

        uint32_t idxCount = static_cast<uint32_t>((n - 2) * 3);
        vertices_.reserve(vertices_.size() + static_cast<size_t>(n));
        indices_.reserve(indices_.size() + idxCount);

        uint32_t offset = static_cast<uint32_t>(indices_.size());
        uint32_t base   = static_cast<uint32_t>(vertices_.size());

        for (const auto &p : path_)
            vertices_.push_back({p.x, p.y, whiteUV_.x, whiteUV_.y, color});

        for (uint32_t i = 2; i < static_cast<uint32_t>(n); ++i)
        {
            indices_.push_back(base);
            indices_.push_back(base + i - 1);
            indices_.push_back(base + i);
        }

        addCmd(offset, idxCount, fontTexture_);
        path_.clear();
    }

    void DrawList::pathStroke(const Color &color, float thickness, bool closed)
    {
        int n = static_cast<int>(path_.size());
        if (n < 2) { path_.clear(); return; }
        thickness = std::max(1.0f, thickness);

        int segs = closed ? n : n - 1;
        uint32_t vtxCount = static_cast<uint32_t>(n * 2);
        uint32_t idxCount = static_cast<uint32_t>(segs * 6);
        vertices_.reserve(vertices_.size() + vtxCount);
        indices_.reserve(indices_.size() + idxCount);

        uint32_t offset = static_cast<uint32_t>(indices_.size());

        // Miter-joined outline — IM_FIXNORMAL2F style (no extra sqrt, capped)
        std::vector<uint32_t> vL(static_cast<size_t>(n));
        std::vector<uint32_t> vR(static_cast<size_t>(n));

        for (int i = 0; i < n; ++i)
        {
            const Vec2 &p = path_[i];
            int iPrev = closed ? ((i - 1 + n) % n) : std::max(0, i - 1);
            int iNext = closed ? ((i + 1) % n)      : std::min(n - 1, i + 1);

            // Incoming tangent (prev→curr), then left normal
            float dx1 = p.x - path_[iPrev].x, dy1 = p.y - path_[iPrev].y;
            float l1 = std::sqrt(dx1*dx1 + dy1*dy1);
            if (l1 > 0.0001f) { dx1 /= l1; dy1 /= l1; }
            float nx1 = -dy1, ny1 = dx1;

            // Outgoing tangent (curr→next), then left normal
            float dx2 = path_[iNext].x - p.x, dy2 = path_[iNext].y - p.y;
            float l2 = std::sqrt(dx2*dx2 + dy2*dy2);
            if (l2 > 0.0001f) { dx2 /= l2; dy2 /= l2; }
            float nx2 = -dy2, ny2 = dx2;

            // Bisector miter: (n1+n2) / |n1+n2|^2  (IM_FIXNORMAL2F pattern)
            float mx = nx1 + nx2, my = ny1 + ny2;
            float d2 = mx*mx + my*my;
            if (d2 < 0.000001f) { mx = nx1; my = ny1; d2 = 1.0f; }
            float inv2 = 1.0f / d2;
            if (inv2 > 100.0f) inv2 = 100.0f; // miter limit
            mx *= inv2 * thickness;
            my *= inv2 * thickness;

            vL[i] = static_cast<uint32_t>(vertices_.size());
            vertices_.push_back({p.x + mx, p.y + my, whiteUV_.x, whiteUV_.y, color});
            vR[i] = static_cast<uint32_t>(vertices_.size());
            vertices_.push_back({p.x - mx, p.y - my, whiteUV_.x, whiteUV_.y, color});
        }

        for (int i = 0; i < segs; ++i)
        {
            int j = (i + 1) % n;
            indices_.insert(indices_.end(),
                {vL[i], vL[j], vR[j], vL[i], vR[j], vR[i]});
        }

        addCmd(offset, idxCount, fontTexture_);
        path_.clear();
    }

    void DrawList::addRect(const Rect &rect, const Color &color)
    {
        uint32_t offset = static_cast<uint32_t>(indices_.size());
        uint32_t v0 = addVertex(rect.x, rect.y, color);
        uint32_t v1 = addVertex(rect.x + rect.w, rect.y, color);
        uint32_t v2 = addVertex(rect.x + rect.w, rect.y + rect.h, color);
        uint32_t v3 = addVertex(rect.x, rect.y + rect.h, color);

        indices_.insert(indices_.end(), {v0, v1, v2, v0, v2, v3});
        addCmd(offset, 6, fontTexture_);
    }
    void DrawList::addRectOutline(const Rect &rect, const Color &color, float thickness)
    {
        thickness = std::max(1.0f, thickness);

        addRect({rect.x, rect.y, rect.w, thickness}, color);
        addRect({rect.x, rect.y + rect.h - thickness, rect.w, thickness}, color);

        addRect({rect.x, rect.y + thickness,
                 thickness, rect.h - thickness * 2.0f},
                color);
        addRect({rect.x + rect.w - thickness, rect.y + thickness,
                 thickness, rect.h - thickness * 2.0f},
                color);
    }
    void DrawList::addRoundRectFilled(const Rect &rect, float radius, const Color &color, int /*segments*/)
    {
        if (rect.w <= 0.0f || rect.h <= 0.0f)
            return;
        radius = std::max(0.0f, std::min(radius, std::min(rect.w, rect.h) * 0.5f));

        if (radius < 0.5f) { addRect(rect, color); return; }

        float x0 = rect.x, y0 = rect.y, x1 = rect.x + rect.w, y1 = rect.y + rect.h;
        // Each arc: 4 points (index range covers 90° in 3 × 30° steps + endpoint)
        //  TR: 270°→360°  BR: 0°→90°  BL: 90°→180°  TL: 180°→270°
        pathArcToFast({x1 - radius, y0 + radius}, radius, 9, 12);
        pathArcToFast({x1 - radius, y1 - radius}, radius, 0,  3);
        pathArcToFast({x0 + radius, y1 - radius}, radius, 3,  6);
        pathArcToFast({x0 + radius, y0 + radius}, radius, 6,  9);
        pathFillConvex(color);
    }

    void DrawList::addRoundRect(const Rect &rect, float radius, const Color &color, float thickness, int /*segments*/)
    {
        if (rect.w <= 0.0f || rect.h <= 0.0f)
            return;
        // Inset by half the stroke thickness so the stroke centerline sits on
        // the fill boundary rather than half-outside, half-inside.
        float inset = thickness * 0.5f;
        Rect r2 = {rect.x + inset, rect.y + inset, rect.w - inset * 2.0f, rect.h - inset * 2.0f};
        radius = std::max(0.0f, std::min(radius, std::min(r2.w, r2.h) * 0.5f));

        if (radius < 0.5f) { addRectOutline(rect, color, thickness); return; }

        float x0 = r2.x, y0 = r2.y, x1 = r2.x + r2.w, y1 = r2.y + r2.h;
        pathArcToFast({x1 - radius, y0 + radius}, radius, 9, 12);
        pathArcToFast({x1 - radius, y1 - radius}, radius, 0,  3);
        pathArcToFast({x0 + radius, y1 - radius}, radius, 3,  6);
        pathArcToFast({x0 + radius, y0 + radius}, radius, 6,  9);
        pathStroke(color, thickness, true);
    }

    void DrawList::addCircleFilled(Vec2 center, float radius, const Color &color, int segments)
    {
        if (radius < 0.5f) return;
        segments = saneSegments(segments);
        path_.reserve(static_cast<size_t>(segments));
        for (int i = 0; i < segments; ++i)
        {
            float a = (static_cast<float>(i) / static_cast<float>(segments)) * Pi * 2.0f;
            path_.push_back({center.x + std::cos(a) * radius,
                             center.y + std::sin(a) * radius});
        }
        pathFillConvex(color);
    }

    void DrawList::addCircle(Vec2 center, float radius, const Color &color, float thickness, int segments)
    {
        if (radius < 0.5f) return;
        segments = saneSegments(segments);
        path_.reserve(static_cast<size_t>(segments));
        for (int i = 0; i < segments; ++i)
        {
            float a = (static_cast<float>(i) / static_cast<float>(segments)) * Pi * 2.0f;
            path_.push_back({center.x + std::cos(a) * radius,
                             center.y + std::sin(a) * radius});
        }
        pathStroke(color, thickness, true);
    }

    void DrawList::addTriangleFilled(Vec2 a, Vec2 b, Vec2 c, const Color &color)
    {
        uint32_t offset = static_cast<uint32_t>(indices_.size());
        uint32_t v0 = addVertex(a.x, a.y, color);
        uint32_t v1 = addVertex(b.x, b.y, color);
        uint32_t v2 = addVertex(c.x, c.y, color);

        indices_.insert(indices_.end(), {v0, v1, v2});
        addCmd(offset, 3, fontTexture_);
    }

    void DrawList::addTriangle(Vec2 a, Vec2 b, Vec2 c, const Color &color, float thickness)
    {
        addPolyline({a, b, c}, color, thickness, true);
    }

    void DrawList::addImage(TextureHandle texture, const Rect &rect, const Rect &uv, const Color &color)
    {
        if (!texture || rect.w <= 0.0f || rect.h <= 0.0f)
            return;

        uint32_t offset = static_cast<uint32_t>(indices_.size());
        uint32_t v0 = addVertex(rect.x, rect.y, uv.x, uv.y, color);
        uint32_t v1 = addVertex(rect.x + rect.w, rect.y, uv.x + uv.w, uv.y, color);
        uint32_t v2 = addVertex(rect.x + rect.w, rect.y + rect.h, uv.x + uv.w, uv.y + uv.h, color);
        uint32_t v3 = addVertex(rect.x, rect.y + rect.h, uv.x, uv.y + uv.h, color);

        indices_.insert(indices_.end(), {v0, v1, v2, v0, v2, v3});
        addCmd(offset, 6, texture);
    }

    void DrawList::addText(const Font &font, Vec2 pos, const Color &color, const char *text, float scale)
    {
        if (!font.texture() || !text)
            return;

        uint32_t offset = static_cast<uint32_t>(indices_.size());
        Vec2 pen = pos;

        while (*text)
        {
            uint32_t codepoint = decodeUtf8(text);
            if (codepoint == '\n')
            {
                pen.x = pos.x;
                pen.y += font.lineHeight() * scale;
                continue;
            }

            const FontGlyph *glyph = font.findGlyph(codepoint);
            if (!glyph)
                continue;

            if (glyph->size.x > 0.0f && glyph->size.y > 0.0f)
            {
                Rect rect{
                    pen.x + glyph->offset.x * scale,
                    pen.y + glyph->offset.y * scale,
                    glyph->size.x * scale,
                    glyph->size.y * scale};
                Rect uv = glyph->uv;

                uint32_t v0 = addVertex(rect.x, rect.y, uv.x, uv.y, color);
                uint32_t v1 = addVertex(rect.x + rect.w, rect.y, uv.x + uv.w, uv.y, color);
                uint32_t v2 = addVertex(rect.x + rect.w, rect.y + rect.h, uv.x + uv.w, uv.y + uv.h, color);
                uint32_t v3 = addVertex(rect.x, rect.y + rect.h, uv.x, uv.y + uv.h, color);
                indices_.insert(indices_.end(), {v0, v1, v2, v0, v2, v3});
            }

            pen.x += glyph->advanceX * scale;
        }

        addCmd(offset, static_cast<uint32_t>(indices_.size()) - offset, font.texture());
    }

    Vec2 DrawList::calcTextSize(const Font &font, const char *text, float scale)
    {
        if (!text)
            return {0.0f, 0.0f};

        float maxLineWidth = 0.0f;
        float lineWidth = 0.0f;
        int lines = 1;
        const char *cursor = text;

        while (*cursor)
        {
            uint32_t codepoint = decodeUtf8(cursor);
            if (codepoint == '\n')
            {
                maxLineWidth = std::max(maxLineWidth, lineWidth);
                lineWidth = 0.0f;
                ++lines;
                continue;
            }

            const FontGlyph *glyph = font.findGlyph(codepoint);
            if (glyph)
                lineWidth += glyph->advanceX * scale;
        }

        maxLineWidth = std::max(maxLineWidth, lineWidth);
        float height = font.lineHeight() * scale * static_cast<float>(lines);

        return {maxLineWidth, height};
    }

    void DrawList::addTextAligned(const Font &font,
                                  const Rect &bounds,
                                  const Color &color,
                                  const char *text,
                                  AlignX ax,
                                  AlignY ay)
    {
        if (!font.texture() || !text)
            return;

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

        addText(font, {x, y}, color, text);
    }

    void DrawList::addIcon(IconId icon, const Rect &rect, const Color &color, float thickness)
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
            addLine({x0 + p, cy}, {cx - s * 0.08f, y1 - p}, color, thickness);
            addLine({cx - s * 0.08f, y1 - p}, {x1 - p, y0 + p}, color, thickness);
            break;

        case IconId::Cross:
            addLine({x0 + p, y0 + p}, {x1 - p, y1 - p}, color, thickness);
            addLine({x1 - p, y0 + p}, {x0 + p, y1 - p}, color, thickness);
            break;

        case IconId::Plus:
            addLine({cx, y0 + p}, {cx, y1 - p}, color, thickness);
            addLine({x0 + p, cy}, {x1 - p, cy}, color, thickness);
            break;

        case IconId::Minus:
            addLine({x0 + p, cy}, {x1 - p, cy}, color, thickness);
            break;

        case IconId::ArrowRight:
            addTriangleFilled({x1 - p, cy}, {x0 + p, y0 + p}, {x0 + p, y1 - p}, color);
            break;

        case IconId::ArrowDown:
            addTriangleFilled({cx, y1 - p}, {x0 + p, y0 + p}, {x1 - p, y0 + p}, color);
            break;

        case IconId::Search:
            addCircle({cx - s * 0.08f, cy - s * 0.08f}, s * 0.23f, color, thickness);
            addLine({cx + s * 0.12f, cy + s * 0.12f}, {x1 - p, y1 - p}, color, thickness);
            break;

        case IconId::None:
            break;
        }
    }

    void DrawList::addLine(Vec2 a, Vec2 b, const Color &color, float thickness)
    {
        thickness = std::max(1.0f, thickness);
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len <= 0.0001f)
            return;

        float nx = -dy / len * thickness * 0.5f;
        float ny = dx / len * thickness * 0.5f;

        uint32_t offset = static_cast<uint32_t>(indices_.size());
        uint32_t v0 = addVertex(a.x + nx, a.y + ny, color);
        uint32_t v1 = addVertex(b.x + nx, b.y + ny, color);
        uint32_t v2 = addVertex(b.x - nx, b.y - ny, color);
        uint32_t v3 = addVertex(a.x - nx, a.y - ny, color);

        indices_.insert(indices_.end(), {v0, v1, v2, v0, v2, v3});
        addCmd(offset, 6, fontTexture_);
    }

    void DrawList::addConvexPolyFilled(const std::vector<Vec2> &points, const Color &color)
    {
        path_.insert(path_.end(), points.begin(), points.end());
        pathFillConvex(color);
    }

    void DrawList::addPolyline(const std::vector<Vec2> &points,
                               const Color &color, float thickness, bool closed)
    {
        path_.insert(path_.end(), points.begin(), points.end());
        pathStroke(color, thickness, closed);
    }

    Rect DrawList::currentClip() const
    {
        if (!clipStack_.empty())
            return clipStack_.back();
        return {0.0f, 0.0f, 100000.0f, 100000.0f};
    }

    void DrawList::addCmd(uint32_t indexOffset, uint32_t indexCount,
                          TextureHandle texture)
    {
        if (indexCount == 0)
            return;

        Rect clip = currentClip();
        if (clip.w <= 0.0f || clip.h <= 0.0f)
            return;

        if (!commands_.empty())
        {
            DrawCmd &prev = commands_.back();
            const bool sameTexture = (prev.texture.value == texture.value);
            const bool sameClip = (prev.clip.x == clip.x &&
                                   prev.clip.y == clip.y &&
                                   prev.clip.w == clip.w &&
                                   prev.clip.h == clip.h);
            // Só faz merge se os índices forem mesmo contíguos
            const bool contiguous = (prev.indexOffset + prev.indexCount == indexOffset);

            if (sameTexture && sameClip && contiguous)
            {
                prev.indexCount += indexCount;
                return;
            }
        }

        commands_.push_back({texture, clip, indexOffset, indexCount});
    }
    uint32_t DrawList::addVertex(float x, float y, const Color &color)
    {
        return addVertex(x, y, whiteUV_.x, whiteUV_.y, color);
    }

    uint32_t DrawList::addVertex(float x, float y, float u, float v, const Color &color)
    {
        uint32_t index = static_cast<uint32_t>(vertices_.size());
        vertices_.push_back({x, y, u, v, color});
        return index;
    }

    Context *CreateContext()
    {
        return new Context();
    }

    void DestroyContext(Context *ctx)
    {
        if (gCurrentContext == ctx)
            gCurrentContext = nullptr;
        delete ctx;
    }

    void SetCurrentContext(Context *ctx)
    {
        gCurrentContext = ctx;
    }

    Context *GetCurrentContext()
    {
        return gCurrentContext;
    }

    IO &GetIO()
    {
        return current().io;
    }

    DrawList &GetDrawList()
    {
        return current().drawList;
    }

    DrawList &GetTransDrawList()
    {
        return current().transDrawList;
    }

    DrawList &GetOverlayDrawList()
    {
        return current().overlayList;
    }

    void SetWhitePixel(TextureHandle atlas, Vec2 whiteUV)
    {
        current().drawList.setWhitePixel(atlas, whiteUV);
        current().transDrawList.setWhitePixel(atlas, whiteUV);
        current().overlayList.setWhitePixel(atlas, whiteUV);
    }

    void NewFrame()
    {
        auto &ctx = current();
        ctx.drawList.clear();
        ctx.transDrawList.clear();
        ctx.overlayList.clear();
        ctx.drawData.passes.clear();
    }

    void Render()
    {
        auto &ctx = current();
        ctx.io.mouseWheelX = 0.0f;
        ctx.io.mouseWheelY = 0.0f;
        ctx.io.inputChars.clear();
        ctx.io.keyEvents.clear();
        ctx.io.dropEvents.clear();
        // passes are already populated by WidgetApp::paint();
        // append the overlay as the last pass (identity camera, always on top).
        if (!ctx.overlayList.commands().empty())
            ctx.drawData.passes.push_back({&ctx.overlayList, Camera2D{}});
        ctx.drawData.displayWidth      = ctx.io.displayWidth;
        ctx.drawData.displayHeight     = ctx.io.displayHeight;
        ctx.drawData.framebufferScaleX = ctx.io.framebufferScaleX;
        ctx.drawData.framebufferScaleY = ctx.io.framebufferScaleY;
    }

    DrawData *GetDrawData()
    {
        return &current().drawData;
    }

} // namespace BuGUI
