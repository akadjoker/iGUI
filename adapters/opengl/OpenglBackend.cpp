// Backend OpenGL 3.3 core para o DrawData do modo imediato. Ver o cabeçalho.
//
// As funções de GL são todas resolvidas pela função que a aplicação passa: o
// adaptador não liga a libGL nem depende de um carregador, o que o deixa
// funcionar em qualquer plataforma onde a janela saiba dar um ponteiro por nome
// (SDL_GL_GetProcAddress, glfwGetProcAddress, wglGetProcAddress, ...).

#include <igui_opengl/OpenglBackend.hpp>

#include <math.h>
#include <stdio.h>

namespace ig
{
namespace opengl
{
namespace
{

// ── Tipos de GL (o suficiente para o que este backend usa) ───────────────────
typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLboolean;
typedef unsigned char GLubyte;
typedef unsigned int GLbitfield;
typedef unsigned long long GLuint64;
typedef long GLsizeiptr;
typedef char GLchar;

const GLenum GL_TRIANGLES = 0x0004u;
const GLenum GL_UNSIGNED_INT = 0x1405u;
const GLenum GL_UNSIGNED_BYTE = 0x1401u;
const GLenum GL_FLOAT = 0x1406u;
const GLenum GL_ARRAY_BUFFER = 0x8892u;
const GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893u;
const GLenum GL_STREAM_DRAW = 0x88E0u;
const GLenum GL_VERTEX_SHADER = 0x8B31u;
const GLenum GL_FRAGMENT_SHADER = 0x8B30u;
const GLenum GL_COMPILE_STATUS = 0x8B81u;
const GLenum GL_LINK_STATUS = 0x8B82u;
const GLenum GL_TEXTURE_2D = 0x0DE1u;
const GLenum GL_RGBA = 0x1908u;
const GLenum GL_RGBA8 = 0x8058u;
const GLenum GL_TEXTURE_MIN_FILTER = 0x2801u;
const GLenum GL_TEXTURE_MAG_FILTER = 0x2800u;
const GLenum GL_LINEAR = 0x2601u;
const GLenum GL_TEXTURE_WRAP_S = 0x2802u;
const GLenum GL_TEXTURE_WRAP_T = 0x2803u;
const GLenum GL_CLAMP_TO_EDGE = 0x812Fu;
const GLenum GL_UNPACK_ALIGNMENT = 0x0CF5u;
const GLenum GL_PACK_ALIGNMENT = 0x0D05u;
const GLenum GL_BLEND = 0x0BE2u;
const GLenum GL_SRC_ALPHA = 0x0302u;
const GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303u;
const GLenum GL_DEPTH_TEST = 0x0B71u;
const GLenum GL_CULL_FACE = 0x0B44u;
const GLenum GL_SCISSOR_TEST = 0x0C11u;
const GLenum GL_COLOR_BUFFER_BIT = 0x00004000u;
const GLenum GL_FRAMEBUFFER = 0x8D40u;
const GLenum GL_COLOR_ATTACHMENT0 = 0x8CE0u;
const GLenum GL_FRAMEBUFFER_COMPLETE = 0x8CD5u;
const GLenum GL_MAJOR_VERSION = 0x821Bu;
const GLenum GL_MINOR_VERSION = 0x821Cu;
const GLenum GL_RENDERER = 0x1F01u;
const GLenum GL_TEXTURE0 = 0x84C0u;
const GLenum GL_TIME_ELAPSED = 0x88BFu;
const GLenum GL_QUERY_RESULT = 0x8866u;
const GLenum GL_QUERY_RESULT_AVAILABLE = 0x8867u;
const GLenum GL_FALSE = 0u;
const GLenum GL_TRUE = 1u;
const GLenum GL_NO_ERROR = 0u;

struct GlFunctions
{
    void (*enable)(GLenum);
    void (*disable)(GLenum);
    void (*blendFunc)(GLenum, GLenum);
    void (*clearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
    void (*clear)(GLbitfield);
    void (*viewport)(GLint, GLint, GLsizei, GLsizei);
    void (*scissor)(GLint, GLint, GLsizei, GLsizei);
    void (*pixelStorei)(GLenum, GLint);
    void (*getIntegerv)(GLenum, GLint*);
    const GLubyte* (*getString)(GLenum);
    GLenum (*getError)(void);
    void (*genVertexArrays)(GLsizei, GLuint*);
    void (*bindVertexArray)(GLuint);
    void (*deleteVertexArrays)(GLsizei, const GLuint*);
    void (*genBuffers)(GLsizei, GLuint*);
    void (*bindBuffer)(GLenum, GLuint);
    void (*bufferData)(GLenum, GLsizeiptr, const void*, GLenum);
    void (*deleteBuffers)(GLsizei, const GLuint*);
    void (*enableVertexAttribArray)(GLuint);
    void (*vertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
    void (*drawElements)(GLenum, GLsizei, GLenum, const void*);
    GLuint (*createShader)(GLenum);
    void (*shaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
    void (*compileShader)(GLuint);
    void (*getShaderiv)(GLuint, GLenum, GLint*);
    void (*getShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
    void (*deleteShader)(GLuint);
    GLuint (*createProgram)(void);
    void (*attachShader)(GLuint, GLuint);
    void (*linkProgram)(GLuint);
    void (*getProgramiv)(GLuint, GLenum, GLint*);
    void (*getProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
    void (*deleteProgram)(GLuint);
    void (*useProgram)(GLuint);
    GLint (*getUniformLocation)(GLuint, const GLchar*);
    void (*uniform1i)(GLint, GLint);
    void (*uniform2f)(GLint, GLfloat, GLfloat);
    void (*activeTexture)(GLenum);
    void (*genTextures)(GLsizei, GLuint*);
    void (*bindTexture)(GLenum, GLuint);
    void (*texImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
    void (*texSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum,
            const void*);
    void (*texParameteri)(GLenum, GLenum, GLint);
    void (*deleteTextures)(GLsizei, const GLuint*);
    void (*genFramebuffers)(GLsizei, GLuint*);
    void (*bindFramebuffer)(GLenum, GLuint);
    void (*framebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
    GLenum (*checkFramebufferStatus)(GLenum);
    void (*deleteFramebuffers)(GLsizei, const GLuint*);
    void (*blitFramebuffer)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield,
            GLenum);
    void (*readPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
    void (*genQueries)(GLsizei, GLuint*);
    void (*deleteQueries)(GLsizei, const GLuint*);
    void (*beginQuery)(GLenum, GLuint);
    void (*endQuery)(GLenum);
    void (*getQueryObjectiv)(GLuint, GLenum, GLint*);
    void (*getQueryObjectui64v)(GLuint, GLenum, GLuint64*);

    bool load(void* (*procAddress)(const char* name))
    {
        // O apelido evita repetir o cast em cada linha; um nome que não exista
        // deixa o ponteiro nulo, e init() só exige os indispensáveis.
#define IG_GL_LOAD(member, type, name) member = reinterpret_cast<type>(procAddress(name))
        IG_GL_LOAD(enable, void (*)(GLenum), "glEnable");
        IG_GL_LOAD(disable, void (*)(GLenum), "glDisable");
        IG_GL_LOAD(blendFunc, void (*)(GLenum, GLenum), "glBlendFunc");
        IG_GL_LOAD(clearColor, void (*)(GLfloat, GLfloat, GLfloat, GLfloat), "glClearColor");
        IG_GL_LOAD(clear, void (*)(GLbitfield), "glClear");
        IG_GL_LOAD(viewport, void (*)(GLint, GLint, GLsizei, GLsizei), "glViewport");
        IG_GL_LOAD(scissor, void (*)(GLint, GLint, GLsizei, GLsizei), "glScissor");
        IG_GL_LOAD(pixelStorei, void (*)(GLenum, GLint), "glPixelStorei");
        IG_GL_LOAD(getIntegerv, void (*)(GLenum, GLint*), "glGetIntegerv");
        IG_GL_LOAD(getString, const GLubyte* (*) (GLenum), "glGetString");
        IG_GL_LOAD(getError, GLenum(*)(void), "glGetError");
        IG_GL_LOAD(genVertexArrays, void (*)(GLsizei, GLuint*), "glGenVertexArrays");
        IG_GL_LOAD(bindVertexArray, void (*)(GLuint), "glBindVertexArray");
        IG_GL_LOAD(deleteVertexArrays, void (*)(GLsizei, const GLuint*), "glDeleteVertexArrays");
        IG_GL_LOAD(genBuffers, void (*)(GLsizei, GLuint*), "glGenBuffers");
        IG_GL_LOAD(bindBuffer, void (*)(GLenum, GLuint), "glBindBuffer");
        IG_GL_LOAD(bufferData, void (*)(GLenum, GLsizeiptr, const void*, GLenum), "glBufferData");
        IG_GL_LOAD(deleteBuffers, void (*)(GLsizei, const GLuint*), "glDeleteBuffers");
        IG_GL_LOAD(enableVertexAttribArray, void (*)(GLuint), "glEnableVertexAttribArray");
        IG_GL_LOAD(vertexAttribPointer,
                void (*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*),
                "glVertexAttribPointer");
        IG_GL_LOAD(drawElements, void (*)(GLenum, GLsizei, GLenum, const void*), "glDrawElements");
        IG_GL_LOAD(createShader, GLuint(*)(GLenum), "glCreateShader");
        IG_GL_LOAD(shaderSource, void (*)(GLuint, GLsizei, const GLchar* const*, const GLint*),
                "glShaderSource");
        IG_GL_LOAD(compileShader, void (*)(GLuint), "glCompileShader");
        IG_GL_LOAD(getShaderiv, void (*)(GLuint, GLenum, GLint*), "glGetShaderiv");
        IG_GL_LOAD(getShaderInfoLog, void (*)(GLuint, GLsizei, GLsizei*, GLchar*),
                "glGetShaderInfoLog");
        IG_GL_LOAD(deleteShader, void (*)(GLuint), "glDeleteShader");
        IG_GL_LOAD(createProgram, GLuint(*)(void), "glCreateProgram");
        IG_GL_LOAD(attachShader, void (*)(GLuint, GLuint), "glAttachShader");
        IG_GL_LOAD(linkProgram, void (*)(GLuint), "glLinkProgram");
        IG_GL_LOAD(getProgramiv, void (*)(GLuint, GLenum, GLint*), "glGetProgramiv");
        IG_GL_LOAD(getProgramInfoLog, void (*)(GLuint, GLsizei, GLsizei*, GLchar*),
                "glGetProgramInfoLog");
        IG_GL_LOAD(deleteProgram, void (*)(GLuint), "glDeleteProgram");
        IG_GL_LOAD(useProgram, void (*)(GLuint), "glUseProgram");
        IG_GL_LOAD(getUniformLocation, GLint(*)(GLuint, const GLchar*), "glGetUniformLocation");
        IG_GL_LOAD(uniform1i, void (*)(GLint, GLint), "glUniform1i");
        IG_GL_LOAD(uniform2f, void (*)(GLint, GLfloat, GLfloat), "glUniform2f");
        IG_GL_LOAD(activeTexture, void (*)(GLenum), "glActiveTexture");
        IG_GL_LOAD(genTextures, void (*)(GLsizei, GLuint*), "glGenTextures");
        IG_GL_LOAD(bindTexture, void (*)(GLenum, GLuint), "glBindTexture");
        IG_GL_LOAD(texImage2D,
                void (*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum,
                        const void*),
                "glTexImage2D");
        IG_GL_LOAD(texSubImage2D,
                void (*)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum,
                        const void*),
                "glTexSubImage2D");
        IG_GL_LOAD(texParameteri, void (*)(GLenum, GLenum, GLint), "glTexParameteri");
        IG_GL_LOAD(deleteTextures, void (*)(GLsizei, const GLuint*), "glDeleteTextures");
        IG_GL_LOAD(genFramebuffers, void (*)(GLsizei, GLuint*), "glGenFramebuffers");
        IG_GL_LOAD(bindFramebuffer, void (*)(GLenum, GLuint), "glBindFramebuffer");
        IG_GL_LOAD(framebufferTexture2D, void (*)(GLenum, GLenum, GLenum, GLuint, GLint),
                "glFramebufferTexture2D");
        IG_GL_LOAD(checkFramebufferStatus, GLenum(*)(GLenum), "glCheckFramebufferStatus");
        IG_GL_LOAD(deleteFramebuffers, void (*)(GLsizei, const GLuint*), "glDeleteFramebuffers");
        IG_GL_LOAD(blitFramebuffer,
                void (*)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield,
                        GLenum),
                "glBlitFramebuffer");
        IG_GL_LOAD(readPixels, void (*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*),
                "glReadPixels");
        IG_GL_LOAD(genQueries, void (*)(GLsizei, GLuint*), "glGenQueries");
        IG_GL_LOAD(deleteQueries, void (*)(GLsizei, const GLuint*), "glDeleteQueries");
        IG_GL_LOAD(beginQuery, void (*)(GLenum, GLuint), "glBeginQuery");
        IG_GL_LOAD(endQuery, void (*)(GLenum), "glEndQuery");
        IG_GL_LOAD(getQueryObjectiv, void (*)(GLuint, GLenum, GLint*), "glGetQueryObjectiv");
        IG_GL_LOAD(getQueryObjectui64v, void (*)(GLuint, GLenum, GLuint64*),
                "glGetQueryObjectui64v");
#undef IG_GL_LOAD

        return enable && disable && blendFunc && clearColor && clear && viewport && scissor &&
               pixelStorei && getError && genVertexArrays && bindVertexArray && genBuffers &&
               bindBuffer && bufferData && enableVertexAttribArray && vertexAttribPointer &&
               drawElements && createShader && shaderSource && compileShader && getShaderiv &&
               createProgram && attachShader && linkProgram && getProgramiv && useProgram &&
               getUniformLocation && uniform1i && uniform2f && activeTexture && genTextures &&
               bindTexture && texImage2D && texParameteri && deleteTextures && genFramebuffers &&
               bindFramebuffer && framebufferTexture2D && checkFramebufferStatus && readPixels;
    }
};

const char* const kVertexShader = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor;
uniform vec2 uInvDisplay;
out vec2 vUv;
out vec4 vColor;
void main()
{
    gl_Position = vec4(aPos.x * uInvDisplay.x * 2.0 - 1.0,
                       1.0 - aPos.y * uInvDisplay.y * 2.0, 0.0, 1.0);
    vUv = aUv;
    vColor = aColor;
}
)";

const char* const kFragmentShader = R"(#version 330 core
in vec2 vUv;
in vec4 vColor;
uniform sampler2D uTexture;
out vec4 fragColor;
void main()
{
    fragColor = vColor * texture(uTexture, vUv);
}
)";

const uint32_t kVertexStride = sizeof(ig::DrawVertex);
const uint32_t kPositionOffset = 0u;
const uint32_t kUvOffset = 8u;
const uint32_t kColorOffset = 16u;

GLuint compileShader(const GlFunctions& gl, GLenum type, const char* source)
{
    const GLuint shader = gl.createShader(type);
    if (!shader) return 0u;
    gl.shaderSource(shader, 1, &source, nullptr);
    gl.compileShader(shader);
    GLint ok = 0;
    gl.getShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[1024];
        log[0] = '\0';
        if (gl.getShaderInfoLog)
            gl.getShaderInfoLog(shader, static_cast<GLsizei>(sizeof(log) - 1u), nullptr, log);
        fprintf(stderr, "[igui::opengl] shader: %s\n", log);
        gl.deleteShader(shader);
        return 0u;
    }
    return shader;
}

void configureVertexArray(const GlFunctions& gl, uint32_t vao, uint32_t vbo, uint32_t ibo)
{
    gl.bindVertexArray(vao);
    gl.bindBuffer(GL_ARRAY_BUFFER, vbo);
    gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    gl.enableVertexAttribArray(0u);
    gl.vertexAttribPointer(0u, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(kVertexStride),
            reinterpret_cast<const void*>(static_cast<uintptr_t>(kPositionOffset)));
    gl.enableVertexAttribArray(1u);
    gl.vertexAttribPointer(1u, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(kVertexStride),
            reinterpret_cast<const void*>(static_cast<uintptr_t>(kUvOffset)));
    gl.enableVertexAttribArray(2u);
    gl.vertexAttribPointer(2u, 4, GL_UNSIGNED_BYTE, GL_TRUE, static_cast<GLsizei>(kVertexStride),
            reinterpret_cast<const void*>(static_cast<uintptr_t>(kColorOffset)));
    gl.bindVertexArray(0u);
}

uint32_t uploadRgba(const GlFunctions& gl, int width, int height, const unsigned char* rgba)
{
    GLuint texture = 0u;
    gl.genTextures(1, &texture);
    gl.bindTexture(GL_TEXTURE_2D, texture);
    gl.pixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl.texImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(GL_RGBA8), width, height, 0, GL_RGBA,
            GL_UNSIGNED_BYTE, rgba);
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(GL_LINEAR));
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(GL_LINEAR));
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(GL_CLAMP_TO_EDGE));
    gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(GL_CLAMP_TO_EDGE));
    return texture;
}

} // namespace

