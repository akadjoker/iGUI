// Round 10, part 1 (docs/DRAW-CALLBACK.md): native draw callbacks on the OpenGL renderer of the zen adapter, read back as pixels.
// Runs only on the throwaway Xvfb of tools/xvfb-test.sh (ThrowawayDisplay.hpp; software GL, llvmpipe, is enough); 77 (skipped) where
// the display has no OpenGL 3.3 or cannot make the offscreen framebuffer.
//   1. idle (a real window on the Xvfb, frames that change nothing are not painted): the callback is called only in frames that are
//      painted, 0 times in 100 idle frames, once for a requestRepaint(), and calling it marks nothing dirty;
//   2. what the callback is given (framebuffer size, rectangles in both conventions, scissor, projections, backend, samples);
//   3. a known triangle inside the widget's rectangle, nothing drawn outside it by a callback that draws the whole target (the
//      scissor is the widget's clip), also when a scroll area clips the widget (only the visible part), and the order between
//      two widgets that overlap it (the one before is covered, the one after covers);
//   4. state: a callback that trashes the GL state (program, vertex array, buffers, textures, blend, scissor, viewport, depth,
//      cull, stencil, colour mask, framebuffer) with restoreState on, and one that puts back what it touched with it off: the
//      widgets drawn after them come out as without the callback;
//   5. with multisampling the callback is told the sample count of the target.
// No standard library: ct and C only.

#include "ThrowawayDisplay.hpp"

#include <igui_zen/ZenApp.hpp>

#include <glad/gl.h>

#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
#include "ScrollWidgets.hpp"
#include "ViewWidgets.hpp"

#include <igui/widgets/Theme.hpp>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

using namespace ig::retained;

