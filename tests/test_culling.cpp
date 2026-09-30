// Off-clip culling: DrawList drops a primitive whose geometry cannot touch
// its clip, and the immediate widgets inherit that for free.
//
// The core property is checked by brute force: every primitive type is
// emitted against thousands of random clips, and whenever it is dropped its
// real geometry (the same call with an unbounded clip) must not overlap the
// scissor rectangle a backend would set - clip rounded outward to whole
// pixels. A primitive that is kept must come out exactly as without culling.

#include "InteractionHarness.hpp"

#include <math.h>
#include <stdio.h>

namespace
{

int gFailures = 0;

void check(bool condition, const char *what)
{
    if (!condition)
    {
        ++gFailures;
        printf("FAIL %s\n", what);
    }
}

// Small deterministic generator, so a failure reproduces.
uint32_t gSeed = 12345u;

float randomFloat(float minimum, float maximum)
{
    gSeed = gSeed * 1664525u + 1013904223u;
    const float unit = static_cast<float>(gSeed >> 8) / 16777216.0f;
    return minimum + (maximum - minimum) * unit;
}

enum Primitive
{
    PLine,
    PRect,
    PRectFilled,
    PRectGradient,
    PImage,
    PCircleFilled,
    PTriangle,
    PRectFilledRounded,
    PRectRounded,
    PArc,
    PCount
};

const char *const kNames[PCount] = {
    "addLine", "addRect", "addRectFilled", "addRectGradient", "addImage", "addCircleFilled",
    "addTriangleFilled", "addRectFilledRounded", "addRectRounded", "addArc"};

struct Shape
{
    ig::Vec2 a;
    ig::Vec2 b;
    ig::Vec2 c;
    float size;
    float thickness;
    float radius;
};

void emit(ig::DrawList &list, int primitive, const Shape &s, const ig::Rect &clip)
{
    const ig::Color color(200, 100, 50, 255);
    const ig::Rect rect(s.a.x, s.a.y, s.size, s.size * 0.6f);
    switch (primitive)
    {
    case PLine: list.addLine(s.a, s.b, color, clip, s.thickness); break;
    case PRect: list.addRect(rect, color, clip, s.thickness); break;
    case PRectFilled: list.addRectFilled(rect, color, clip); break;
    case PRectGradient: list.addRectGradient(rect, color, color, color, color, clip); break;
    case PImage:
        list.addImage(ig::TextureId(7u), rect, ig::Vec2(0.0f, 0.0f), ig::Vec2(1.0f, 1.0f), color, clip);
        break;
    case PCircleFilled: list.addCircleFilled(s.a, s.radius, color, clip); break;
    case PTriangle: list.addTriangleFilled(s.a, s.b, s.c, color, clip); break;
    case PRectFilledRounded: list.addRectFilledRounded(rect, s.radius, color, clip); break;
    case PRectRounded: list.addRectRounded(rect, s.radius, color, clip, s.thickness); break;
    case PArc: list.addArc(s.a, s.radius, 0.3f, 2.4f, color, clip, s.thickness); break;
    default: break;
    }
}

struct Box
{
    float minX;
    float minY;
    float maxX;
    float maxY;
};

// What the SDL2/raylib backends scissor to: floor the origin, ceil the far edge.
bool touchesScissor(const Box &box, const ig::Rect &clip)
{
    if (!(clip.width > 0.0f) || !(clip.height > 0.0f))
        return false;
    const float left = floorf(clip.x);
    const float top = floorf(clip.y);
    const float right = ceilf(clip.x + clip.width);
    const float bottom = ceilf(clip.y + clip.height);
    return box.maxX > left && box.minX < right && box.maxY > top && box.minY < bottom;
}

struct Triangle
{
    ig::Vec2 p[3];
};

ct::Vector<Triangle> triangles(const ig::DrawData &data)
{
    ct::Vector<Triangle> result;
    for (size_t c = 0; c < data.commands.size(); ++c)
    {
        if (data.commands[c].type != ig::DrawCommandType::Geometry)
            continue;
        const ig::GeometryCommand &g = data.commands[c].payload.geometry;
        for (uint32_t t = 0; t + 2u < g.indexCount; t += 3u)
        {
            Triangle triangle;
            for (uint32_t k = 0; k < 3u; ++k)
                triangle.p[k] = data.vertices[data.indices[g.firstIndex + t + k] + g.vertexOffset].position;
            result.push_back(triangle);
        }
    }
    return result;
}

Box bounds(const Triangle &triangle)
{
    Box box = {1e30f, 1e30f, -1e30f, -1e30f};
    for (int k = 0; k < 3; ++k)
    {
        const ig::Vec2 &p = triangle.p[k];
        box.minX = p.x < box.minX ? p.x : box.minX;
        box.minY = p.y < box.minY ? p.y : box.minY;
        box.maxX = p.x > box.maxX ? p.x : box.maxX;
        box.maxY = p.y > box.maxY ? p.y : box.maxY;
    }
    return box;
}

bool sameTriangle(const Triangle &a, const Triangle &b)
{
    for (int k = 0; k < 3; ++k)
        if (a.p[k].x != b.p[k].x || a.p[k].y != b.p[k].y)
            return false;
    return true;
}

// Compound primitives (addRect, addRectRounded are made of addLine calls)
// are culled piece by piece, so the property is per triangle: every
// triangle of the unclipped geometry that can put a pixel inside the
// scissor must still be emitted, unchanged.
void test_culling_never_drops_visible_geometry()
{
    const ig::Rect unbounded(-1e6f, -1e6f, 2e6f, 2e6f);
    for (int primitive = 0; primitive < PCount; ++primitive)
    {
        size_t dropped = 0u;
        size_t visible = 0u;
        for (int iteration = 0; iteration < 3000; ++iteration)
        {
            Shape s;
            s.a = ig::Vec2(randomFloat(0.0f, 200.0f), randomFloat(0.0f, 200.0f));
            s.b = ig::Vec2(randomFloat(0.0f, 200.0f), randomFloat(0.0f, 200.0f));
            s.c = ig::Vec2(randomFloat(0.0f, 200.0f), randomFloat(0.0f, 200.0f));
            s.size = randomFloat(1.0f, 60.0f);
            s.thickness = randomFloat(0.5f, 6.0f);
            s.radius = randomFloat(0.5f, 30.0f);
            // Clips from far away to overlapping, with fractional edges and
            // some with no area at all.
            const float clipWidth = iteration % 17 == 0 ? 0.0f : randomFloat(0.5f, 120.0f);
            const float clipHeight = iteration % 23 == 0 ? 0.0f : randomFloat(0.5f, 120.0f);
            const ig::Rect clip(randomFloat(-80.0f, 260.0f), randomFloat(-80.0f, 260.0f),
                                clipWidth, clipHeight);

            ig::DrawList reference;
            emit(reference, primitive, s, unbounded);
            ig::DrawList list;
            emit(list, primitive, s, clip);
            const ct::Vector<Triangle> full = triangles(reference.data(ig::Vec2(1.0f, 1.0f), 1.0f));
            const ct::Vector<Triangle> kept = triangles(list.data(ig::Vec2(1.0f, 1.0f), 1.0f));
            dropped += full.size() - kept.size();

            for (size_t i = 0; i < full.size(); ++i)
            {
                if (!touchesScissor(bounds(full[i]), clip))
                    continue;
                ++visible;
                bool found = false;
                for (size_t j = 0; j < kept.size() && !found; ++j)
                    found = sameTriangle(full[i], kept[j]);
                if (!found)
                {
                    char message[160];
                    snprintf(message, sizeof(message),
                             "%s dropped a triangle inside clip (%g,%g %gx%g)",
                             kNames[primitive], clip.x, clip.y, clip.width, clip.height);
                    check(false, message);
                    break;
                }
            }
        }
        // Both outcomes must actually occur, or the test proves nothing.
        char message[96];
        snprintf(message, sizeof(message), "%s: random clips exercised both paths", kNames[primitive]);
        check(dropped > 200u && visible > 200u, message);
    }
}

void test_text_command_is_culled_only_when_certainly_invisible()
{
    const ig::Rect clip(100.0f, 100.0f, 200.0f, 100.0f);
    struct Case
    {
        ig::Vec2 position;
        bool emitted;
    };
    // Width/line count are the backend's, so only right-of and below the
    // clip can be ruled out; text that starts above/left may still run in.
    const Case cases[] = {
        {ig::Vec2(150.0f, 150.0f), true},
        {ig::Vec2(-500.0f, 150.0f), true},
        {ig::Vec2(150.0f, -500.0f), true},
        {ig::Vec2(301.0f, 150.0f), true}, // inside the rounding margin
        {ig::Vec2(310.0f, 150.0f), false},
        {ig::Vec2(150.0f, 210.0f), false},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        ig::DrawList list;
        list.addText("text", cases[i].position, ig::FontId(), 16.0f, ig::Color(), clip);
        const bool emitted = list.data(ig::Vec2(1.0f, 1.0f), 1.0f).commands.size() == 1u;
        char message[96];
        snprintf(message, sizeof(message), "addText at (%g, %g)", cases[i].position.x, cases[i].position.y);
        check(emitted == cases[i].emitted, message);
    }
    ig::DrawList empty;
    empty.addText("text", ig::Vec2(0.0f, 0.0f), ig::FontId(), 16.0f, ig::Color(), ig::Rect());
    check(empty.data(ig::Vec2(1.0f, 1.0f), 1.0f).commands.size() == 0u, "addText with an empty clip");
}

// Every geometry command of a frame must be able to touch its clip.
size_t offClipTriangles(const ig::DrawData &data)
{
    size_t count = 0u;
    for (size_t c = 0; c < data.commands.size(); ++c)
    {
        if (data.commands[c].type != ig::DrawCommandType::Geometry)
            continue;
        const ig::GeometryCommand &g = data.commands[c].payload.geometry;
        for (uint32_t t = 0; t + 2u < g.indexCount; t += 3u)
        {
            Box box = {1e30f, 1e30f, -1e30f, -1e30f};
            for (uint32_t k = 0; k < 3u; ++k)
            {
                const ig::Vec2 &p = data.vertices[data.indices[g.firstIndex + t + k] + g.vertexOffset].position;
                box.minX = p.x < box.minX ? p.x : box.minX;
                box.minY = p.y < box.minY ? p.y : box.minY;
                box.maxX = p.x > box.maxX ? p.x : box.maxX;
                box.maxY = p.y > box.maxY ? p.y : box.maxY;
            }
            // Same 2px margin the DrawList keeps for pixel rounding.
            const ig::Rect grown(g.clip.x - 2.0f, g.clip.y - 2.0f, g.clip.width + 4.0f, g.clip.height + 4.0f);
            if (!touchesScissor(box, grown))
                ++count;
        }
    }
    return count;
}

void test_scrolled_child_emits_only_visible_rows()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    auto build = [](ig::Context &c)
    {
        c.beginMainWindow("main");
        if (c.beginChild("child", 150.0f, true, 300.0f))
        {
            for (int i = 0; i < 300; ++i)
            {
                char label[16];
                snprintf(label, sizeof(label), "row %d", i);
                c.button(label);
            }
            c.endChild();
        }
        c.endWindow();
    };
    harness.frame(build);
    const ig::DrawData &data = harness.frame(build);
    check(offClipTriangles(data) == 0u, "child: geometry outside its clip");
    // 150px shows ~5 of the 28px rows; before culling this frame had 1232
    // vertices and 300 text commands.
    size_t text = 0u;
    for (size_t i = 0; i < data.commands.size(); ++i)
        if (data.commands[i].type == ig::DrawCommandType::Text)
            ++text;
    check(text <= 8u, "child: text commands for rows scrolled out of view");
    check(data.vertices.size() < 120u, "child: vertex count does not scale with hidden rows");
}