struct Backend::Impl
{
    GlFunctions gl;
    GLint uInvDisplay;
    GLint uTexture;

    Impl()
            : gl(),
              uInvDisplay(-1),
              uTexture(-1)
    {
    }
};

Backend::Backend()
        : impl_(new Impl()),
          program_(0u),
          vao_(0u),
          vbo_(0u),
          ibo_(0u),
          textVao_(0u),
          textVbo_(0u),
          textIbo_(0u),
          atlasTexture_(0u),
          whiteTexture_(0u),
          quadVao_(0u),
          quadVbo_(0u),
          quadIbo_(0u),
          currentTarget_(-1),
          major_(0),
          minor_(0),
          drawCalls_(0),
          uploadedVertices_(0),
          windowWidth_(0),
          windowHeight_(0),
          atlas_(nullptr),
          atlasUploaded_(false),
          rgba_(),
          textScratch_()
{
    for (int i = 0; i < TargetCount; ++i)
    {
        query_[i][0] = 0u;
        query_[i][1] = 0u;
        lastGpuSeconds_[i] = 0.0;
        querySlot_[i] = 0;
        targetFramebuffer_[i] = 0u;
        targetTexture_[i] = 0u;
        targetWidth_[i] = 0;
        targetHeight_[i] = 0;
    }
}

