#pragma once

#include <ct/hashmap.hpp>
#include <igui/widgets/Retained.hpp>
#include <stdint.h>

namespace ig
{
namespace zen
{

// Draws the retained DrawData with OpenGL 3.3 core or OpenGL ES 3.0: one shader, one vertex and
// index buffer per pass, a draw call per command with its clip as a scissor.
// Textures live on the GPU; a TextureHandle holds the GL texture name.
// Every call needs the window's GL context to be current.
class GlRaster
{
public:
    ~GlRaster();
    // Frees the GPU objects. Needs the context current; the destructor calls it too.
    void shutdown();

    // Loads the GL functions and builds the shader and buffers. False when the
    // context is too old.
    bool init(void* (*procAddress)(const char* name));
    bool ready() const
    {
        return program_ != 0;
    }

    ig::retained::TextureHandle createTexture(int width, int height, const unsigned char* rgba);
    // A texture that is white with a coverage in alpha (a font page) from one byte a pixel: kept by the driver as one byte too.
    ig::retained::TextureHandle createTextureA8(int width, int height, const unsigned char* alpha);
    bool updateTextureA8(ig::retained::TextureHandle texture, const unsigned char* alpha, int width, int height);
    void destroyTexture(ig::retained::TextureHandle texture);
    bool updateTexture(ig::retained::TextureHandle texture, const unsigned char* rgba, int width,
            int height);

    // Clears the framebuffer to background (0xAARRGGBB) and draws every pass.
    void render(ig::retained::DrawData& data, int width, int height, uint32_t background);

    // true: render() draws into an RGBA8 framebuffer object of the size asked for (made on first use) instead of the window's, and
    // readPixels() reads that one: a screenshot of a window that is never shown. Default false.
    // `samples` (0: none) asks for a multisampled target like the window's own (msaa), resolved into a single sample target on read.
    void setOffscreen(bool on, int samples = 0)
    {
        offscreen_ = on;
        wantSamples_ = samples;
    }

    // With setOffscreen(true): makes the target for a window of this size now and says whether the driver completed it (a
    // multisampled one that it cannot complete is made single sample). false: offscreen drawing is not possible here and
    // render() draws nothing. Without setOffscreen it is true.
    bool prepareOffscreen(int width, int height);

    // Reads the framebuffer back as 0xAARRGGBB, top row first.
    void readPixels(uint32_t* dst, int width, int height) const;

    const char* renderer() const;
    // The context version that init() found, and whether it is an OpenGL ES context.
    int versionMajor() const
    {
        return major_;
    }
    int versionMinor() const
    {
        return minor_;
    }
    bool es() const
    {
        return es_;
    }

private:
    struct Size
    {
        int width = 0;
        int height = 0;
        bool alphaOnly = false; // a one-byte texture read as white with that alpha (the font atlas pages)
    };

    bool offscreen_ = false;
    int wantSamples_ = 0;
    bool offscreenFailed_ = false;
    int samples_ = 0;        // what the offscreen target really has
    unsigned fbo_ = 0;       // drawn into (multisampled when samples_ > 0)
    unsigned rbo_ = 0;
    mutable unsigned resolveFbo_ = 0; // samples_ > 0: the single sample copy that readPixels() reads
    unsigned resolveRbo_ = 0;
    int fboWidth_ = 0;
    int fboHeight_ = 0;
    bool bindTarget(int width, int height); // false: no offscreen target could be made
    int major_ = 0;
    int minor_ = 0;
    bool es_ = false;
    unsigned program_ = 0;
    unsigned vao_ = 0;
    unsigned vbo_ = 0;
    unsigned ibo_ = 0;
    unsigned whiteTexture_ = 0;
    int uRotation_ = -1;
    int uTranslate_ = -1;
    int uInvDisplay_ = -1;
    int uTexture_ = -1;
    int uRoundCount_ = -1;
    int uRoundRect_ = -1;
    int uRoundRadii_ = -1;
    int uAaWidth_ = -1;
    ct::HashMap<uintptr_t, Size> sizes_;
};

} // namespace zen
} // namespace ig