void test_off_screen_window_emits_nothing()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    auto build = [](ig::Context &c)
    {
        if (c.beginWindow("away", ig::Rect(2000.0f, 100.0f, 400.0f, 400.0f)))
        {
            for (int i = 0; i < 10; ++i)
                c.button("button");
            c.endWindow();
        }
    };
    harness.frame(build);
    const ig::DrawData &data = harness.frame(build);
    check(data.vertices.size() == 0u && data.commands.size() == 0u,
          "window entirely off screen still emits draw data");
}

void test_partly_visible_window_keeps_its_visible_part()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    bool clicked = false;
    auto build = [&clicked](ig::Context &c)
    {
        if (c.beginWindow("edge", ig::Rect(1200.0f, 100.0f, 400.0f, 400.0f)))
        {
            if (c.button("button", ig::Rect(0.0f, 0.0f, 300.0f, 40.0f)))
                clicked = true;
            c.endWindow();
        }
    };
    harness.frame(build);
    const ig::DrawData &data = harness.frame(build);
    check(offClipTriangles(data) == 0u, "edge window: geometry outside its clip");
    // The button's left end (x 1208..1280) is on screen and still drawn.
    check(harness.hasVertexAt(1208.0f, 132.0f), "edge window: visible part of the button missing");
    harness.click(1240.0f, 150.0f, build);
    check(clicked, "edge window: visible part of the button not clickable");
}

} // namespace

int main()
{
    test_culling_never_drops_visible_geometry();
    test_text_command_is_culled_only_when_certainly_invisible();
    test_scrolled_child_emits_only_visible_rows();
    test_off_screen_window_emits_nothing();
    test_partly_visible_window_keeps_its_visible_part();
    if (gFailures != 0)
    {
        printf("test_culling: %d failure(s)\n", gFailures);
        return 1;
    }
    printf("test_culling: all tests passed\n");
    return 0;
}