Backend::~Backend()
{
    shutdown();
    delete impl_;
    impl_ = nullptr;
}

bool Backend::init(void* (*procAddress)(const char* name))
{
    if (!impl_ || !procAddress) return false;
    if (!impl_->gl.load(procAddress)) return false;

    GlFunctions& gl = impl_->gl;
    GLint major = 0;
    GLint minor = 0;
    if (gl.getIntegerv)
    {
        gl.getIntegerv(GL_MAJOR_VERSION, &major);
        gl.getIntegerv(GL_MINOR_VERSION, &minor);
    }
    major_ = major;
    minor_ = minor;
    if (major < 3 || (major == 3 && minor < 3)) return false;

    const GLuint vertexShader = compileShader(gl, GL_VERTEX_SHADER, kVertexShader);
    const GLuint fragmentShader = compileShader(gl, GL_FRAGMENT_SHADER, kFragmentShader);
    if (!vertexShader || !fragmentShader)
    {
        if (vertexShader) gl.deleteShader(vertexShader);
        if (fragmentShader) gl.deleteShader(fragmentShader);
        return false;
    }
    program_ = gl.createProgram();
    gl.attachShader(program_, vertexShader);
    gl.attachShader(program_, fragmentShader);
    gl.linkProgram(program_);
    gl.deleteShader(vertexShader);
    gl.deleteShader(fragmentShader);
    GLint linked = 0;
    gl.getProgramiv(program_, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        char log[1024];
        log[0] = '\0';
        if (gl.getProgramInfoLog)
            gl.getProgramInfoLog(program_, static_cast<GLsizei>(sizeof(log) - 1u), nullptr, log);
        fprintf(stderr, "[igui::opengl] link: %s\n", log);
        gl.deleteProgram(program_);
        program_ = 0u;
        return false;
    }

    impl_->uInvDisplay = gl.getUniformLocation(program_, "uInvDisplay");
    impl_->uTexture = gl.getUniformLocation(program_, "uTexture");

    gl.genVertexArrays(1, &vao_);
    gl.genBuffers(1, &vbo_);
    gl.genBuffers(1, &ibo_);
    configureVertexArray(gl, vao_, vbo_, ibo_);
    gl.genVertexArrays(1, &textVao_);
    gl.genBuffers(1, &textVbo_);
    gl.genBuffers(1, &textIbo_);
    configureVertexArray(gl, textVao_, textVbo_, textIbo_);
    gl.genVertexArrays(1, &quadVao_);
    gl.genBuffers(1, &quadVbo_);
    gl.genBuffers(1, &quadIbo_);
    configureVertexArray(gl, quadVao_, quadVbo_, quadIbo_);

    const unsigned char white[4] = { 255u, 255u, 255u, 255u };
    whiteTexture_ = uploadRgba(gl, 1, 1, white);

    if (gl.genQueries && gl.deleteQueries && gl.beginQuery && gl.endQuery && gl.getQueryObjectiv &&
            gl.getQueryObjectui64v)
    {
        for (int i = 0; i < TargetCount; ++i) gl.genQueries(2, query_[i]);
    }
    else
    {
        for (int i = 0; i < TargetCount; ++i) query_[i][0] = query_[i][1] = 0u;
    }
    return true;
}

