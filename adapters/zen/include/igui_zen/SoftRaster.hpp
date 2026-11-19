#pragma once

#include <ct/vector.hpp>
#include <igui/widgets/Retained.hpp>
#include <stdint.h>

namespace ig
{
namespace zen
{

// Software rasteriser for the retained DrawData. Triangles are drawn into a
// supersampled buffer and box-filtered down, so every edge is anti-aliased
// and shared diagonals of a triangle fan leave no seam. A command's rounded
// clips (DrawList::pushClipRoundedRect) are tested per sample, so their curves
// are anti-aliased the same way (with one sample per pixel, by partial coverage).
class SoftRaster
{
public:
    ig::retained::TextureHandle createTexture(int width, int height, const unsigned char* rgba);
    void destroyTexture(ig::retained::TextureHandle texture);
    // Replaces the pixels; the size must match the texture's.
    bool updateTexture(ig::retained::TextureHandle texture, const unsigned char* rgba, int width,
            int height);

    // A texture that is white with a coverage in alpha (a font page) from one byte a pixel: the same texture as the RGBA one of
    // white pixels with that alpha, a quarter of the memory. updateTextureA8 replaces the bytes (the size must match).
    ig::retained::TextureHandle createTextureA8(int width, int height, const unsigned char* alpha);
    bool updateTextureA8(ig::retained::TextureHandle texture, const unsigned char* alpha, int width, int height);

    // Samples per axis: 1 = no AA, 2 = 4 samples per pixel, 3 or 4 = more.
    void setSamples(int perAxis);
    // With anti-aliasing the window is rasterised a band of this many sample rows at a time (128: 64 pixel rows with 2 samples
    // a side), so the sample buffer is a band of the window and not the window times four (11 MB of a 1000x700 window at the
    // default). The picture is the same whatever the band, a count only a test changes.
    void setBandRows(int rows)
    {
        bandRows_ = rows < 8 ? 8 : rows;
    }
    int samples() const
    {
        return samples_;
    }

    // dst is 0xAARRGGBB. It is overwritten, starting from background. When the
    // draw data, size and settings match the previous call, the cached image is
    // copied instead of rasterised again; returns true only when it rasterised.
    bool render(ig::retained::DrawData& data, uint32_t* dst, int width, int height, int stride,
            uint32_t background);

private:
    struct Texture
    {
        int width = 0;
        int height = 0;
        uint32_t version = 0; // bumped on update so the frame cache sees new pixels
        ct::Vector<uint32_t> pixels;
        // A texture whose pixels are all white (the font atlas: white with the coverage in alpha) keeps only the alpha, a byte for each
        // pixel instead of four; a texel is read back as white with that alpha, the same pixel the full texture would have given.
        bool alphaOnly = false;
        ct::Vector<uint8_t> alpha;
    };
    static bool allWhite(const unsigned char* rgba, size_t pixels);
    static void store(Texture& texture, const unsigned char* rgba);

    struct Vertex
    {
        int32_t x, y; // 28.4 fixed point, in sample units
        float u, v;
        float r, g, b, a;
    };

    // The rounded clips of the command being drawn, in sample space terms: a sample's centre is
    // taken back to the DrawList's coordinates (the camera's inverse) and tested against each.
    struct RoundState
    {
        int count = 0;
        ig::retained::RoundClip clips[ig::retained::DrawCmd::MaxRoundClips];
        float scale = 1.0f;      // logical units to samples
        float translateX = 0.0f; // camera translation, logical units
        float translateY = 0.0f;
        float cosine = 1.0f; // camera rotation times scale
        float sine = 0.0f;
        float inverse = 1.0f;    // 1 / (cosine^2 + sine^2)
        float sampleSize = 1.0f; // one sample, in DrawList units
        bool smooth = false;     // partial coverage (one sample per pixel); else all or nothing
    };

    float roundCoverage(int sampleX, int sampleY) const;
    // Averages the samples of the pixel rows yStart..yEnd (the band in scratch_ holds them) into dst.
    void resolve(uint32_t* dst, int width, int yStart, int yEnd, int stride);
    void storeCache(uint64_t sig, const uint32_t* src, int width, int height, int stride);
    void drawTriangle(const Vertex& a, const Vertex& b, const Vertex& c, const Texture* texture,
            int clipX0, int clipY0, int clipX1, int clipY1);

    uint64_t signature(const ig::retained::DrawData& data, int width, int height,
            uint32_t background) const;

    int samples_ = 2;
    int bandRows_ = 128; // sample rows rasterised at a time when there is anti-aliasing: the buffer holds a band, not the window
    int bandOrigin_ = 0; // the sample row that is the first of the band in target_
    uint64_t lastSignature_ = 0;
    bool cacheValid_ = false; // cache_ holds the frame of lastSignature_: it is made when a frame is the same as the one before
                              // (an application that paints a different frame every time, or that skips the same ones, never keeps a copy)
    int cacheWidth_ = 0;
    int cacheHeight_ = 0;
    ct::Vector<uint32_t> cache_;
    uint32_t* target_ = nullptr;
    int targetWidth_ = 0;
    int targetHeight_ = 0;
    int targetStride_ = 0;
    ct::Vector<uint32_t> scratch_;
    ct::Vector<Vertex> vertices_;
    RoundState round_;
};

} // namespace zen
} // namespace ig
