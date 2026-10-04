#pragma once

#include <igui/widgets/Retained.hpp>
#include <ct/vector.hpp>
#include <stdint.h>

namespace ig
{
namespace zen
{

// Software rasteriser for the retained DrawData. Triangles are drawn into a
// supersampled buffer and box-filtered down, so every edge is anti-aliased
// and shared diagonals of a triangle fan leave no seam.
class SoftRaster
{
public:
    ig::retained::TextureHandle createTexture(int width, int height, const unsigned char *rgba);
    void destroyTexture(ig::retained::TextureHandle texture);
    // Replaces the pixels; the size must match the texture's.
    bool updateTexture(ig::retained::TextureHandle texture, const unsigned char *rgba, int width, int height);

    // Samples per axis: 1 = no AA, 2 = 4 samples per pixel, 3 or 4 = more.
    void setSamples(int perAxis);
    int samples() const { return samples_; }

    // dst is 0xAARRGGBB. It is overwritten, starting from background. When the
    // draw data, size and settings match the previous call, the cached image is
    // copied instead of rasterised again; returns true only when it rasterised.
    bool render(ig::retained::DrawData &data, uint32_t *dst, int width, int height,
                int stride, uint32_t background);

private:
    struct Texture
    {
        int width = 0;
        int height = 0;
        uint32_t version = 0; // bumped on update so the frame cache sees new pixels
        ct::Vector<uint32_t> pixels;
    };

    struct Vertex
    {
        int32_t x, y; // 28.4 fixed point, in sample units
        float u, v;
        float r, g, b, a;
    };

    void resolve(uint32_t *dst, int width, int height, int stride);
    void storeCache(uint64_t sig, const uint32_t *src, int width, int height, int stride);
    void drawTriangle(const Vertex &a, const Vertex &b, const Vertex &c, const Texture *texture,
                      int clipX0, int clipY0, int clipX1, int clipY1);

    uint64_t signature(const ig::retained::DrawData &data, int width, int height,
                       uint32_t background) const;

    int samples_ = 2;
    uint64_t lastSignature_ = 0;
    int cacheWidth_ = 0;
    int cacheHeight_ = 0;
    ct::Vector<uint32_t> cache_;
    uint32_t *target_ = nullptr;
    int targetWidth_ = 0;
    int targetHeight_ = 0;
    int targetStride_ = 0;
    ct::Vector<uint32_t> scratch_;
    ct::Vector<Vertex> vertices_;
};

} // namespace zen
} // namespace ig