void Backend::shutdown()
{
    if (!impl_ || !impl_->gl.clear) return;
    GlFunctions& gl = impl_->gl;
    for (int i = 0; i < TargetCount; ++i)
    {
        if (query_[i][0]) gl.deleteQueries(2, query_[i]);
        query_[i][0] = query_[i][1] = 0u;
    }
    if (whiteTexture_) gl.deleteTextures(1, &whiteTexture_);
    if (atlasTexture_) gl.deleteTextures(1, &atlasTexture_);
    whiteTexture_ = atlasTexture_ = 0u;
    if (textIbo_) gl.deleteBuffers(1, &textIbo_);
    if (textVbo_) gl.deleteBuffers(1, &textVbo_);
    if (textVao_) gl.deleteVertexArrays(1, &textVao_);
    if (quadIbo_) gl.deleteBuffers(1, &quadIbo_);
    if (quadVbo_) gl.deleteBuffers(1, &quadVbo_);
    if (quadVao_) gl.deleteVertexArrays(1, &quadVao_);
    if (ibo_) gl.deleteBuffers(1, &ibo_);
    if (vbo_) gl.deleteBuffers(1, &vbo_);
    if (vao_) gl.deleteVertexArrays(1, &vao_);
    if (targetFramebuffer_[0]) gl.deleteFramebuffers(1, &targetFramebuffer_[0]);
    if (targetFramebuffer_[1]) gl.deleteFramebuffers(1, &targetFramebuffer_[1]);
    if (targetTexture_[0]) gl.deleteTextures(1, &targetTexture_[0]);
    if (targetTexture_[1]) gl.deleteTextures(1, &targetTexture_[1]);
    for (int i = 0; i < TargetCount; ++i)
    {
        targetFramebuffer_[i] = 0u;
        targetTexture_[i] = 0u;
        targetWidth_[i] = 0;
        targetHeight_[i] = 0;
    }
    currentTarget_ = -1;
    if (program_) gl.deleteProgram(program_);
    program_ = vao_ = vbo_ = ibo_ = 0u;
    textVao_ = textVbo_ = textIbo_ = 0u;
    quadVao_ = quadVbo_ = quadIbo_ = 0u;
}

