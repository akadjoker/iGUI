#include <igui_zen/GlRaster.hpp>

#include <glad/gl.h>

#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

namespace ig
{
namespace zen
{

namespace
{

const char *const kVertexShader = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor;
uniform mat2 uRotation;   // camera rotation and scale
uniform vec2 uTranslate;  // camera translation, in logical pixels
uniform vec2 uInvDisplay; // 1 / logical display size
out vec2 vUv;
out vec4 vColor;
void main()
{
    vec2 p = uRotation * aPos + uTranslate;
    gl_Position = vec4(p.x * uInvDisplay.x * 2.0 - 1.0, 1.0 - p.y * uInvDisplay.y * 2.0, 0.0, 1.0);
    vUv = aUv;
    vColor = aColor;
}
)";

const char *const kFragmentShader = R"(#version 330 core
in vec2 vUv;
in vec4 vColor;
uniform sampler2D uTexture;
out vec4 fragColor;
void main()
{
    fragColor = vColor * texture(uTexture, vUv);
}
)";

unsigned compile(unsigned type, const char *source)
{
    const unsigned shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
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

unsigned uploadRgba(int width, int height, const unsigned char *rgba)
{
    unsigned texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
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
    if (!program_)
        return;
    glDeleteTextures(1, &whiteTexture_);
    glDeleteBuffers(1, &vbo_);
    glDeleteBuffers(1, &ibo_);
    glDeleteVertexArrays(1, &vao_);
    glDeleteProgram(program_);
    program_ = 0;
}

bool GlRaster::init(void *(*procAddress)(const char *name))
{
    if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(procAddress)))
        return false;
    glGetIntegerv(GL_MAJOR_VERSION, &major_);
    glGetIntegerv(GL_MINOR_VERSION, &minor_);
    if (major_ < 3 || (major_ == 3 && minor_ < 3))
        return false;

