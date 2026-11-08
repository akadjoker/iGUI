// Rounded clips of the retained DrawList: what the commands carry, nesting, batching, the CPU
// fallback (ClipToRoundClips), the per-vertex-colour polygon, and the software rasteriser's
// coverage at a rounded corner.

#include <igui/widgets/Retained.hpp>
#include <igui_zen/SoftRaster.hpp>

#include <math.h>
#include <stdio.h>

using namespace ig::retained;

namespace
{

int gFailures = 0;

void check(bool condition, const char* what)
{
    if (!condition)
    {
        ++gFailures;
        printf("FAIL %s\n", what);
    }
}

bool near(float a, float b, float tolerance)
{
    return fabsf(a - b) <= tolerance;
}

const Color Red(255, 0, 0, 255);
const float NoRadii[4] = { 0, 0, 0, 0 };

void testCommandsCarryClip()
{
    DrawList list;
    const float radii[4] = { 8, 6, 4, 2 };
    list.pushClipRoundedRect({ 10, 20, 100, 60 }, radii);
    list.addRect({ 0, 0, 200, 200 }, Red);
    list.popClip();
    list.addRect({ 0, 0, 200, 200 }, Red);

    check(list.commands().size() == 2, "rounded and plain command are separate");
    const DrawCmd& a = list.commands()[0];
    check(a.roundClipCount == 1, "command carries one rounded clip");
    check(a.clip.x == 10 && a.clip.y == 20 && a.clip.w == 100 && a.clip.h == 60,
            "scissor is the rounded clip's rectangle");
    check(a.roundClips[0].rect.w == 100 && a.roundClips[0].rect.h == 60, "round rect kept");
    check(a.roundClips[0].radii[0] == 8 && a.roundClips[0].radii[1] == 6 &&
                    a.roundClips[0].radii[2] == 4 && a.roundClips[0].radii[3] == 2,
            "radii in TL TR BR BL order");
    check(list.commands()[1].roundClipCount == 0, "popClip leaves later commands unrounded");
    check(list.pushClipCount() == 1, "a rounded push counts as a clip change");
}

void testNoRadiusIsPlainClip()
{
    DrawList list;
    list.pushClipRoundedRect({ 0, 0, 50, 50 }, NoRadii);
    list.addRect({ 0, 0, 100, 100 }, Red);
    list.popClip();
    check(list.commands().size() == 1 && list.commands()[0].roundClipCount == 0,
            "zero radii: plain scissor");
    check(list.commands()[0].clip.w == 50, "zero radii: scissor still applies");

    const float negative[4] = { -3, -1, 0, -9 };
    list.pushClipRoundedRect({ 0, 0, 50, 50 }, negative);
    list.addRect({ 0, 0, 100, 100 }, Red);
    list.popClip();
    check(list.commands().back().roundClipCount == 0, "negative radii count as zero");
}

void testRadiiScaleToFit()
{
    DrawList list;
    const float radii[4] = { 100, 100, 100, 100 };
    list.pushClipRoundedRect({ 0, 0, 50, 30 }, radii);
    list.addRect({ 0, 0, 100, 100 }, Red);
    list.popClip();
    const DrawCmd& c = list.commands()[0];
    check(near(c.roundClips[0].radii[0], 15, 0.001f), "radii shrink to half the short side");
    const float uneven[4] = { 40, 10, 0, 0 };
    list.pushClipRoundedRect({ 0, 0, 100, 100 }, uneven);
    list.addRect({ 0, 0, 100, 100 }, Red);
    list.popClip();
    const DrawCmd& d = list.commands().back();
    check(near(d.roundClips[0].radii[0], 40, 0.001f), "radii that fit are untouched");
    const float tight[4] = { 60, 60, 0, 0 };
    list.pushClipRoundedRect({ 0, 0, 100, 100 }, tight);
    list.addRect({ 0, 0, 100, 100 }, Red);
    list.popClip();
    const DrawCmd& e = list.commands().back();
    check(near(e.roundClips[0].radii[0], 50, 0.001f) && near(e.roundClips[0].radii[1], 50, 0.001f),
            "two radii overflowing a side scale together");
}

void testNesting()
{
    DrawList list;
    const float big[4] = { 20, 20, 20, 20 };
    const float small[4] = { 5, 5, 5, 5 };
    list.pushClipRoundedRect({ 0, 0, 100, 100 }, big);
    list.pushClipRoundedRect({ 50, 50, 100, 100 }, small);
    list.addRect({ 0, 0, 500, 500 }, Red);
    list.popClip();
    list.addRect({ 0, 0, 500, 500 }, Red);
    list.pushClip({ 0, 0, 30, 30 });
    list.addRect({ 0, 0, 500, 500 }, Red);
    list.popClip();
    list.popClip();

    check(list.commands().size() == 3, "three commands");
    const DrawCmd& inner = list.commands()[0];
    check(inner.roundClipCount == 2, "nested rounded clips both kept");
    check(inner.roundClips[0].radii[0] == 20 && inner.roundClips[1].radii[0] == 5,
            "outermost first");
    check(inner.clip.x == 50 && inner.clip.y == 50 && inner.clip.w == 50 && inner.clip.h == 50,
            "scissor is the intersection");
    const DrawCmd& outer = list.commands()[1];
    check(outer.roundClipCount == 1 && outer.roundClips[0].radii[0] == 20 && outer.clip.w == 100,
            "after popClip the outer clip is back");
    const DrawCmd& plain = list.commands()[2];
    check(plain.roundClipCount == 1 && plain.clip.w == 30,
            "a plain clip inside a rounded one keeps the rounded clip");

    // The same shape pushed twice is one clip.
    DrawList twice;
    twice.pushClipRoundedRect({ 0, 0, 100, 100 }, big);
    twice.pushClipRoundedRect({ 0, 0, 100, 100 }, big);
    twice.addRect({ 0, 0, 500, 500 }, Red);
    check(twice.commands()[0].roundClipCount == 1, "identical nested clip is not repeated");

    // Past the limit the outermost fall away, the innermost stay.
    DrawList deep;
    for (int i = 0; i < DrawCmd::MaxRoundClips + 2; ++i)
    {
        const float r[4] = { 1.0f + i, 1.0f + i, 1.0f + i, 1.0f + i };
        deep.pushClipRoundedRect({ 0, 0, 1000.0f - i, 1000.0f - i }, r);
    }
    deep.addRect({ 0, 0, 2000, 2000 }, Red);
    const DrawCmd& d = deep.commands()[0];
    check(d.roundClipCount == DrawCmd::MaxRoundClips, "limit respected");
    check(d.roundClips[DrawCmd::MaxRoundClips - 1].radii[0] == DrawCmd::MaxRoundClips + 2,
            "innermost kept");
    check(d.roundClips[0].radii[0] == 3, "oldest dropped");
}

void testBatching()
{
    const float r[4] = { 6, 6, 6, 6 };
    const float r2[4] = { 7, 7, 7, 7 };
    DrawList list;
    list.pushClipRoundedRect({ 0, 0, 100, 100 }, r);
    list.addRect({ 0, 0, 10, 10 }, Red);
    list.addRect({ 20, 0, 10, 10 }, Red);
    list.popClip();
    check(list.commands().size() == 1, "same clip, same texture: merged");

    list.pushClipRoundedRect({ 0, 0, 100, 100 }, r2);
    list.addRect({ 40, 0, 10, 10 }, Red);
    list.popClip();
    check(list.commands().size() == 2, "different radii: not merged");

    list.pushClipRoundedRect({ 0, 0, 100, 100 }, r2);
    list.addRect({ 60, 0, 10, 10 }, Red);
    list.popClip();
    check(list.commands().size() == 2, "a re-pushed identical clip merges with the previous");

    list.pushClipRoundedRect({ 0, 0, 100, 100 }, r2);
    list.pushClipRoundedRect({ 10, 10, 50, 50 }, r);
    list.addRect({ 60, 0, 10, 10 }, Red);
    list.popClip();
    list.popClip();
    check(list.commands().size() == 3, "an extra nested clip: not merged");

    DrawList plainThenRound;
    plainThenRound.pushClip({ 0, 0, 100, 100 });
    plainThenRound.addRect({ 0, 0, 10, 10 }, Red);
    plainThenRound.pushClipRoundedRect({ 0, 0, 100, 100 }, r);
    plainThenRound.addRect({ 20, 0, 10, 10 }, Red);
    check(plainThenRound.commands().size() == 2, "same scissor, rounded or not: not merged");
}

float polygonArea(const ClippedGeometry& g)
{
    float area = 0;
    for (size_t i = 0; i + 2 < g.indices.size(); i += 3)
    {
        const DrawVertex &a = g.vertices[g.indices[i]], &b = g.vertices[g.indices[i + 1]],
                         &c = g.vertices[g.indices[i + 2]];
        area += fabsf((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)) * 0.5f;
    }
    return area;
}

// Distance inside (negative) or outside (positive) a uniformly rounded rectangle.
float roundedDistance(float x, float y, const Rect& r, float radius)
{
    const float qx = fabsf(x - (r.x + r.w * 0.5f)) - r.w * 0.5f + radius;
    const float qy = fabsf(y - (r.y + r.h * 0.5f)) - r.h * 0.5f + radius;
    const float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    return sqrtf(ox * ox + oy * oy) + (qx > qy ? (qx < 0 ? qx : 0) : (qy < 0 ? qy : 0)) - radius;
}

void testCpuFallback()
{
    // An image quad far larger than the clip, so UV = position / 400 everywhere (linear, hence
    // exact after clipping).
    DrawList list;
    const float radii[4] = { 20, 20, 20, 20 };
    const Rect rect = { 50, 60, 100, 80 };
    list.pushClipRoundedRect(rect, radii);
    TextureHandle texture;
    texture.value = 7;
    list.addImage(texture, { 0, 0, 400, 400 }, { 0, 0, 1, 1 });
    const DrawCmd& command = list.commands()[0];
    check(command.roundClipCount == 1, "image command carries the clip");

    ClippedGeometry out;
    ClipToRoundClips(list.vertices().data(), list.vertices().size(), list.indices().data(),
            list.indices().size(), command, out);
    check(!out.indices.empty() && out.indices.size() % 3 == 0, "clipped geometry produced");

    const float expected = rect.w * rect.h - (4.0f - 3.14159265f) * 20.0f * 20.0f;
    const float area = polygonArea(out);
    check(near(area, expected, expected * 0.01f), "clipped area equals the rounded rect's");

    bool inside = true, uvOk = true;
    for (size_t i = 0; i < out.vertices.size(); ++i)
    {
        const DrawVertex& v = out.vertices[i];
        if (roundedDistance(v.x, v.y, rect, 20) > 0.15f) inside = false;
        // addImage maps (0,0)..(400,400) to uv (0,0)..(1,1).
        if (!near(v.u, v.x / 400, 0.0005f) || !near(v.v, v.y / 400, 0.0005f)) uvOk = false;
    }
    check(inside, "every output vertex is inside the rounded rect");
    check(uvOk, "UVs are interpolated at the cut points");

    // Colour: a gradient quad built by hand (red channel = x / 2) through the multi-colour fill.
    DrawList gradient;
    gradient.setAntiAliasing(false);
    gradient.pushClipRoundedRect(rect, radii);
    const Vec2 points[4] = { { 0, 0 }, { 400, 0 }, { 400, 400 }, { 0, 400 } };
    const Color colors[4] = { Color(0, 0, 0, 255), Color(200, 0, 0, 255), Color(200, 0, 0, 255),
        Color(0, 0, 0, 255) };
    gradient.addConvexPolyFilledMultiColor(points, colors, 4);
    ClippedGeometry gout;
    check(!gradient.commands().empty(), "gradient command exists");
    ClipToRoundClips(gradient.vertices().data(), gradient.vertices().size(),
            gradient.indices().data(), gradient.indices().size(), gradient.commands()[0], gout);
    bool colourOk = !gout.vertices.empty();
    for (size_t i = 0; i < gout.vertices.size(); ++i)
        if (!near(gout.vertices[i].color.r, gout.vertices[i].x / 2.0f, 1.0f)) colourOk = false;
    check(colourOk, "colour is interpolated at the cut points");

    // Triangles wholly outside the shape's bounds vanish, wholly inside are copied untouched.
    DrawList two;
    two.setAntiAliasing(false);
    two.pushClipRoundedRect(rect, radii);
    two.addRect({ 0, 0, 10, 10 }, Red);   // outside
    two.addRect({ 80, 90, 10, 10 }, Red); // inside, away from the corners
    ClippedGeometry tout;
    for (size_t c = 0; c < two.commands().size(); ++c)
        ClipToRoundClips(two.vertices().data(), two.vertices().size(), two.indices().data(),
                two.indices().size(), two.commands()[c], tout);
    check(tout.indices.size() == 6, "outside dropped, inside kept as is");
    check(near(polygonArea(tout), 100.0f, 0.01f), "the inside quad is not changed");

    // Two nested clips: the result lies in both.
    DrawList nested;
    nested.setAntiAliasing(false);
    nested.pushClipRoundedRect({ 0, 0, 100, 100 }, radii);
    const float other[4] = { 30, 30, 30, 30 };
    nested.pushClipRoundedRect({ 50, 50, 100, 100 }, other);
    nested.addRect({ -50, -50, 300, 300 }, Red);
    ClippedGeometry nout;
    ClipToRoundClips(nested.vertices().data(), nested.vertices().size(), nested.indices().data(),
            nested.indices().size(), nested.commands()[0], nout);
    bool both = !nout.vertices.empty();
    for (size_t i = 0; i < nout.vertices.size(); ++i)
    {
        const DrawVertex& v = nout.vertices[i];
        if (roundedDistance(v.x, v.y, { 0, 0, 100, 100 }, 20) > 0.15f ||
                roundedDistance(v.x, v.y, { 50, 50, 100, 100 }, 30) > 0.15f)
            both = false;
    }
    check(both, "nested clips: output inside both");
}

void testMultiColourPolygon()
{
    const Vec2 points[4] = { { 0, 0 }, { 10, 0 }, { 10, 10 }, { 0, 10 } };
    const Color colors[4] = { Color(255, 0, 0, 255), Color(0, 255, 0, 255), Color(0, 0, 255, 255),
        Color(255, 255, 0, 255) };

    DrawList sharp;
    sharp.setAntiAliasing(false);
    sharp.addConvexPolyFilledMultiColor(points, colors, 4);
    check(sharp.vertices().size() == 4 && sharp.indices().size() == 6,
            "plain: 4 vertices 2 triangles");
    bool same = sharp.vertices().size() == 4;
    for (size_t i = 0; same && i < 4; ++i)
        same = sharp.vertices()[i].color.r == colors[i].r &&
               sharp.vertices()[i].color.g == colors[i].g &&
               sharp.vertices()[i].color.b == colors[i].b && sharp.vertices()[i].x == points[i].x;
    check(same, "each vertex keeps its colour and position");

    DrawList smooth;
    smooth.addConvexPolyFilledMultiColor(points, colors, 4);
    check(smooth.vertices().size() == 8, "anti-aliased: inner and fringe vertex per point");
    bool fringe = smooth.vertices().size() == 8;
    for (size_t i = 0; fringe && i < 4; ++i)
    {
        const DrawVertex &inner = smooth.vertices()[i * 2], &outer = smooth.vertices()[i * 2 + 1];
        fringe = inner.color.r == colors[i].r && inner.color.a == 255 && outer.color.a == 0 &&
                 outer.color.r == colors[i].r && outer.color.g == colors[i].g &&
                 outer.color.b == colors[i].b;
    }
    check(fringe, "the fringe fades each vertex's own colour to transparent");

    DrawList degenerate;
    degenerate.addConvexPolyFilledMultiColor(points, colors, 2);
    degenerate.addConvexPolyFilledMultiColor(nullptr, colors, 4);
    check(degenerate.commands().empty(), "fewer than 3 points or null: nothing");
}

uint32_t pixelAt(const ct::Vector<uint32_t>& image, int width, int x, int y)
{
    return image[static_cast<size_t>(y) * width + x];
}

int channel(uint32_t pixel, int shift)
{
    return static_cast<int>((pixel >> shift) & 0xff);
}

// A 0xFFFFFFFF background, a red fill clipped to the rounded rectangle (4,4,32,32) radius 12.
void renderCorner(int samples, ct::Vector<uint32_t>& image, Camera2D camera = Camera2D{},
        float displayWidth = 40)
{
    DrawList list;
    const float radii[4] = { 12, 12, 12, 12 };
    list.pushClipRoundedRect({ 4, 4, 32, 32 }, radii);
    list.addRect({ 0, 0, 100, 100 }, Red);
    list.popClip();

    ig::retained::DrawData data;
    data.passes.push_back({ &list, camera });
    data.displayWidth = displayWidth;
    data.displayHeight = displayWidth;
    const int size = static_cast<int>(displayWidth);
    image.resize(static_cast<size_t>(size) * size);
    ig::zen::SoftRaster raster;
    raster.setSamples(samples);
    raster.render(data, image.data(), size, size, size, 0xffffffffu);
}

void testSoftRasterCoverage()
{
    for (int samples = 1; samples <= 4; samples += (samples == 1 ? 1 : 2))
    {
        ct::Vector<uint32_t> image;
        renderCorner(samples, image);
        char name[96];
        snprintf(name, sizeof name, "soft x%d: centre opaque red", samples);
        const uint32_t centre = pixelAt(image, 40, 20, 20);
        check(channel(centre, 16) == 255 && channel(centre, 8) == 0, name);
        snprintf(name, sizeof name, "soft x%d: outside the corner is the background", samples);
        const uint32_t cornerOut = pixelAt(image, 40, 4, 4);
        check(channel(cornerOut, 8) == 255, name);
        check(channel(pixelAt(image, 40, 5, 5), 8) == 255, name);
        snprintf(name, sizeof name, "soft x%d: inside the curve is red", samples);
        check(channel(pixelAt(image, 40, 9, 9), 8) == 0, name);
        snprintf(name, sizeof name, "soft x%d: straight edge is crisp", samples);
        check(channel(pixelAt(image, 40, 4, 20), 8) == 0 &&
                        channel(pixelAt(image, 40, 3, 20), 8) == 255,
                name);
        check(channel(pixelAt(image, 40, 35, 20), 8) == 0 &&
                        channel(pixelAt(image, 40, 36, 20), 8) == 255,
                name);
        // Pixel (7,7) is the one the arc passes through: partial coverage.
        const int green = channel(pixelAt(image, 40, 7, 7), 8);
        snprintf(name, sizeof name, "soft x%d: the edge pixel is partial (green %d)", samples,
                green);
        check(samples == 1 ? (green > 20 && green < 235) : (green > 40 && green < 215), name);
        // Corners are symmetric.
        check(pixelAt(image, 40, 7, 7) == pixelAt(image, 40, 32, 7) &&
                        pixelAt(image, 40, 7, 7) == pixelAt(image, 40, 7, 32) &&
                        pixelAt(image, 40, 7, 7) == pixelAt(image, 40, 32, 32),
                "soft: the four corners match");
    }

    // A camera that doubles the scale: the shape follows (corner pixel (8,8) is now outside, the
    // pixel at the arc's apex region is filled).
    Camera2D zoom;
    zoom.scale = 2.0f;
    zoom.pivotX = 0.0f;
    zoom.pivotY = 0.0f;
    ct::Vector<uint32_t> image;
    renderCorner(2, image, zoom, 80);
    check(channel(pixelAt(image, 80, 8, 8), 8) == 255,
            "camera scale: shape scales (corner outside)");
    check(channel(pixelAt(image, 80, 8, 40), 8) == 0 &&
                    channel(pixelAt(image, 80, 7, 40), 8) == 255,
            "camera scale: straight edge at 2x");
    check(channel(pixelAt(image, 80, 18, 18), 8) == 0, "camera scale: inside the 2x curve");
    check(channel(pixelAt(image, 80, 10, 10), 8) == 255, "camera scale: outside the 2x curve");

    // A translation moves it.
    Camera2D pan;
    pan.x = 10.0f;
    pan.pivotX = 0.0f;
    pan.pivotY = 0.0f;
    renderCorner(2, image, pan, 60);
    check(channel(pixelAt(image, 60, 14, 20), 8) == 0 &&
                    channel(pixelAt(image, 60, 13, 20), 8) == 255,
            "camera pan: shape moves");
}

} // namespace

int main()
{
    testCommandsCarryClip();
    testNoRadiusIsPlainClip();
    testRadiiScaleToFit();
    testNesting();
    testBatching();
    testCpuFallback();
    testMultiColourPolygon();
    testSoftRasterCoverage();
    if (gFailures)
    {
        printf("%d failure(s)\n", gFailures);
        return 1;
    }
    printf("round clip tests passed\n");
    return 0;
}
