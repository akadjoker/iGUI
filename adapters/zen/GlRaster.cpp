#include <igui_zen/GlRaster.hpp>

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <glad/gl.h>
#endif

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

namespace ig
{
namespace zen
{

namespace
{

const char* const kVertexShader = R"(
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor;
uniform mat2 uRotation;   // camera rotation and scale
uniform vec2 uTranslate;  // camera translation, in logical pixels
uniform vec2 uInvDisplay; // 1 / logical display size
out vec2 vUv;
out vec4 vColor;
out highp vec2 vPos;
void main()
{
    vec2 p = uRotation * aPos + uTranslate;
    gl_Position = vec4(p.x * uInvDisplay.x * 2.0 - 1.0, 1.0 - p.y * uInvDisplay.y * 2.0, 0.0, 1.0);
    vUv = aUv;
    vColor = aColor;
    vPos = aPos;
}
)";

const char* const kFragmentShader = R"(
precision mediump float;
in vec2 vUv;
in vec4 vColor;
in highp vec2 vPos;
uniform sampler2D uTexture;
// Rounded clips of the command (DrawCmd::roundClips): centre and half size of each rectangle, and
// its corner radii (top-left, top-right, bottom-right, bottom-left). uAaWidth is one framebuffer
// pixel in the DrawList's units.
uniform int uRoundCount;
uniform highp vec4 uRoundRect[4];
uniform highp vec4 uRoundRadii[4];
uniform highp float uAaWidth;
out vec4 fragColor;
void main()
{
    float coverage = 1.0;
    for (int i = 0; i < 4; ++i)
    {
        if (i >= uRoundCount) break;
        highp vec2 p = vPos - uRoundRect[i].xy;
        highp vec4 r = uRoundRadii[i];
        highp float radius = p.x < 0.0 ? (p.y < 0.0 ? r.x : r.w) : (p.y < 0.0 ? r.y : r.z);
        highp vec2 q = abs(p) - uRoundRect[i].zw + radius;
        highp float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
        coverage *= clamp(0.5 - d / uAaWidth, 0.0, 1.0);
    }
    fragColor = vColor * texture(uTexture, vUv);
    fragColor.a *= coverage;
}
)";

// The same GLSL body serves both profiles: desktop 3.3 core and ES 3.0 differ only in the version
// line (the ES fragment shader also needs a default precision, which core ignores).
unsigned compile(unsigned type, const char* source, bool es)
{
    const char* sources[2] = { es ? "#version 300 es\n" : "#version 330 core\n", source };
    const unsigned shader = glCreateShader(type);
    glShaderSource(shader, 2, sources, nullptr);
    glCompileShader(shader);
    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof log, nullptr, log);
        fprintf(stderr, "[GlRaster] shader: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// Whether every pixel is white (what the font atlas is: white, the coverage in alpha).
bool allWhite(const unsigned char* rgba, size_t pixels)
{
    for (size_t i = 0; i < pixels; ++i)
        if ((rgba[i * 4u] & rgba[i * 4u + 1u] & rgba[i * 4u + 2u]) != 255u) return false;
    return true;
}

// Such a texture is kept in the driver as one byte a pixel (a quarter of the memory), read as (1, 1, 1, red): the same texel.
void swizzleAlphaOnly(bool on)
{
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, on ? GL_ONE : GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, on ? GL_ONE : GL_GREEN);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, on ? GL_ONE : GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, on ? GL_RED : GL_ALPHA);
}

// The alpha of every pixel, in a buffer of its own (freed by the caller).
unsigned char* alphaBytes(const unsigned char* rgba, size_t pixels)
{
    unsigned char* out = new unsigned char[pixels];
    for (size_t i = 0; i < pixels; ++i) out[i] = rgba[i * 4u + 3u];
    return out;
}

unsigned uploadRgba(int width, int height, const unsigned char* rgba, bool* alphaOnly = nullptr)
{
    unsigned texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    const size_t pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (alphaOnly && allWhite(rgba, pixels))
    {
        unsigned char* alpha = alphaBytes(rgba, pixels);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, alpha);
        delete[] alpha;
        swizzleAlphaOnly(true);
        *alphaOnly = true;
    }
    else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return texture;
}

} // namespace

GlRaster::~GlRaster()
{
    shutdown();
}

void GlRaster::shutdown()
{
    if (!program_) return;
    glDeleteTextures(1, &whiteTexture_);
    glDeleteBuffers(1, &vbo_);
    glDeleteBuffers(1, &ibo_);
    glDeleteVertexArrays(1, &vao_);
    glDeleteProgram(program_);
    if (fbo_)
    {
        glDeleteFramebuffers(1, &fbo_);
        glDeleteRenderbuffers(1, &rbo_);
        fbo_ = rbo_ = 0;
        if (resolveFbo_)
        {
            glDeleteFramebuffers(1, &resolveFbo_);
            glDeleteRenderbuffers(1, &resolveRbo_);
            resolveFbo_ = resolveRbo_ = 0;
        }
    }
    program_ = 0;
}