void Backend::setFontAtlas(ig::FontAtlas* atlas)
{
    if (atlas_ != atlas) atlasUploaded_ = false;
    atlas_ = atlas;
}

bool Backend::uploadAtlas()
{
    if (!atlas_ || !atlas_->valid()) return false;
    if (atlasUploaded_ && atlasTexture_ != 0u) return true;
    GlFunctions& gl = impl_->gl;

    const ig::Span<const uint8_t> pixels = atlas_->pixels();
    const int width = static_cast<int>(atlas_->width());
    const int height = static_cast<int>(atlas_->height());
    if (width <= 0 || height <= 0) return false;
    const ig::Span<const uint8_t>::size_type needed =
            static_cast<ig::Span<const uint8_t>::size_type>(width) *
            static_cast<ig::Span<const uint8_t>::size_type>(height);
    if (pixels.size() < needed) return false;

    // O atlas é uma máscara de 8 bits (um byte por pixel, a cobertura do glifo),
    // não RGBA: compõe-se branco com essa cobertura, exatamente como o backend de
    // SDL2 faz. Sem esta conversão uma textura RGBA ficava com um quarto dos
    // bytes e o upload abortava.
    const ig::Span<const uint8_t>::size_type bytes = needed * 4u;
    if (rgba_.size() < bytes) rgba_.resize(bytes);
    for (ig::Span<const uint8_t>::size_type i = 0; i < needed; ++i)
    {
        const ig::Span<const uint8_t>::size_type pixel = i * 4u;
        rgba_[pixel + 0u] = 255u;
        rgba_[pixel + 1u] = 255u;
        rgba_[pixel + 2u] = 255u;
        rgba_[pixel + 3u] = pixels[i];
    }

    if (!atlasTexture_) atlasTexture_ = uploadRgba(gl, width, height, rgba_.data());
    else
    {
        gl.bindTexture(GL_TEXTURE_2D, atlasTexture_);
        gl.pixelStorei(GL_UNPACK_ALIGNMENT, 1);
        gl.texSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                rgba_.data());
    }
    if (!atlasTexture_) return false;
    atlasUploaded_ = true;
    // O Contexto tira daqui o TextureId da geometria sólida (o texel branco do
    // atlas), por isso é o backend que diz ao atlas qual é o nome da sua textura
    // na GPU - como fazem os backends de SDL2 e Raylib.
    atlas_->setTexture(ig::TextureId(static_cast<uint64_t>(atlasTexture_)));
    return true;
}