    const unsigned vs = compile(GL_VERTEX_SHADER, kVertexShader);
    const unsigned fs = compile(GL_FRAGMENT_SHADER, kFragmentShader);
    if (!vs || !fs)
        return false;
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

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ibo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    const GLsizei stride = sizeof(ig::retained::DrawVertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(offsetof(ig::retained::DrawVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(offsetof(ig::retained::DrawVertex, u)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, reinterpret_cast<void *>(offsetof(ig::retained::DrawVertex, color)));
    glBindVertexArray(0);

    const unsigned char white[4] = {255, 255, 255, 255};
    whiteTexture_ = uploadRgba(1, 1, white);
    return true;
}

const char *GlRaster::renderer() const
{
    return reinterpret_cast<const char *>(glGetString(GL_RENDERER));
}

ig::retained::TextureHandle GlRaster::createTexture(int width, int height, const unsigned char *rgba)
{
    ig::retained::TextureHandle handle;
    if (width <= 0 || height <= 0)
        return handle;
    const unsigned texture = uploadRgba(width, height, rgba);
    handle.value = texture;
    Size size;
    size.width = width;
    size.height = height;
    sizes_[handle.value] = size;
    return handle;
}

void GlRaster::destroyTexture(ig::retained::TextureHandle handle)
{
    if (!handle)
        return;
    const unsigned texture = static_cast<unsigned>(handle.value);
    glDeleteTextures(1, &texture);
    sizes_.erase(handle.value);
}

bool GlRaster::updateTexture(ig::retained::TextureHandle handle, const unsigned char *rgba, int width, int height)
{
    Size *size = handle ? sizes_.find(handle.value) : nullptr;
    if (!size || width <= 0 || height <= 0)
        return false;
    glBindTexture(GL_TEXTURE_2D, static_cast<unsigned>(handle.value));
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (size->width == width && size->height == height)
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        size->width = width;
        size->height = height;
    }
    return true;
}

void GlRaster::render(ig::retained::DrawData &data, int width, int height, uint32_t background)
{
    data.stats.reset();
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(((background >> 16) & 0xff) / 255.0f, ((background >> 8) & 0xff) / 255.0f,
                 (background & 0xff) / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!program_ || data.displayWidth <= 0.0f || data.displayHeight <= 0.0f)
        return;

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

    for (const ig::retained::DrawPass &pass : data.passes)
    {
        if (!pass.list || pass.list->commands().empty())
            continue;
        const auto &vertices = pass.list->vertices();
        const auto &indices = pass.list->indices();

        // The camera rotates and scales about its pivot, then moves.
        const float cosine = cosf(pass.camera.angle) * pass.camera.scale;
        const float sine = sinf(pass.camera.angle) * pass.camera.scale;
        const float pivotX = pass.camera.pivotX * data.displayWidth;
        const float pivotY = pass.camera.pivotY * data.displayHeight;
        const float offsetX = pass.camera.x - pivotX;
        const float offsetY = pass.camera.y - pivotY;
        const float translateX = cosine * offsetX - sine * offsetY + pivotX;
        const float translateY = sine * offsetX + cosine * offsetY + pivotY;
        const float rotation[4] = {cosine, sine, -sine, cosine}; // column major
        glUniformMatrix2fv(uRotation_, 1, GL_FALSE, rotation);
        glUniform2f(uTranslate_, translateX, translateY);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(ig::retained::DrawVertex)),
                     vertices.data(), GL_STREAM_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
                     indices.data(), GL_STREAM_DRAW);

        for (const ig::retained::DrawCmd &command : pass.list->commands())
        {
            if (command.indexCount == 0 || command.indexOffset + command.indexCount > indices.size())
                continue;
            int sx = 0, sy = 0, sw = width, sh = height;
            if (pass.camera.angle == 0.0f)
            {
                float x0 = (cosine * command.clip.x + translateX) * scaleX;
                float y0 = (cosine * command.clip.y + translateY) * scaleY;
                float x1 = (cosine * (command.clip.x + command.clip.w) + translateX) * scaleX;
                float y1 = (cosine * (command.clip.y + command.clip.h) + translateY) * scaleY;
                if (x0 > x1) { const float t = x0; x0 = x1; x1 = t; }
                if (y0 > y1) { const float t = y0; y0 = y1; y1 = t; }
                const int ix0 = static_cast<int>(floorf(x0)) < 0 ? 0 : static_cast<int>(floorf(x0));
                const int iy0 = static_cast<int>(floorf(y0)) < 0 ? 0 : static_cast<int>(floorf(y0));
                int ix1 = static_cast<int>(ceilf(x1));
                int iy1 = static_cast<int>(ceilf(y1));
                if (ix1 > width) ix1 = width;
                if (iy1 > height) iy1 = height;
                if (ix1 <= ix0 || iy1 <= iy0)
                    continue;
                sx = ix0;
                sw = ix1 - ix0;
                sy = height - iy1; // GL's origin is the bottom left
                sh = iy1 - iy0;
            }
            glScissor(sx, sy, sw, sh);
            glBindTexture(GL_TEXTURE_2D, command.texture ? static_cast<unsigned>(command.texture.value) : whiteTexture_);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(command.indexCount), GL_UNSIGNED_INT,
                           reinterpret_cast<void *>(static_cast<uintptr_t>(command.indexOffset) * sizeof(uint32_t)));
            data.stats.drawCalls++;
        }
    }
    glBindVertexArray(0);
    glDisable(GL_SCISSOR_TEST);
}

void GlRaster::readPixels(uint32_t *dst, int width, int height) const
{
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    // glReadPixels gives the bottom row first, so rows go in from the end.
    for (int y = 0; y < height; ++y)
    {
        glReadPixels(0, height - 1 - y, width, 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, dst + static_cast<size_t>(y) * width);
    }
}

} // namespace zen
} // namespace ig