#ifndef __EMSCRIPTEN__
namespace
{
// glad calls its loader as a function that returns a function pointer; the platform's returns
// void*. Calling one through a pointer of the other type is undefined (the function sanitizer says
// so on every load): a function of glad's own type forwards to the platform's.
void* (*gProcAddress)(const char* name) = nullptr;
GLADapiproc procThunk(const char* name)
{
    return reinterpret_cast<GLADapiproc>(gProcAddress(name));
}
} // namespace
#endif

bool GlRaster::init(void* (*procAddress)(const char* name))
{
#ifdef __EMSCRIPTEN__
    (void) procAddress; // WebGL 2 functions are linked directly
#else
    gProcAddress = procAddress;
    const bool loaded = gladLoadGL(procThunk) != 0;
    gProcAddress = nullptr;
    if (!loaded) return false;
#endif
    glGetIntegerv(GL_MAJOR_VERSION, &major_);
    glGetIntegerv(GL_MINOR_VERSION, &minor_);
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    es_ = version && strncmp(version, "OpenGL ES", 9) == 0;
    if (major_ < 3 || (!es_ && major_ == 3 && minor_ < 3)) return false;

    const unsigned vs = compile(GL_VERTEX_SHADER, kVertexShader, es_);
    const unsigned fs = compile(GL_FRAGMENT_SHADER, kFragmentShader, es_);
    if (!vs || !fs)
    {
        // One can compile while the other fails, and the context keeps it alive
        // until it is deleted: shutdown() has no program to clean up here.
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    int ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }
    uRotation_ = glGetUniformLocation(program_, "uRotation");
    uTranslate_ = glGetUniformLocation(program_, "uTranslate");
    uInvDisplay_ = glGetUniformLocation(program_, "uInvDisplay");
    uTexture_ = glGetUniformLocation(program_, "uTexture");
    uRoundCount_ = glGetUniformLocation(program_, "uRoundCount");
    uRoundRect_ = glGetUniformLocation(program_, "uRoundRect");
    uRoundRadii_ = glGetUniformLocation(program_, "uRoundRadii");
    uAaWidth_ = glGetUniformLocation(program_, "uAaWidth");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ibo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    const GLsizei stride = sizeof(ig::retained::DrawVertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void*>(offsetof(ig::retained::DrawVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<void*>(offsetof(ig::retained::DrawVertex, u)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
            reinterpret_cast<void*>(offsetof(ig::retained::DrawVertex, color)));
    glBindVertexArray(0);

    const unsigned char white[4] = { 255, 255, 255, 255 };
    whiteTexture_ = uploadRgba(1, 1, white);
    return true;
}

const char* GlRaster::renderer() const
{
    return reinterpret_cast<const char*>(glGetString(GL_RENDERER));
}

ig::retained::TextureHandle GlRaster::createTexture(int width, int height,
        const unsigned char* rgba)
{
    ig::retained::TextureHandle handle;
    if (width <= 0 || height <= 0) return handle;
    bool alphaOnly = false;
    const unsigned texture = uploadRgba(width, height, rgba, &alphaOnly);
    handle.value = texture;
    Size size;
    size.width = width;
    size.height = height;
    size.alphaOnly = alphaOnly;
    sizes_[handle.value] = size;
    return handle;
}

ig::retained::TextureHandle GlRaster::createTextureA8(int width, int height, const unsigned char* alpha)
{
    ig::retained::TextureHandle handle;
    if (width <= 0 || height <= 0) return handle;
    unsigned texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, alpha);
    swizzleAlphaOnly(true);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    handle.value = texture;
    Size size;
    size.width = width;
    size.height = height;
    size.alphaOnly = true;
    sizes_[handle.value] = size;
    return handle;
}

bool GlRaster::updateTextureA8(ig::retained::TextureHandle handle, const unsigned char* alpha, int width, int height)
{
    Size* size = handle ? sizes_.find(handle.value) : nullptr;
    if (!size || !size->alphaOnly || size->width != width || size->height != height) return false;
    glBindTexture(GL_TEXTURE_2D, static_cast<unsigned>(handle.value));
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, alpha);
    return true;
}

void GlRaster::destroyTexture(ig::retained::TextureHandle handle)
{
    if (!handle) return;
    const unsigned texture = static_cast<unsigned>(handle.value);
    glDeleteTextures(1, &texture);
    sizes_.erase(handle.value);
}

bool GlRaster::updateTexture(ig::retained::TextureHandle handle, const unsigned char* rgba,
        int width, int height)
{
    Size* size = handle ? sizes_.find(handle.value) : nullptr;
    if (!size || width <= 0 || height <= 0) return false;
    glBindTexture(GL_TEXTURE_2D, static_cast<unsigned>(handle.value));
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    const size_t pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    const bool white = allWhite(rgba, pixels);
    if (size->alphaOnly && white)
    {
        // still white: one byte a pixel, as it was made
        unsigned char* alpha = alphaBytes(rgba, pixels);
        if (size->width == width && size->height == height)
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, alpha);
        else
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, alpha);
        delete[] alpha;
    }
    else if (size->alphaOnly)
    {
        // no longer white: whole pixels, read as they are
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        swizzleAlphaOnly(false);
        size->alphaOnly = false;
    }
    else if (size->width == width && size->height == height)
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    size->width = width;
    size->height = height;
    return true;
}