void Backend::setWindowSize(int width, int height)
{
    windowWidth_ = width;
    windowHeight_ = height;
}

void Backend::clear(const ig::Color& color)
{
    if (!ready()) return;
    GlFunctions& gl = impl_->gl;
    gl.disable(GL_SCISSOR_TEST);
    gl.clearColor(static_cast<GLfloat>(color.r) / 255.0f, static_cast<GLfloat>(color.g) / 255.0f,
            static_cast<GLfloat>(color.b) / 255.0f, static_cast<GLfloat>(color.a) / 255.0f);
    gl.clear(GL_COLOR_BUFFER_BIT);
}

bool Backend::ensureTarget(int target, int width, int height)
{
    if (target < 0 || target >= TargetCount || width <= 0 || height <= 0) return false;
    GlFunctions& gl = impl_->gl;
    if (!targetFramebuffer_[target])
    {
        gl.genFramebuffers(1, &targetFramebuffer_[target]);
        gl.genTextures(1, &targetTexture_[target]);
    }
    if (targetWidth_[target] != width || targetHeight_[target] != height)
    {
        gl.bindTexture(GL_TEXTURE_2D, targetTexture_[target]);
        gl.texImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(GL_RGBA8), width, height, 0, GL_RGBA,
                GL_UNSIGNED_BYTE, nullptr);
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(GL_LINEAR));
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(GL_LINEAR));
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(GL_CLAMP_TO_EDGE));
        gl.texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(GL_CLAMP_TO_EDGE));
        gl.bindFramebuffer(GL_FRAMEBUFFER, targetFramebuffer_[target]);
        gl.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                targetTexture_[target], 0);
        if (gl.checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            gl.bindFramebuffer(GL_FRAMEBUFFER, 0u);
            return false;
        }
        targetWidth_[target] = width;
        targetHeight_[target] = height;
    }
    bindTarget(target);
    return true;
}

void Backend::bindTarget(int target)
{
    GlFunctions& gl = impl_->gl;
    if (target < 0 || target >= TargetCount || !targetFramebuffer_[target])
    {
        currentTarget_ = -1;
        gl.bindFramebuffer(GL_FRAMEBUFFER, 0u);
        gl.viewport(0, 0, windowWidth_, windowHeight_);
        return;
    }
    currentTarget_ = target;
    gl.bindFramebuffer(GL_FRAMEBUFFER, targetFramebuffer_[target]);
    gl.viewport(0, 0, targetWidth_[target], targetHeight_[target]);
}

bool Backend::beginTarget(int target, int width, int height)
{
    if (!ready()) return false;
    return ensureTarget(target, width, height);
}

void Backend::endTarget(int target, int x, int y, int width, int height)
{
    if (!ready() || target < 0 || target >= TargetCount || !targetFramebuffer_[target] ||
            width <= 0 || height <= 0)
        return;
    bindTarget(-1);
    copyTargetToWindow(target, x, y, width, height);
}

void Backend::copyTargetToWindow(int target, int x, int y, int width, int height)
{
    GlFunctions& gl = impl_->gl;

    // Um quad texturizado em coordenadas de pixel da janela: o shader é o mesmo
    // que desenha tudo o resto, com o uInvDisplay da janela em vez do do painel.
    // O uv é invertido no eixo do v porque a textura do target tem a origem em
    // baixo (como todo o GL) e o retângulo de destino é dado com o y para baixo.
    const float x0 = static_cast<float>(x);
    const float y0 = static_cast<float>(y);
    const float x1 = static_cast<float>(x + width);
    const float y1 = static_cast<float>(y + height);
    const ig::Color white(255u, 255u, 255u, 255u);
    const ig::DrawVertex vertices[4] = { { ig::Vec2(x0, y0), ig::Vec2(0.0f, 1.0f), white },
        { ig::Vec2(x1, y0), ig::Vec2(1.0f, 1.0f), white },
        { ig::Vec2(x1, y1), ig::Vec2(1.0f, 0.0f), white },
        { ig::Vec2(x0, y1), ig::Vec2(0.0f, 0.0f), white } };
    const ig::DrawIndex indices[6] = { 0u, 1u, 2u, 0u, 2u, 3u };

    gl.disable(GL_SCISSOR_TEST);
    gl.disable(GL_BLEND);
    gl.useProgram(program_);
    gl.uniform1i(impl_->uTexture, 0);
    gl.uniform2f(impl_->uInvDisplay,
            windowWidth_ > 0 ? 1.0f / static_cast<float>(windowWidth_) : 0.0f,
            windowHeight_ > 0 ? 1.0f / static_cast<float>(windowHeight_) : 0.0f);
    gl.activeTexture(GL_TEXTURE0);
    gl.bindTexture(GL_TEXTURE_2D, targetTexture_[target]);
    gl.bindVertexArray(quadVao_);
    gl.bindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    gl.bufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(vertices)), vertices,
            GL_STREAM_DRAW);
    gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadIbo_);
    gl.bufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(indices)), indices,
            GL_STREAM_DRAW);
    gl.drawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    gl.bindVertexArray(0u);
    gl.enable(GL_BLEND);
}

