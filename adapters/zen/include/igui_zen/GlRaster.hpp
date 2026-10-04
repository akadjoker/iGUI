#pragma once

#include <igui/widgets/Retained.hpp>
#include <ct/hashmap.hpp>
#include <stdint.h>

namespace ig
{
namespace zen
{

// Draws the retained DrawData with OpenGL 3.3 core: one shader, one vertex and
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
    bool init(void *(*procAddress)(const char *name));
    bool ready() const { return program_ != 0; }

    ig::retained::TextureHandle createTexture(int width, int height, const unsigned char *rgba);
    void destroyTexture(ig::retained::TextureHandle texture);
    bool updateTexture(ig::retained::TextureHandle texture, const unsigned char *rgba, int width, int height);

    // Clears the framebuffer to background (0xAARRGGBB) and draws every pass.
    void render(ig::retained::DrawData &data, int width, int height, uint32_t background);

    // Reads the framebuffer back as 0xAARRGGBB, top row first.
    void readPixels(uint32_t *dst, int width, int height) const;

    const char *renderer() const;
    // The context version that init() found.
    int versionMajor() const { return major_; }
    int versionMinor() const { return minor_; }

private:
    struct Size
    {
        int width = 0;
        int height = 0;
    };

    int major_ = 0;
    int minor_ = 0;
    unsigned program_ = 0;
    unsigned vao_ = 0;
    unsigned vbo_ = 0;
    unsigned ibo_ = 0;
    unsigned whiteTexture_ = 0;
    int uRotation_ = -1;
    int uTranslate_ = -1;
    int uInvDisplay_ = -1;
    int uTexture_ = -1;
    ct::HashMap<uintptr_t, Size> sizes_;
};

} // namespace zen
} // namespace ig