bool GlRaster::bindTarget(int width, int height)
{
    if (!offscreen_)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }
    if (offscreenFailed_ || width <= 0 || height <= 0) return false;
    if (!fbo_ || width != fboWidth_ || height != fboHeight_)
    {
        if (!fbo_)
        {
            glGenFramebuffers(1, &fbo_);
            glGenRenderbuffers(1, &rbo_);
            glGenFramebuffers(1, &resolveFbo_);
            glGenRenderbuffers(1, &resolveRbo_);
            int maxSamples = 0;
            glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
            samples_ = wantSamples_ < maxSamples ? wantSamples_ : maxSamples;
        }
        // The samples asked for first; a driver that cannot complete that target (a software one may lack the multisample
        // storage or the resolve) gets a single sample one; if that is not complete either, offscreen drawing is not possible.
        for (;;)
        {
            glBindRenderbuffer(GL_RENDERBUFFER, rbo_);
            if (samples_ > 0)
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples_, GL_RGBA8, width, height);
            else
                glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo_);
            bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
            if (samples_ > 0)
            {
                glBindRenderbuffer(GL_RENDERBUFFER, resolveRbo_);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
                glBindFramebuffer(GL_FRAMEBUFFER, resolveFbo_);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, resolveRbo_);
                complete = complete && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
            }
            if (complete) break;
            if (samples_ > 0)
            {
                fprintf(stderr, "[GlRaster] offscreen target with %d samples is not complete, trying a single sample one\n", samples_);
                samples_ = 0;
                continue;
            }
            fprintf(stderr, "[GlRaster] the offscreen framebuffer (RGBA8, %dx%d) is not complete on this driver\n", width, height);
            offscreenFailed_ = true;
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }
        fboWidth_ = width;
        fboHeight_ = height;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    return true;
}

bool GlRaster::prepareOffscreen(int width, int height)
{
    return bindTarget(width, height);
}

