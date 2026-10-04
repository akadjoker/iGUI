#include "SoftRaster.hpp"

#include <math.h>
#include <string.h>

namespace
{

inline int64_t edge(int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t px, int32_t py)
{
    return static_cast<int64_t>(bx - ax) * (py - ay) - static_cast<int64_t>(by - ay) * (px - ax);
}

// Any antisymmetric rule works: the shared edge of two triangles is walked in
// opposite directions, so exactly one of them owns the samples that sit on it.
inline int64_t ownedBias(int32_t ax, int32_t ay, int32_t bx, int32_t by)
{
    const int32_t dy = by - ay;
    return (dy > 0 || (dy == 0 && bx < ax)) ? 0 : -1;
}


inline int64_t floorDiv(int64_t n, int64_t d) // d > 0
{
    return n >= 0 ? n / d : -((-n + d - 1) / d);
}

// Keeps the k in [kmin, kmax] where w + dw * k >= 0; false when none is left.
inline bool narrow(int64_t w, int64_t dw, int64_t &kmin, int64_t &kmax)
{
    if (dw == 0)
        return w >= 0;
    if (dw > 0)
    {
        const int64_t lo = -floorDiv(w, dw); // ceil(-w / dw)
        if (lo > kmin)
            kmin = lo;
    }
    else
    {
        const int64_t hi = floorDiv(w, -dw);
        if (hi < kmax)
            kmax = hi;
    }
    return kmin <= kmax;
}

inline int minOf(int a, int b, int c) { return a < b ? (a < c ? a : c) : (b < c ? b : c); }
inline int maxOf(int a, int b, int c) { return a > b ? (a > c ? a : c) : (b > c ? b : c); }
inline int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline uint8_t toByte(float v) { return v <= 0.0f ? 0 : (v >= 255.0f ? 255 : static_cast<uint8_t>(v + 0.5f)); }

inline uint32_t blend(uint32_t dst, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
{
    const uint32_t inv = 255u - a;
    const uint32_t dr = (dst >> 16) & 0xffu;
    const uint32_t dg = (dst >> 8) & 0xffu;
    const uint32_t db = dst & 0xffu;
    const uint32_t outR = (r * a + dr * inv + 127u) / 255u;
    const uint32_t outG = (g * a + dg * inv + 127u) / 255u;
    const uint32_t outB = (b * a + db * inv + 127u) / 255u;
    return 0xff000000u | (outR << 16) | (outG << 8) | outB;
}

} // namespace

namespace
{
inline uint32_t packPixel(const unsigned char *p)
{
    return (static_cast<uint32_t>(p[3]) << 24) | (static_cast<uint32_t>(p[0]) << 16) |
           (static_cast<uint32_t>(p[1]) << 8) | p[2];
}
} // namespace

ig::retained::TextureHandle SoftRaster::createTexture(int width, int height, const unsigned char *rgba)
{
    Texture *texture = new Texture();
    texture->width = width;
    texture->height = height;
    texture->pixels.resize(static_cast<size_t>(width) * height);
    for (size_t i = 0; i < texture->pixels.size(); ++i)
        texture->pixels[i] = packPixel(rgba + i * 4u);
    ig::retained::TextureHandle handle;
    handle.value = reinterpret_cast<uintptr_t>(texture);
    return handle;
}

bool SoftRaster::updateTexture(ig::retained::TextureHandle handle, const unsigned char *rgba, int width, int height)
{
    Texture *texture = reinterpret_cast<Texture *>(handle.value);
    if (!texture || texture->width != width || texture->height != height)
        return false;
    for (size_t i = 0; i < texture->pixels.size(); ++i)
        texture->pixels[i] = packPixel(rgba + i * 4u);
    ++texture->version;
    return true;
}

void SoftRaster::destroyTexture(ig::retained::TextureHandle texture)
{
    delete reinterpret_cast<Texture *>(texture.value);
}

void SoftRaster::setSamples(int perAxis)
{
    samples_ = clampInt(perAxis, 1, 4);
}

void SoftRaster::drawTriangle(const Vertex &va, const Vertex &vb, const Vertex &vc, const Texture *texture,
                              int clipX0, int clipY0, int clipX1, int clipY1)
{
    const Vertex *a = &va;
    const Vertex *b = &vb;
    const Vertex *c = &vc;
    int64_t area = edge(a->x, a->y, b->x, b->y, c->x, c->y);
    if (area == 0)
        return;
    if (area < 0)
    {
        const Vertex *t = b;
        b = c;
        c = t;
        area = -area;
    }

    int x0 = minOf(a->x, b->x, c->x) >> 4;
    int y0 = minOf(a->y, b->y, c->y) >> 4;
    int x1 = (maxOf(a->x, b->x, c->x) + 15) >> 4;
    int y1 = (maxOf(a->y, b->y, c->y) + 15) >> 4;
    if (x0 < clipX0) x0 = clipX0;
    if (y0 < clipY0) y0 = clipY0;
    if (x1 > clipX1) x1 = clipX1;
    if (y1 > clipY1) y1 = clipY1;
    if (x0 >= x1 || y0 >= y1)
        return;

    // w0 weighs a, w1 weighs b, w2 weighs c; each steps linearly per sample.
    const int64_t bias0 = ownedBias(b->x, b->y, c->x, c->y);
    const int64_t bias1 = ownedBias(c->x, c->y, a->x, a->y);
    const int64_t bias2 = ownedBias(a->x, a->y, b->x, b->y);
    const int64_t dw0x = -static_cast<int64_t>(c->y - b->y) * 16, dw0y = static_cast<int64_t>(c->x - b->x) * 16;
    const int64_t dw1x = -static_cast<int64_t>(a->y - c->y) * 16, dw1y = static_cast<int64_t>(a->x - c->x) * 16;
    const int64_t dw2x = -static_cast<int64_t>(b->y - a->y) * 16, dw2y = static_cast<int64_t>(b->x - a->x) * 16;

    const int32_t startX = x0 * 16 + 8;
    const int32_t startY = y0 * 16 + 8;
    int64_t row0 = edge(b->x, b->y, c->x, c->y, startX, startY);
    int64_t row1 = edge(c->x, c->y, a->x, a->y, startX, startY);
    int64_t row2 = edge(a->x, a->y, b->x, b->y, startX, startY);

    const bool constantUv = a->u == b->u && a->u == c->u && a->v == b->v && a->v == c->v;
    const bool constantColor = a->r == b->r && a->r == c->r && a->g == b->g && a->g == c->g &&
                               a->b == b->b && a->b == c->b && a->a == b->a && a->a == c->a;
    const bool flat = constantUv && constantColor;

    // Samples one texel; used once for the whole triangle when flat.
    auto texel = [texture](float u, float v, float &r, float &g, float &bl, float &al) {
        if (!texture)
            return;
        const int tx = clampInt(static_cast<int>(u * texture->width), 0, texture->width - 1);
        const int ty = clampInt(static_cast<int>(v * texture->height), 0, texture->height - 1);
        const uint32_t t = texture->pixels[static_cast<size_t>(ty) * texture->width + tx];
        r = r * ((t >> 16) & 0xffu) * (1.0f / 255.0f);
        g = g * ((t >> 8) & 0xffu) * (1.0f / 255.0f);
        bl = bl * (t & 0xffu) * (1.0f / 255.0f);
        al = al * (t >> 24) * (1.0f / 255.0f);
    };

    uint32_t flatR = 0, flatG = 0, flatB = 0, flatA = 0;
    if (flat)
    {
        float r = a->r, g = a->g, bl = a->b, al = a->a;
        texel(a->u, a->v, r, g, bl, al);
        flatR = toByte(r);
        flatG = toByte(g);
        flatB = toByte(bl);
        flatA = toByte(al);
        if (flatA == 0)
            return;
    }

    const float invArea = 1.0f / static_cast<float>(area);
    const float dl0 = static_cast<float>(dw0x) * invArea;
    const float dl1 = static_cast<float>(dw1x) * invArea;
    const float dl2 = static_cast<float>(dw2x) * invArea;
    const uint32_t flatPixel = 0xff000000u | (flatR << 16) | (flatG << 8) | flatB;
    for (int y = y0; y < y1; ++y, row0 += dw0y, row1 += dw1y, row2 += dw2y)
    {
        // The covered samples of a row are one interval: intersect the three edges.
        int64_t kmin = 0, kmax = x1 - x0 - 1;
        if (!narrow(row0 + bias0, dw0x, kmin, kmax) || !narrow(row1 + bias1, dw1x, kmin, kmax) ||
            !narrow(row2 + bias2, dw2x, kmin, kmax))
            continue;
        const int xs = x0 + static_cast<int>(kmin);
        const int xe = x0 + static_cast<int>(kmax) + 1;
        uint32_t *line = target_ + static_cast<size_t>(y) * targetStride_;
        if (flat)
        {
            if (flatA == 255u)
            {
                for (int x = xs; x < xe; ++x)
                    line[x] = flatPixel;
            }
            else
            {
                for (int x = xs; x < xe; ++x)
                    line[x] = blend(line[x], flatR, flatG, flatB, flatA);
            }
            continue;
        }

        const int64_t skip = xs - x0;
        float l0 = static_cast<float>(row0 + dw0x * skip) * invArea;
        float l1 = static_cast<float>(row1 + dw1x * skip) * invArea;
        float l2 = static_cast<float>(row2 + dw2x * skip) * invArea;
        for (int x = xs; x < xe; ++x, l0 += dl0, l1 += dl1, l2 += dl2)
        {
            float r = l0 * a->r + l1 * b->r + l2 * c->r;
            float g = l0 * a->g + l1 * b->g + l2 * c->g;
            float bl = l0 * a->b + l1 * b->b + l2 * c->b;
            float al = l0 * a->a + l1 * b->a + l2 * c->a;
            if (texture)
                texel(l0 * a->u + l1 * b->u + l2 * c->u, l0 * a->v + l1 * b->v + l2 * c->v, r, g, bl, al);
            const uint32_t alpha = toByte(al);
            if (alpha == 0)
                continue;
            line[x] = alpha == 255u ? (0xff000000u | (static_cast<uint32_t>(toByte(r)) << 16) |
                                       (static_cast<uint32_t>(toByte(g)) << 8) | toByte(bl))
                                    : blend(line[x], toByte(r), toByte(g), toByte(bl), alpha);
        }
    }
}

uint64_t SoftRaster::signature(const ig::retained::DrawData &data, int width, int height,
                               uint32_t background) const
{
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](const void *p, size_t n) {
        const unsigned char *b = static_cast<const unsigned char *>(p);
        for (size_t i = 0; i < n; ++i)
            h = (h ^ b[i]) * 1099511628211ull;
    };
    const int header[4] = {width, height, samples_, static_cast<int>(background)};
    mix(header, sizeof header);
    mix(&data.displayWidth, sizeof data.displayWidth);
    mix(&data.displayHeight, sizeof data.displayHeight);
    for (const ig::retained::DrawPass &pass : data.passes)
    {
        if (!pass.list)
            continue;
        mix(&pass.camera, sizeof pass.camera);
        const auto &v = pass.list->vertices();
        const auto &ix = pass.list->indices();
        mix(v.data(), v.size() * sizeof v[0]);
        mix(ix.data(), ix.size() * sizeof ix[0]);
        for (const ig::retained::DrawCmd &cmd : pass.list->commands())
        {
            mix(&cmd.texture.value, sizeof cmd.texture.value);
            if (cmd.texture)
                mix(&reinterpret_cast<const Texture *>(cmd.texture.value)->version, sizeof(uint32_t));
            mix(&cmd.clip, sizeof cmd.clip);
            mix(&cmd.indexOffset, sizeof cmd.indexOffset);
            mix(&cmd.indexCount, sizeof cmd.indexCount);
        }
    }
    return h;
}