ig::TextMetrics Backend::measureText(ig::FontId font, ig::StringView text, float logicalSize, float)
{
    if (atlas_ && atlas_->valid()) return atlas_->measureText(font, text, logicalSize);
    // Sem atlas (o módulo de fonte não foi construído): avanço fixo, como os
    // outros backends fazem quando não têm de onde medir.
    ig::TextMetrics metrics;
    metrics.width = static_cast<float>(text.size()) * logicalSize * 0.5f;
    metrics.height = logicalSize;
    metrics.ascent = logicalSize * 0.78f;
    metrics.descent = logicalSize * 0.22f;
    return metrics;
}

void Backend::drawCommand(uint32_t firstIndex, uint32_t indexCount, const ig::Rect& clip,
        ig::TextureId texture, float scaleX, float scaleY)
{
    if (indexCount == 0u) return;
    GlFunctions& gl = impl_->gl;

    // O clip é em coordenadas lógicas e o scissor em pixels, com o eixo do y ao
    // contrário (o GL tem a origem em baixo).
    int x0 = static_cast<int>(floorf(clip.x * scaleX));
    int y0 = static_cast<int>(floorf(clip.y * scaleY));
    int x1 = static_cast<int>(ceilf((clip.x + clip.width) * scaleX));
    int y1 = static_cast<int>(ceilf((clip.y + clip.height) * scaleY));
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > currentWidth()) x1 = currentWidth();
    if (y1 > currentHeight()) y1 = currentHeight();
    if (x1 <= x0 || y1 <= y0) return;

    gl.enable(GL_SCISSOR_TEST);
    gl.scissor(x0, currentHeight() - y1, x1 - x0, y1 - y0);
    gl.bindTexture(GL_TEXTURE_2D,
            texture.value != 0u ? static_cast<GLuint>(texture.value) : whiteTexture_);
    gl.drawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT,
            reinterpret_cast<const void*>(
                    static_cast<uintptr_t>(firstIndex) * sizeof(ig::DrawIndex)));
    ++drawCalls_;
}

bool Backend::render(const ig::DrawData& data)
{
    if (!ready() || data.displaySize.x <= 0.0f || data.displaySize.y <= 0.0f) return false;
    GlFunctions& gl = impl_->gl;
    if (currentWidth() <= 0 || currentHeight() <= 0) return false;

    const bool hasAtlas = atlas_ && atlas_->valid();
    if (hasAtlas && !uploadAtlas()) return false;

    gl.disable(GL_DEPTH_TEST);
    gl.disable(GL_CULL_FACE);
    gl.enable(GL_BLEND);
    gl.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl.useProgram(program_);
    gl.uniform1i(impl_->uTexture, 0);
    gl.uniform2f(impl_->uInvDisplay, 1.0f / data.displaySize.x, 1.0f / data.displaySize.y);
    gl.activeTexture(GL_TEXTURE0);

    const float scaleX = static_cast<float>(currentWidth()) / data.displaySize.x;
    const float scaleY = static_cast<float>(currentHeight()) / data.displaySize.y;

    drawCalls_ = 0;
    uploadedVertices_ = static_cast<int>(data.vertices.size());

    gl.bindVertexArray(vao_);
    gl.bindBuffer(GL_ARRAY_BUFFER, vbo_);
    gl.bufferData(GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(data.vertices.size() * sizeof(ig::DrawVertex)),
            data.vertices.data(), GL_STREAM_DRAW);
    gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    gl.bufferData(GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(data.indices.size() * sizeof(ig::DrawIndex)),
            data.indices.data(), GL_STREAM_DRAW);

    for (ig::Span<const ig::DrawCommand>::size_type i = 0; i < data.commands.size(); ++i)
    {
        const ig::DrawCommand& command = data.commands[i];
        if (command.type != ig::DrawCommandType::Geometry) continue;
        drawCommand(command.payload.geometry.firstIndex, command.payload.geometry.indexCount,
                command.payload.geometry.clip, command.payload.geometry.texture, scaleX, scaleY);
    }
    gl.bindVertexArray(0u);

    // Comandos de texto que o Context não tenha resolvido: com um TextProvider
    // não sobra nenhum, mas sem ele (sem módulo de fonte) o backend é quem
    // desenha o texto.
    if (hasAtlas)
    {
        textScratch_.clear();
        bool any = false;
        for (ig::Span<const ig::DrawCommand>::size_type i = 0; i < data.commands.size(); ++i)
        {
            const ig::DrawCommand& command = data.commands[i];
            if (command.type != ig::DrawCommandType::Text) continue;
            const ig::TextCommand& text = command.payload.text;
            if (text.textOffset > data.textBytes.size() ||
                    text.textSize > data.textBytes.size() - text.textOffset)
                continue;
            const ig::StringView view(data.textBytes.data() + text.textOffset, text.textSize);
            if (atlas_->appendText(textScratch_, text.font, view, text.position, text.logicalSize,
                        text.color, text.clip))
                any = true;
        }
        if (any)
        {
            const ig::DrawData scratch = textScratch_.data(data.displaySize, data.dpiScale);
            gl.bindVertexArray(textVao_);
            gl.bindBuffer(GL_ARRAY_BUFFER, textVbo_);
            gl.bufferData(GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(scratch.vertices.size() * sizeof(ig::DrawVertex)),
                    scratch.vertices.data(), GL_STREAM_DRAW);
            gl.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, textIbo_);
            gl.bufferData(GL_ELEMENT_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(scratch.indices.size() * sizeof(ig::DrawIndex)),
                    scratch.indices.data(), GL_STREAM_DRAW);
            for (ig::Span<const ig::DrawCommand>::size_type i = 0; i < scratch.commands.size(); ++i)
            {
                const ig::DrawCommand& command = scratch.commands[i];
                if (command.type != ig::DrawCommandType::Geometry) continue;
                drawCommand(command.payload.geometry.firstIndex,
                        command.payload.geometry.indexCount, command.payload.geometry.clip,
                        command.payload.geometry.texture, scaleX, scaleY);
            }
            gl.bindVertexArray(0u);
            uploadedVertices_ += static_cast<int>(scratch.vertices.size());
        }
    }

    gl.disable(GL_SCISSOR_TEST);
    return true;
}