void GlRaster::render(ig::retained::DrawData& data, int width, int height, uint32_t background)
{
    data.stats.reset();
    if (!bindTarget(width, height)) return;
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(((background >> 16) & 0xff) / 255.0f, ((background >> 8) & 0xff) / 255.0f,
            (background & 0xff) / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!program_ || data.displayWidth <= 0.0f || data.displayHeight <= 0.0f) return;

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_SCISSOR_TEST);
    glUseProgram(program_);
    glUniform1i(uTexture_, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao_);
    glUniform2f(uInvDisplay_, 1.0f / data.displayWidth, 1.0f / data.displayHeight);

    // Logical units to framebuffer pixels, for the scissor.
    const float scaleX = static_cast<float>(width) / data.displayWidth;
    const float scaleY = static_cast<float>(height) / data.displayHeight;

    glUniform1i(uRoundCount_, 0);
    int roundsSet = 0; // rounded clips currently in the uniforms
    for (const ig::retained::DrawPass& pass: data.passes)
    {
        if (!pass.list || pass.list->commands().empty()) continue;
        const auto& vertices = pass.list->vertices();
        const auto& indices = pass.list->indices();

        // The camera rotates and scales about its pivot, then moves.
        const float cosine = cosf(pass.camera.angle) * pass.camera.scale;
        const float sine = sinf(pass.camera.angle) * pass.camera.scale;
        const float pivotX = pass.camera.pivotX * data.displayWidth;
        const float pivotY = pass.camera.pivotY * data.displayHeight;
        const float offsetX = pass.camera.x - pivotX;
        const float offsetY = pass.camera.y - pivotY;
        const float translateX = cosine * offsetX - sine * offsetY + pivotX;
        const float translateY = sine * offsetX + cosine * offsetY + pivotY;
        const float rotation[4] = { cosine, sine, -sine, cosine }; // column major
        glUniformMatrix2fv(uRotation_, 1, GL_FALSE, rotation);
        glUniform2f(uTranslate_, translateX, translateY);
        // One framebuffer pixel, in the list's units (the rounded clips' anti-aliasing width).
        glUniform1f(uAaWidth_, 2.0f / ((scaleX + scaleY) * pass.camera.scale));

        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(vertices.size() * sizeof(ig::retained::DrawVertex)),
                vertices.data(), GL_STREAM_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)), indices.data(),
                GL_STREAM_DRAW);

        for (const ig::retained::DrawCmd& command: pass.list->commands())
        {
            if (command.indexCount == 0 ||
                    command.indexOffset + command.indexCount > indices.size())
                continue;
            int sx = 0, sy = 0, sw = width, sh = height;
            if (pass.camera.angle == 0.0f)
            {
                float x0 = (cosine * command.clip.x + translateX) * scaleX;
                float y0 = (cosine * command.clip.y + translateY) * scaleY;
                float x1 = (cosine * (command.clip.x + command.clip.w) + translateX) * scaleX;
                float y1 = (cosine * (command.clip.y + command.clip.h) + translateY) * scaleY;
                if (x0 > x1)
                {
                    const float t = x0;
                    x0 = x1;
                    x1 = t;
                }
                if (y0 > y1)
                {
                    const float t = y0;
                    y0 = y1;
                    y1 = t;
                }
                const int ix0 = static_cast<int>(floorf(x0)) < 0 ? 0 : static_cast<int>(floorf(x0));
                const int iy0 = static_cast<int>(floorf(y0)) < 0 ? 0 : static_cast<int>(floorf(y0));
                int ix1 = static_cast<int>(ceilf(x1));
                int iy1 = static_cast<int>(ceilf(y1));
                if (ix1 > width) ix1 = width;
                if (iy1 > height) iy1 = height;
                if (ix1 <= ix0 || iy1 <= iy0) continue;
                sx = ix0;
                sw = ix1 - ix0;
                sy = height - iy1; // GL's origin is the bottom left
                sh = iy1 - iy0;
            }
            glScissor(sx, sy, sw, sh);
            if (command.roundClipCount > 0 || roundsSet > 0)
            {
                float rects[ig::retained::DrawCmd::MaxRoundClips * 4] = {};
                float radii[ig::retained::DrawCmd::MaxRoundClips * 4] = {};
                const int count = command.roundClipCount;
                for (int i = 0; i < count; ++i)
                {
                    const ig::retained::RoundClip& clip = command.roundClips[i];
                    rects[i * 4] = clip.rect.x + clip.rect.w * 0.5f;
                    rects[i * 4 + 1] = clip.rect.y + clip.rect.h * 0.5f;
                    rects[i * 4 + 2] = clip.rect.w * 0.5f;
                    rects[i * 4 + 3] = clip.rect.h * 0.5f;
                    for (int c = 0; c < 4; ++c) radii[i * 4 + c] = clip.radii[c];
                }
                glUniform1i(uRoundCount_, count);
                if (count > 0)
                {
                    glUniform4fv(uRoundRect_, count, rects);
                    glUniform4fv(uRoundRadii_, count, radii);
                }
                roundsSet = count;
            }
            glBindTexture(GL_TEXTURE_2D,
                    command.texture ? static_cast<unsigned>(command.texture.value) : whiteTexture_);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(command.indexCount), GL_UNSIGNED_INT,
                    reinterpret_cast<void*>(
                            static_cast<uintptr_t>(command.indexOffset) * sizeof(uint32_t)));
            data.stats.drawCalls++;
        }
    }
    glBindVertexArray(0);
    glDisable(GL_SCISSOR_TEST);
}

void GlRaster::readPixels(uint32_t* dst, int width, int height) const
{
    if (offscreen_ && samples_ > 0)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFbo_);
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, resolveFbo_);
    }
    else
        glBindFramebuffer(GL_READ_FRAMEBUFFER, offscreen_ ? fbo_ : 0);
#ifdef __EMSCRIPTEN__
    // WebGL reads RGBA bytes only: the screenshot path is not used in the browser
    (void) dst;
    (void) width;
    (void) height;
    return;
#else
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    // glReadPixels gives the bottom row first, so rows go in from the end.
    for (int y = 0; y < height; ++y)
    {
        glReadPixels(0, height - 1 - y, width, 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                dst + static_cast<size_t>(y) * width);
    }
#endif
}

} // namespace zen
} // namespace ig