static int failures = 0;
static void check(bool ok, const char* what)
{
    if (!ok)
    {
        fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

namespace
{

constexpr int kW = 220;
constexpr int kH = 140;
uint32_t gBackground = 0xff1e1e1eu; // what the window is filled with: the theme's bgColor (set in makeApp)

// ── what the callbacks do, chosen by Mode ───────────────────────────────────────────────────────────────────────────────
enum class Mode
{
    Nothing,
    Triangle, // the lower left half of the widget's rectangle, red, through a viewport of the rectangle
    Fill,     // the whole target, one colour: only the scissor keeps it inside the widget
    Trash     // changes a lot of GL state and draws nothing
};

struct Cb
{
    Mode mode = Mode::Nothing;
    float colour[4] = { 1, 0, 0, 1 };
    int calls = 0;
    NativeDrawInfo last;
};

unsigned gProgram = 0, gVao = 0, gVbo = 0, gColourLoc = 0;
unsigned gTrashProgram = 0, gTrashTex = 0, gTrashFbo = 0, gTrashVao = 0, gTrashVbo = 0;

unsigned compile(unsigned type, const char* body, int backend)
{
    const char* sources[2] = { backend == static_cast<int>(NativeBackend::OpenGLES3) ? "#version 300 es\n" : "#version 330 core\n", body };
    const unsigned shader = glCreateShader(type);
    glShaderSource(shader, 2, sources, nullptr);
    glCompileShader(shader);
    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        glGetShaderInfoLog(shader, sizeof log, nullptr, log);
        fprintf(stderr, "shader: %s\n", log);
    }
    return shader;
}

unsigned makeProgram(int backend)
{
    const unsigned vs = compile(GL_VERTEX_SHADER, "layout(location = 0) in vec2 aPos;\nvoid main() { gl_Position = vec4(aPos, 0.0, 1.0); }\n", backend);
    const unsigned fs = compile(GL_FRAGMENT_SHADER, "precision mediump float;\nuniform vec4 uColour;\nout vec4 o;\nvoid main() { o = uColour; }\n", backend);
    const unsigned program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    int linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        char log[512];
        glGetProgramInfoLog(program, sizeof log, nullptr, log);
        fprintf(stderr, "link: %s\n", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

void ensure(int backend)
{
    if (gProgram) return;
    gProgram = makeProgram(backend);
    gColourLoc = static_cast<unsigned>(glGetUniformLocation(gProgram, "uColour"));
    glGenVertexArrays(1, &gVao);
    glBindVertexArray(gVao);
    glGenBuffers(1, &gVbo);
    glBindBuffer(GL_ARRAY_BUFFER, gVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    // trash objects
    gTrashProgram = makeProgram(backend);
    glGenTextures(1, &gTrashTex);
    glGenVertexArrays(1, &gTrashVao);
    glBindVertexArray(gTrashVao);
    glGenBuffers(1, &gTrashVbo);
    glBindBuffer(GL_ARRAY_BUFFER, gTrashVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glGenFramebuffers(1, &gTrashFbo);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void drawVertices(const float* xy, int count, const float colour[4])
{
    glUseProgram(gProgram);
    glUniform4fv(static_cast<int>(gColourLoc), 1, colour);
    glBindVertexArray(gVao);
    glBindBuffer(GL_ARRAY_BUFFER, gVbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<long>(count) * 2 * sizeof(float), xy, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, count);
    const unsigned err = glGetError();
    int linked = 0;
    glGetProgramiv(gProgram, GL_LINK_STATUS, &linked);
    check(err == GL_NO_ERROR && linked, "the callback's draw raised no GL error and its program linked");
    if (err != GL_NO_ERROR || !linked) fprintf(stderr, "   GL error 0x%x, linked %d\n", err, linked);
}

void callback(const NativeDrawInfo& info)
{
    Cb* cb = static_cast<Cb*>(info.user);
    cb->last = info;
    ++cb->calls;
    ensure(info.backend);
    switch (cb->mode)
    {
        case Mode::Nothing: break;
        case Mode::Triangle:
        {
            glViewport(info.rectGl[0], info.rectGl[1], info.rectGl[2], info.rectGl[3]);
            const float v[6] = { -1, -1, 1, -1, -1, 1 };
            drawVertices(v, 3, cb->colour);
            break;
        }
        case Mode::Fill:
        {
            glViewport(0, 0, info.framebufferWidth, info.framebufferHeight);
            const float v[12] = { -1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1 };
            drawVertices(v, 6, cb->colour);
            break;
        }
        case Mode::Trash:
        {
            // every piece of state iGUI relies on, changed
            glUseProgram(gTrashProgram);
            glBindVertexArray(gTrashVao);
            glBindBuffer(GL_ARRAY_BUFFER, gTrashVbo);
            glActiveTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_2D, gTrashTex);
            glDisable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ZERO);
            glBlendEquation(GL_FUNC_SUBTRACT);
            glDisable(GL_SCISSOR_TEST);
            glScissor(0, 0, 3, 3);
            glViewport(0, 0, 3, 3);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            glEnable(GL_STENCIL_TEST);
            glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
            glBindFramebuffer(GL_FRAMEBUFFER, gTrashFbo);
            break;
        }
    }
}

// What a trashing callback with restoreState false does to put things back: the state it changes, put back as iGUI had it. The
// framebuffer is the one the window draws to: 0 here, the offscreen target's in these tests (read from the binding at entry).
void trashAndRestore(const NativeDrawInfo& info)
{
    Cb* cb = static_cast<Cb*>(info.user);
    cb->last = info;
    ++cb->calls;
    ensure(info.backend);
    int fbo = 0, viewport[4] = { 0, 0, 0, 0 }, scissor[4] = { 0, 0, 0, 0 };
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetIntegerv(GL_SCISSOR_BOX, scissor);
    check(glIsEnabled(GL_BLEND) && glIsEnabled(GL_SCISSOR_TEST) && !glIsEnabled(GL_DEPTH_TEST) && !glIsEnabled(GL_CULL_FACE) && !glIsEnabled(GL_STENCIL_TEST),
            "a callback is entered with blend and scissor on and depth, cull and stencil off");
    check(viewport[0] == 0 && viewport[1] == 0 && viewport[2] == info.framebufferWidth && viewport[3] == info.framebufferHeight, "... and the viewport is the whole target");
    check(scissor[0] == info.scissorGl[0] && scissor[1] == info.scissorGl[1] && scissor[2] == info.scissorGl[2] && scissor[3] == info.scissorGl[3],
            "... and the scissor is the box it was told");
    int program = -1, vao = -1, buffer = -1, texture = -1;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    check(program == 0 && buffer == 0 && texture == 0, "... with no program, array buffer or texture of iGUI bound (a clean slate)");
    check(vao != 0, "... and a vertex array of its own bound (core profile needs one)");
    glUseProgram(gTrashProgram);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, 5, 5);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_STENCIL_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    // ... and everything of the list put back
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glUseProgram(0);
}

// ── a window, scenes, and the pixels of a scene ─────────────────────────────────────────────────────────────────────────────────
struct Px
{
    ct::Vector<uint32_t> p;
    int w = 0, h = 0;
    uint32_t at(int x, int y) const
    {
        return p[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
    }
};

bool pumpAndRead(ig::zen::App& zen, Px& out, int frames = 8)
{
    for (int i = 0; i < frames; ++i)
        if (!zen.pump()) return false;
    return zen.readWindowPixels(out.p, out.w, out.h) && out.w == kW && out.h == kH;
}

Widget* newScene(ig::zen::App& zen, const char* name)
{
    Widget* root = zen.widgets().addStage(name);
    zen.widgets().setTransition(TransitionType::None, 0.0f);
    zen.widgets().setStage(name);
    Widget* holder = root->createChild<Widget>();
    holder->setRect({ 0, 0, static_cast<float>(kW), static_cast<float>(kH) });
    return holder;
}

bool makeApp(ig::zen::App& zen, int msaa = 0)
{
    ig::zen::App::Config config;
    config.title = "iGUI native callback test";
    config.width = kW;
    config.height = kH;
    config.preferOpenGL = true;
    config.vsync = false;
    config.showStats = false;
    config.msaa = msaa;
    config.defaultDynamicPage = false;
    if (!zen.create(config)) return false;
    gProgram = gVao = gVbo = gColourLoc = gTrashProgram = gTrashTex = gTrashFbo = gTrashVao = gTrashVbo = 0; // (objects of a context that is gone)
    const Color bg = zen.widgets().bgColor();
    gBackground = 0xff000000u | (static_cast<uint32_t>(bg.r) << 16) | (static_cast<uint32_t>(bg.g) << 8) | static_cast<uint32_t>(bg.b);
    return true;
}

bool near(uint32_t a, uint32_t b, int tolerance = 2)
{
    for (int shift = 0; shift < 24; shift += 8)
    {
        const int d = static_cast<int>((a >> shift) & 0xff) - static_cast<int>((b >> shift) & 0xff);
        if (d > tolerance || d < -tolerance) return false;
    }
    return true;
}

uint32_t rgb(int r, int g, int b)
{
    return 0xff000000u | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

// how many pixels of a rectangle (x0, y0 inclusive; x1, y1 exclusive) are not `colour`
int notColour(const Px& px, int x0, int y0, int x1, int y1, uint32_t colour)
{
    int n = 0;
    for (int y = y0 < 0 ? 0 : y0; y < y1 && y < px.h; ++y)
        for (int x = x0 < 0 ? 0 : x0; x < x1 && x < px.w; ++x)
            if (!near(px.at(x, y), colour)) ++n;
    return n;
}

int differs(const Px& a, const Px& b)
{
    int n = 0;
    for (size_t i = 0; i < a.p.size() && i < b.p.size(); ++i)
        if (a.p[i] != b.p[i]) ++n;
    return n;
}

// ── 1. idle ───────────────────────────────────────────────────────────────────────────────────────────────────────────────
void testIdle()
{
    printf("-- idle: called only in painted frames\n");
    ig::zen::App zen;
    if (!makeApp(zen))
    {
        printf("   no window with OpenGL here: skipped\n");
        return;
    }
    if (!zen.usesOpenGL())
    {
        printf("   no OpenGL: skipped\n");
        return;
    }
    check(NativeDrawAvailable(), "NativeDrawAvailable() is true as soon as the OpenGL application exists (before any paint)");
    check(GetIO().nativeBackend == static_cast<int>(zen.gl().nativeBackend()), "... with the backend of the renderer");
    zen.setSkipIdleFrames(true);
    Widget* holder = newScene(zen, "idle");
    auto* view = holder->createChild<NativeView>();
    view->setRect({ 20, 20, 100, 60 });
    Cb cb;
    cb.mode = Mode::Nothing;
    view->setNativeDraw(callback, &cb);
    check(view->isNative(), "the view is native");
    for (int i = 0; i < 20; ++i) zen.pump();
    check(cb.calls >= 1, "the callback ran in the frames painted while the window settled");
    const int calls0 = cb.calls;
    const int painted0 = zen.paintedFrames();
    for (int i = 0; i < 100; ++i) zen.pump();
    char what[160];
    snprintf(what, sizeof what, "100 idle frames: no frame painted (%d) and no callback (%d)", zen.paintedFrames() - painted0, cb.calls - calls0);
    check(zen.paintedFrames() == painted0 && cb.calls == calls0, what);
    view->requestRepaint();
    for (int i = 0; i < 6; ++i) zen.pump();
    snprintf(what, sizeof what, "requestRepaint(): one frame painted (%d) and the callback called once (%d)", zen.paintedFrames() - painted0, cb.calls - calls0);
    check(zen.paintedFrames() == painted0 + 1 && cb.calls == calls0 + 1, what);
    for (int i = 0; i < 30; ++i) zen.pump();
    check(zen.paintedFrames() == painted0 + 1 && cb.calls == calls0 + 1, "... and then none: calling the callback marked nothing dirty");
    view->setRect({ 20, 20, 110, 60 });
    for (int i = 0; i < 6; ++i) zen.pump();
    check(zen.paintedFrames() == painted0 + 2 && cb.calls == calls0 + 2, "a change of the widget: one more frame, one more call");
    check(cb.calls <= zen.paintedFrames(), "never more calls than frames painted");
}

// ── 2. what it is given, 3. pixels, 4. state, 5. samples ─────────────────────────────────────────────────────────────────────
void testInfo(ig::zen::App& zen)
{
    printf("-- what the callback is given\n");
    Widget* holder = newScene(zen, "info");
    auto* view = holder->createChild<NativeView>();
    view->setRect({ 40, 30, 80, 60 });
    Cb cb;
    view->setNativeDraw(callback, &cb);
    Px px;
    check(pumpAndRead(zen, px), "the scene is drawn and read back");
    check(cb.calls >= 1, "the callback was called");
    const NativeDrawInfo& i = cb.last;
    check(i.user == &cb, "it gets its user pointer");
    check(i.framebufferWidth == kW && i.framebufferHeight == kH, "the framebuffer size in pixels");
    check(i.backend == static_cast<int>(zen.gl().nativeBackend()) && i.backend != 0, "the backend tag is the renderer's");
    check(i.samples == zen.gl().targetSamples() && GetIO().nativeSamples == i.samples, "the sample count of the target");
    check(i.pixelScaleX == 1.0f && i.pixelScaleY == 1.0f, "the pixel scale");
    check(i.rect.x == 40.0f && i.rect.y == 30.0f && i.rect.w == 80.0f && i.rect.h == 60.0f, "the widget rectangle in pixels, origin top left");
    check(i.clip.x == 40.0f && i.clip.y == 30.0f && i.clip.w == 80.0f && i.clip.h == 60.0f, "the clip in pixels (the widget's: nothing clips it more)");
    check(i.rectGl[0] == 40 && i.rectGl[1] == kH - 90 && i.rectGl[2] == 80 && i.rectGl[3] == 60, "the rectangle for glViewport, origin bottom left");
    check(i.scissorGl[0] == 40 && i.scissorGl[1] == kH - 90 && i.scissorGl[2] == 80 && i.scissorGl[3] == 60, "the scissor box, origin bottom left");
    // the pixel projection maps the corners of the rectangle to clip space; the logical one gives the same at scale 1
    auto map = [](const float* m, float x, float y, float& ox, float& oy)
    {
        ox = m[0] * x + m[4] * y + m[12];
        oy = m[1] * x + m[5] * y + m[13];
    };
    float ax, ay, bx, by;
    map(i.pixelProjection, 40.0f, 30.0f, ax, ay);
    check(fabsf(ax - (40.0f / kW * 2.0f - 1.0f)) < 1e-5f && fabsf(ay - (1.0f - 30.0f / kH * 2.0f)) < 1e-5f, "the pixel projection maps the rectangle's top left to clip space");
    map(i.projection, 40.0f, 30.0f, bx, by);
    check(fabsf(ax - bx) < 1e-5f && fabsf(ay - by) < 1e-5f, "and the logical one the same (scale 1)");
    map(i.projection, 120.0f, 90.0f, bx, by);
    check(fabsf(bx - (120.0f / kW * 2.0f - 1.0f)) < 1e-5f && fabsf(by - (1.0f - 90.0f / kH * 2.0f)) < 1e-5f, "the bottom right too");
    check(i.restoreState, "restoreState is on by default");
    check(notColour(px, 0, 0, kW, kH, gBackground) == 0, "a callback that draws nothing leaves the window as it was");
}

void testTriangle(ig::zen::App& zen)
{
    printf("-- a known triangle inside the rectangle\n");
    Widget* holder = newScene(zen, "triangle");
    auto* view = holder->createChild<NativeView>();
    view->setRect({ 40, 30, 80, 60 });
    Cb cb;
    cb.mode = Mode::Triangle;
    view->setNativeDraw(callback, &cb);
    Px px;
    check(pumpAndRead(zen, px), "drawn and read back");
    const uint32_t red = rgb(255, 0, 0);
    // lower left half of the rectangle: u < v (u across, v down). Points away from the diagonal.
    int inside = 0, insideWrong = 0, other = 0, otherWrong = 0;
    for (int y = 30; y < 90; ++y)
        for (int x = 40; x < 120; ++x)
        {
            const float u = (static_cast<float>(x - 40) + 0.5f) / 80.0f, v = (static_cast<float>(y - 30) + 0.5f) / 60.0f;
            if (fabsf(u - v) < 0.05f) continue;
            if (u < v)
            {
                ++inside;
                if (!near(px.at(x, y), red)) ++insideWrong;
            }
            else
            {
                ++other;
                if (!near(px.at(x, y), gBackground)) ++otherWrong;
            }
        }
    char what[140];
    snprintf(what, sizeof what, "the lower left half is red (%d of %d pixels are not)", insideWrong, inside);
    check(inside > 1000 && insideWrong == 0, what);
    snprintf(what, sizeof what, "the upper right half is untouched (%d of %d pixels are not)", otherWrong, other);
    check(other > 1000 && otherWrong == 0, what);
    int outside = notColour(px, 0, 0, kW, 29, gBackground) + notColour(px, 0, 91, kW, kH, gBackground) + notColour(px, 0, 29, 39, 91, gBackground) +
                  notColour(px, 121, 29, kW, 91, gBackground);
    snprintf(what, sizeof what, "nothing outside the rectangle is drawn (%d pixels)", outside);
    check(outside == 0, what);
}

void testScissor(ig::zen::App& zen)
{
    printf("-- the clip is the scissor\n");
    {
        Widget* holder = newScene(zen, "fill");
        auto* view = holder->createChild<NativeView>();
        view->setRect({ 40, 30, 80, 60 });
        Cb cb;
        cb.mode = Mode::Fill;
        cb.colour[0] = 0.0f;
        cb.colour[2] = 1.0f;
        view->setNativeDraw(callback, &cb);
        Px px;
        check(pumpAndRead(zen, px), "a callback that fills the whole target: drawn");
        const uint32_t blue = rgb(0, 0, 255);
        char what[140];
        const int in = notColour(px, 40, 30, 120, 90, blue);
        snprintf(what, sizeof what, "inside the rectangle it is all blue (%d pixels are not)", in);
        check(in == 0, what);
        const int out = notColour(px, 0, 0, kW, 30, gBackground) + notColour(px, 0, 90, kW, kH, gBackground) + notColour(px, 0, 30, 40, 90, gBackground) +
                        notColour(px, 120, 30, kW, 90, gBackground);
        snprintf(what, sizeof what, "outside it is untouched (%d pixels are not)", out);
        check(out == 0, what);
    }
    {
        // a scroll area: the view's rectangle reaches 30 pixels below the area; the callback gets the clip, which is the area's
        Widget* holder = newScene(zen, "scroll");
        auto* area = holder->createChild<ScrollView>();
        area->setRect({ 20, 20, 120, 80 });
        Widget* content = area->setContent<Widget>();
        content->setRect({ 0, 0, 100, 300 });
        auto* view = content->createChild<NativeView>();
        view->setRect({ 10, 40, 80, 70 }); // content coordinates: 40..110 in a view that shows 0..80 of it
        Cb cb;
        cb.mode = Mode::Fill;
        cb.colour[1] = 1.0f;
        cb.colour[0] = 0.0f;
        view->setNativeDraw(callback, &cb);
        Px px;
        check(pumpAndRead(zen, px), "a native view in a scroll area, partly out of it: drawn");
        const NativeDrawInfo& i = cb.last;
        const Rect abs = view->absoluteRect();
        check(i.rect.x == abs.x && i.rect.y == abs.y && i.rect.w == abs.w && i.rect.h == abs.h, "the rectangle given is the whole widget's, even where it is out of the clip");
        const float visBottom = 100.0f; // the area's bottom edge
        check(abs.y + abs.h > visBottom, "(the widget reaches below the area)");
        check(i.clip.y + i.clip.h <= visBottom + 0.5f && i.clip.h < i.rect.h, "the clip given is cut by the scroll area");
        const uint32_t green = rgb(0, 255, 0);
        const int x0 = static_cast<int>(abs.x), x1 = static_cast<int>(abs.x + abs.w), y0 = static_cast<int>(abs.y), y1 = static_cast<int>(visBottom - 1);
        char what[160];
        const int in = notColour(px, x0, y0 + 1, x1, y1, green);
        snprintf(what, sizeof what, "the visible part of the view is filled (%d pixels are not)", in);
        check(in == 0, what);
        // below the area, where the view would go on: untouched (the scroll bar is on the right of the area: not looked at)
        const int below = notColour(px, x0, static_cast<int>(visBottom) + 2, x1, static_cast<int>(abs.y + abs.h), gBackground);
        snprintf(what, sizeof what, "the part of the view below the scroll area is untouched (%d pixels are not)", below);
        check(below == 0, what);
        const int left = notColour(px, 0, 0, x0 - 1, kH, gBackground);
        check(left == 0, "and nothing is drawn left of the view");
    }
}

void testOrder(ig::zen::App& zen)
{
    printf("-- the order between overlapping widgets\n");
    Widget* holder = newScene(zen, "order");
    auto* a = holder->createChild<Panel>();
    a->setRect({ 10, 10, 120, 100 });
    a->setBgColor(Color(0, 160, 0, 255));
    auto* view = holder->createChild<NativeView>();
    view->setRect({ 40, 30, 100, 60 });
    Cb cb;
    cb.mode = Mode::Fill;
    view->setNativeDraw(callback, &cb);
    auto* b = holder->createChild<Panel>();
    b->setRect({ 100, 50, 80, 70 });
    b->setBgColor(Color(30, 70, 200, 255));
    Px px;
    check(pumpAndRead(zen, px), "drawn");
    const uint32_t red = rgb(255, 0, 0), green = rgb(0, 160, 0), blue = rgb(30, 70, 200);
    check(near(px.at(60, 50), red), "inside the view and inside the panel before it: the callback's colour (it is drawn after the panel)");
    check(near(px.at(20, 20), green), "the panel before it, outside the view: its own colour");
    check(near(px.at(120, 70), blue), "where the panel after it overlaps the view: the panel's colour (it covers the callback)");
    check(near(px.at(160, 100), blue), "the panel after it, outside the view");
    check(near(px.at(135, 40), red), "the view, outside both panels: the callback's colour (nothing under it but the window)");
    check(near(px.at(200, 20), gBackground), "outside everything: the window");
}

void testState(ig::zen::App& zen)
{
    printf("-- the GL state after a callback\n");
    // the same scene three times: a callback that does nothing, one that trashes the state with restoreState on, and one that
    // changes state and puts it back with it off; what is drawn after the view (a translucent panel, a clipped panel, a label)
    // must come out the same.
    auto build = [&](const char* name, NativeDrawFn fn, Cb* cb, bool restore, Px& px)
    {
        Widget* holder = newScene(zen, name);
        auto* first = holder->createChild<Panel>();
        first->setRect({ 10, 10, 100, 40 });
        first->setBgColor(Color(120, 40, 40, 255));
        auto* view = holder->createChild<NativeView>();
        view->setRect({ 60, 20, 60, 40 });
        view->setNativeDraw(fn, cb, restore);
        auto* glass = holder->createChild<Panel>();
        glass->setRect({ 50, 40, 100, 60 });
        glass->setBgColor(Color(20, 200, 220, 120)); // translucent: needs blending
        auto* label = holder->createChild<Label>("Hamburgefonstiv");
        label->setRect({ 20, 100, 180, 24 });
        auto* box = holder->createChild<Panel>();
        box->setRect({ 150, 10, 40, 40 });
        box->setBgColor(Color(200, 200, 40, 255));
        return pumpAndRead(zen, px);
    };
    Cb nothing;
    Px baseline;
    check(build("state0", callback, &nothing, true, baseline), "the baseline scene (a callback that draws nothing)");
    check(nothing.calls >= 1, "the callback was called");
    Cb trash;
    trash.mode = Mode::Trash;
    Px trashed;
    check(build("state1", callback, &trash, true, trashed), "the scene with a callback that trashes the state, restoreState on");
    check(trash.calls >= 1, "the trashing callback was called");
    char what[200];
    snprintf(what, sizeof what, "every pixel is the same as without the trashing callback (%d differ)", differs(baseline, trashed));
    check(differs(baseline, trashed) == 0, what);
    check(notColour(baseline, 55, 45, 145, 95, gBackground) > 1000, "(the translucent panel and the rest were really drawn in the baseline)");
    Cb polite;
    Px politePx;
    check(build("state2", trashAndRestore, &polite, false, politePx), "the scene with a callback that puts back what it touched, restoreState off");
    snprintf(what, sizeof what, "every pixel is the same as without it (%d differ)", differs(baseline, politePx));
    check(differs(baseline, politePx) == 0, what);
    // the same trash, again, over several frames and with a window that is repainted: the state does not leak from one frame
    // into the next
    Px again;
    check(pumpAndRead(zen, again, 6) && differs(baseline, politePx) == 0 && differs(again, politePx) == 0, "and a later frame draws the same");
}

void testSamples()
{
    printf("-- the sample count\n");
    ig::zen::App zen;
    if (!makeApp(zen, 4) || !zen.usesOpenGL())
    {
        printf("   no multisampled target here: skipped\n");
        return;
    }
    Widget* holder = newScene(zen, "samples");
    auto* view = holder->createChild<NativeView>();
    view->setRect({ 10, 10, 50, 50 });
    Cb cb;
    view->setNativeDraw(callback, &cb);
    Px px;
    check(pumpAndRead(zen, px), "drawn with 4 samples asked for");
    check(cb.calls >= 1 && cb.last.samples == zen.gl().targetSamples(), "the callback is given the sample count the target really has");
    printf("   (the target has %d samples)\n", cb.last.samples);
    Cb tri;
    tri.mode = Mode::Triangle;
    view->setNativeDraw(callback, &tri);
    check(pumpAndRead(zen, px), "a triangle into the multisampled target");
    check(near(px.at(15, 55), rgb(255, 0, 0)), "is resolved when read back");
}

} // namespace

int main()
{
    // first of all: no window on a display that is not a throwaway one
    if (!igui_test::onThrowawayDisplay("test_zen_native_callback_gl")) return 77;

    // 1. a real window of the Xvfb, before the offscreen ones (the skipping of idle frames is decided by the first frame of the
    // process, and a pending screenshot, which the offscreen target needs, turns it off)
    testIdle();
    if (failures)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }

    char path[] = "/tmp/igui_zen_native_callback_XXXXXX";
    const int fd = mkstemp(path);
    if (fd < 0) return 77;
    close(fd);
    setenv("ZEN_SCREENSHOT", path, 1);
    setenv("ZEN_SCREENSHOT_BACKEND", "gl", 1);
    setenv("ZEN_SCREENSHOT_FRAME", "100000", 1); // (no picture is written: the pixels are read back by the test)
    {
        ig::zen::App zen;
        if (!makeApp(zen))
        {
            fprintf(stderr, "test_zen_native_callback_gl: no OpenGL context, or no offscreen framebuffer, on this X server: skipped\n");
            remove(path);
            return 77;
        }
        check(zen.usesOpenGL(), "the offscreen application draws with OpenGL");
        testInfo(zen);
        testTriangle(zen);
        testScissor(zen);
        testOrder(zen);
        testState(zen);
    }
    testSamples();
    remove(path);
    if (failures)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_zen_native_callback_gl: ok\n");
    return 0;
}