bool SoftRaster::render(ig::retained::DrawData &data, uint32_t *dst, int width, int height,
                        int stride, uint32_t background)
{
    data.stats.reset();
    if (width <= 0 || height <= 0 || data.displayWidth <= 0.0f)
        return false;

    const uint64_t sig = signature(data, width, height, background);
    if (sig == lastSignature_ && cacheWidth_ == width && cacheHeight_ == height)
    {
        for (int y = 0; y < height; ++y)
            memcpy(dst + static_cast<size_t>(y) * stride, cache_.data() + static_cast<size_t>(y) * width,
                   static_cast<size_t>(width) * sizeof(uint32_t));
        return false;
    }

    const int ss = samples_;
    const int bufW = width * ss;
    const int bufH = height * ss;
    if (ss == 1)
    {
        target_ = dst;
        targetStride_ = stride;
    }
    else
    {
        scratch_.resize(static_cast<size_t>(bufW) * bufH);
        target_ = scratch_.data();
        targetStride_ = bufW;
    }
    targetWidth_ = bufW;
    targetHeight_ = bufH;
    for (int y = 0; y < bufH; ++y)
    {
        uint32_t *line = target_ + static_cast<size_t>(y) * targetStride_;
        for (int x = 0; x < bufW; ++x)
            line[x] = background;
    }

    // Logical units to samples: the window may have a content scale above 1.
    const float scale = static_cast<float>(width) / data.displayWidth * ss;

    for (const ig::retained::DrawPass &pass : data.passes)
    {
        if (!pass.list)
            continue;
        const auto &source = pass.list->vertices();
        const auto &indices = pass.list->indices();

        const float cosine = cosf(pass.camera.angle) * pass.camera.scale;
        const float sine = sinf(pass.camera.angle) * pass.camera.scale;
        const float pivotX = pass.camera.pivotX * data.displayWidth;
        const float pivotY = pass.camera.pivotY * data.displayHeight;
        const float offsetX = pass.camera.x - pivotX;
        const float offsetY = pass.camera.y - pivotY;
        const float translateX = cosine * offsetX - sine * offsetY + pivotX;
        const float translateY = sine * offsetX + cosine * offsetY + pivotY;

        vertices_.resize(source.size());
        for (size_t i = 0; i < source.size(); ++i)
        {
            const ig::retained::DrawVertex &s = source[i];
            Vertex &v = vertices_[i];
            v.x = static_cast<int32_t>(lroundf((cosine * s.x - sine * s.y + translateX) * scale * 16.0f));
            v.y = static_cast<int32_t>(lroundf((sine * s.x + cosine * s.y + translateY) * scale * 16.0f));
            v.u = s.u;
            v.v = s.v;
            v.r = s.color.r;
            v.g = s.color.g;
            v.b = s.color.b;
            v.a = s.color.a;
        }

        for (const ig::retained::DrawCmd &command : pass.list->commands())
        {
            int cx0 = 0, cy0 = 0, cx1 = bufW, cy1 = bufH;
            if (pass.camera.angle == 0.0f)
            {
                float fx0 = (cosine * command.clip.x + translateX) * scale;
                float fy0 = (cosine * command.clip.y + translateY) * scale;
                float fx1 = (cosine * (command.clip.x + command.clip.w) + translateX) * scale;
                float fy1 = (cosine * (command.clip.y + command.clip.h) + translateY) * scale;
                if (fx0 > fx1) { const float t = fx0; fx0 = fx1; fx1 = t; }
                if (fy0 > fy1) { const float t = fy0; fy0 = fy1; fy1 = t; }
                cx0 = clampInt(static_cast<int>(floorf(fx0)), 0, bufW);
                cy0 = clampInt(static_cast<int>(floorf(fy0)), 0, bufH);
                cx1 = clampInt(static_cast<int>(ceilf(fx1)), 0, bufW);
                cy1 = clampInt(static_cast<int>(ceilf(fy1)), 0, bufH);
            }
            if (command.indexOffset + command.indexCount > indices.size())
                continue;
            const Texture *texture = command.texture ? reinterpret_cast<const Texture *>(command.texture.value) : nullptr;
            for (uint32_t i = 0; i + 2 < command.indexCount; i += 3)
            {
                const uint32_t ia = indices[command.indexOffset + i];
                const uint32_t ib = indices[command.indexOffset + i + 1];
                const uint32_t ic = indices[command.indexOffset + i + 2];
                if (ia >= vertices_.size() || ib >= vertices_.size() || ic >= vertices_.size())
                    continue;
                drawTriangle(vertices_[ia], vertices_[ib], vertices_[ic], texture, cx0, cy0, cx1, cy1);
            }
            ++data.stats.drawCalls;
            data.stats.triangles += static_cast<int>(command.indexCount / 3u);
        }
        data.stats.vertices += static_cast<int>(source.size());
        data.stats.clipChanges += pass.list->pushClipCount();
    }

    if (ss != 1)
        resolve(dst, width, height, stride);
    storeCache(sig, dst, width, height, stride);
    return true;
}