ig::TextureId Backend::createTexture(int width, int height, const unsigned char* rgba)
{
    if (!ready() || width <= 0 || height <= 0 || !rgba) return ig::TextureId();
    return ig::TextureId(uploadRgba(impl_->gl, width, height, rgba));
}

bool Backend::updateTexture(ig::TextureId texture, int width, int height, const unsigned char* rgba)
{
    if (!ready() || texture.value == 0u || width <= 0 || height <= 0 || !rgba) return false;
    GlFunctions& gl = impl_->gl;
    gl.bindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture.value));
    gl.pixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl.texSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    return true;
}

void Backend::destroyTexture(ig::TextureId texture)
{
    if (!ready() || texture.value == 0u) return;
    GLuint name = static_cast<GLuint>(texture.value);
    impl_->gl.deleteTextures(1, &name);
}

bool Backend::readPixels(int x, int y, int width, int height, unsigned char* rgba)
{
    if (!ready() || !rgba || width <= 0 || height <= 0) return false;
    GlFunctions& gl = impl_->gl;
    // Um erro pendente de outra chamada faria esta leitura parecer falhada.
    while (gl.getError() != GL_NO_ERROR)
    {
    }
    gl.pixelStorei(GL_PACK_ALIGNMENT, 1);
    // O GL devolve a linha de baixo primeiro; a aplicação quer a de cima.
    for (int row = 0; row < height; ++row)
    {
        gl.readPixels(x, currentHeight() - 1 - (y + row), width, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                rgba + static_cast<size_t>(row) * static_cast<size_t>(width) * 4u);
    }
    return gl.getError() == GL_NO_ERROR;
}

const char* Backend::renderer() const
{
    if (!impl_ || !impl_->gl.getString) return "";
    const GLubyte* name = impl_->gl.getString(GL_RENDERER);
    return name ? reinterpret_cast<const char*>(name) : "";
}

void Backend::gpuTimerBegin(int slot)
{
    if (!gpuTimerSupported() || slot < 0 || slot >= TargetCount) return;
    // A query escrita é a outra do slot: o resultado da anterior ainda pode
    // estar em trânsito e é lido sem esperar por ele.
    querySlot_[slot] = 1 - querySlot_[slot];
    impl_->gl.beginQuery(GL_TIME_ELAPSED, query_[slot][querySlot_[slot]]);
}

void Backend::gpuTimerEnd(int slot)
{
    if (!gpuTimerSupported() || slot < 0 || slot >= TargetCount) return;
    impl_->gl.endQuery(GL_TIME_ELAPSED);
}

double Backend::gpuSeconds(int slot)
{
    if (!gpuTimerSupported() || slot < 0 || slot >= TargetCount) return 0.0;
    GlFunctions& gl = impl_->gl;
    const GLuint previous = query_[slot][1 - querySlot_[slot]];
    GLint available = 0;
    gl.getQueryObjectiv(previous, GL_QUERY_RESULT_AVAILABLE, &available);
    if (available)
    {
        GLuint64 nanoseconds = 0u;
        gl.getQueryObjectui64v(previous, GL_QUERY_RESULT, &nanoseconds);
        lastGpuSeconds_[slot] = static_cast<double>(nanoseconds) / 1e9;
    }
    // Sem resultado pronto fica o último conhecido: o número de um frame atrás,
    // que é melhor do que zero para uma média.
    return lastGpuSeconds_[slot];
}

} // namespace opengl
} // namespace ig