void SoftRaster::resolve(uint32_t *dst, int width, int height, int stride)
{
    const int ss = samples_;
    const int bufW = width * ss;
    if (ss == 2)
    {
        // Packed average of four pixels: per-byte (a + b) / 2 without overflow, twice.
        auto avg = [](uint32_t a, uint32_t b) { return (a & b) + (((a ^ b) >> 1) & 0x7f7f7f7fu); };
        for (int y = 0; y < height; ++y)
        {
            const uint32_t *top = scratch_.data() + static_cast<size_t>(y * 2) * bufW;
            const uint32_t *bottom = top + bufW;
            uint32_t *out = dst + static_cast<size_t>(y) * stride;
            for (int x = 0; x < width; ++x)
                out[x] = 0xff000000u | avg(avg(top[x * 2], top[x * 2 + 1]), avg(bottom[x * 2], bottom[x * 2 + 1]));
        }
        return;
    }

    const uint32_t count = static_cast<uint32_t>(ss * ss);
    for (int y = 0; y < height; ++y)
    {
        uint32_t *out = dst + static_cast<size_t>(y) * stride;
        for (int x = 0; x < width; ++x)
        {
            uint32_t r = 0, g = 0, b = 0;
            for (int sy = 0; sy < ss; ++sy)
            {
                const uint32_t *in = scratch_.data() + static_cast<size_t>(y * ss + sy) * bufW + x * ss;
                for (int sx = 0; sx < ss; ++sx)
                {
                    r += (in[sx] >> 16) & 0xffu;
                    g += (in[sx] >> 8) & 0xffu;
                    b += in[sx] & 0xffu;
                }
            }
            out[x] = 0xff000000u | (((r + count / 2) / count) << 16) | (((g + count / 2) / count) << 8) |
                     ((b + count / 2) / count);
        }
    }
}

void SoftRaster::storeCache(uint64_t sig, const uint32_t *src, int width, int height, int stride)
{
    cache_.resize(static_cast<size_t>(width) * height);
    for (int y = 0; y < height; ++y)
        memcpy(cache_.data() + static_cast<size_t>(y) * width, src + static_cast<size_t>(y) * stride,
               static_cast<size_t>(width) * sizeof(uint32_t));
    lastSignature_ = sig;
    cacheWidth_ = width;
    cacheHeight_ = height;
}
