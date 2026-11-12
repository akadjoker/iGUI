// The retained DrawList's vertex transform: pushTransform / popTransform map every vertex the
// primitives add, composition, clips pushed under a transform, text quads, clear().

#include <igui/widgets/Retained.hpp>

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

bool near(float a, float b)
{
    return fabsf(a - b) <= 1e-4f;
}

const Color Red(255, 0, 0, 255);

bool vertexAt(const DrawList& list, size_t i, float x, float y)
{
    return i < list.vertices().size() && near(list.vertices()[i].x, x) &&
           near(list.vertices()[i].y, y);
}

void testIdentity()
{
    DrawList list;
    const float identity[6] = { 1, 0, 0, 1, 0, 0 };
    list.pushTransform(identity);
    list.addRect({ 10, 20, 30, 40 }, Red);
    list.popTransform();
    check(list.vertices().size() == 4, "rect has four vertices");
    check(vertexAt(list, 0, 10, 20) && vertexAt(list, 2, 40, 60), "identity changes nothing");
    check(!list.hasTransform(), "popped");
}

void testTranslateScale()
{
    DrawList list;
    const float move[6] = { 1, 0, 0, 1, 5, 7 };
    list.pushTransform(move);
    check(list.hasTransform() && near(list.currentTransform()[4], 5), "in force");
    list.addRect({ 10, 20, 30, 40 }, Red);
    list.popTransform();
    check(vertexAt(list, 0, 15, 27) && vertexAt(list, 2, 45, 67), "translation moves the rect");
    check(near(list.currentTransform()[0], 1) && near(list.currentTransform()[4], 0),
            "identity again after pop");

    DrawList scaled;
    const float scale[6] = { 2, 0, 0, 3, 0, 0 };
    scaled.pushTransform(scale);
    scaled.addRect({ 10, 20, 30, 40 }, Red);
    check(vertexAt(scaled, 0, 20, 60) && vertexAt(scaled, 2, 80, 180), "scale about the origin");
}

void testRotate()
{
    DrawList list;
    const float rot[6] = { 0, 1, -1, 0, 0, 0 }; // rotate(90deg)
    list.pushTransform(rot);
    list.addRect({ 10, 20, 30, 40 }, Red);
    // (x, y) -> (-y, x)
    check(vertexAt(list, 0, -20, 10), "rotated corner 0");
    check(vertexAt(list, 1, -20, 40), "rotated corner 1");
    check(vertexAt(list, 2, -60, 40), "rotated corner 2");
    check(vertexAt(list, 3, -60, 10), "rotated corner 3");
}

void testNested()
{
    DrawList list;
    const float move[6] = { 1, 0, 0, 1, 20, 0 };
    const float scale[6] = { 2, 0, 0, 2, 0, 0 };
    list.pushTransform(move);
    list.pushTransform(scale); // translate(20) scale(2): x -> 2x + 20
    check(near(list.currentTransform()[0], 2) && near(list.currentTransform()[4], 20),
            "composition: outer * inner");
    list.addRect({ 1, 1, 1, 1 }, Red);
    check(vertexAt(list, 0, 22, 2), "nested mapping");
    list.popTransform();
    check(near(list.currentTransform()[0], 1) && near(list.currentTransform()[4], 20),
            "inner pop restores the outer");
    list.popTransform();
    check(!list.hasTransform(), "outer pop");

    DrawList other;
    other.pushTransform(scale);
    other.pushTransform(move); // scale(2) translate(20): x -> 2x + 40
    check(near(other.currentTransform()[4], 40), "scale then translate is scaled");
}

void testClip()
{
    DrawList list;
    const float scale[6] = { 2, 0, 0, 2, 0, 0 };
    list.pushTransform(scale);
    list.pushClip({ 5, 5, 10, 10 });
    list.popTransform();
    list.addRect({ 0, 0, 100, 100 }, Red);
    const DrawCmd& cmd = list.commands()[0];
    check(near(cmd.clip.x, 10) && near(cmd.clip.y, 10) && near(cmd.clip.w, 20) &&
                    near(cmd.clip.h, 20),
            "clip pushed under scale(2) scissors the mapped rect");
    list.popClip();

    DrawList before;
    before.pushClip({ 5, 5, 10, 10 });
    before.pushTransform(scale);
    before.addRect({ 0, 0, 100, 100 }, Red);
    check(near(before.commands()[0].clip.x, 5) && near(before.commands()[0].clip.w, 10),
            "a clip pushed before the transform stays unmoved");

    DrawList rotated;
    const float rot[6] = { 0, 1, -1, 0, 0, 0 };
    const float radii[4] = { 4, 4, 4, 4 };
    rotated.pushTransform(rot);
    rotated.pushClipRoundedRect({ 10, 20, 30, 40 }, radii);
    rotated.addRect({ -100, -100, 200, 200 }, Red);
    const DrawCmd& r = rotated.commands()[0];
    check(near(r.clip.x, -60) && near(r.clip.y, 10) && near(r.clip.w, 40) && near(r.clip.h, 30),
            "rotation: bounding box of the mapped corners");
    check(r.roundClipCount == 0, "rotation drops the rounded part");

    DrawList round;
    const float s2[6] = { 2, 0, 0, 3, 0, 0 };
    round.pushTransform(s2);
    round.pushClipRoundedRect({ 10, 10, 30, 30 }, radii);
    round.addRect({ 0, 0, 500, 500 }, Red);
    const DrawCmd& q = round.commands()[0];
    check(q.roundClipCount == 1 && near(q.roundClips[0].radii[0], 8) &&
                    near(q.roundClips[0].rect.w, 60),
            "scale keeps the rounded part, radii by min(|a|, |d|)");
}

void testText()
{
    FontAtlas atlas;
    check(atlas.buildDefault(), "default atlas");
    TextureHandle texture;
    texture.value = 1;
    atlas.setTexture(texture);
    const Font& font = atlas.defaultFont();
    DrawList plain, moved;
    plain.addText(font, { 10, 10 }, Red, "Hi");
    const float move[6] = { 1, 0, 0, 1, 100, 50 };
    moved.pushTransform(move);
    moved.addText(font, { 10, 10 }, Red, "Hi");
    check(plain.vertices().size() == moved.vertices().size() && plain.vertices().size() > 0,
            "same glyph quads");
    bool all = true;
    for (size_t i = 0; i < plain.vertices().size() && i < moved.vertices().size(); ++i)
        all = all && near(moved.vertices()[i].x, plain.vertices()[i].x + 100) &&
              near(moved.vertices()[i].y, plain.vertices()[i].y + 50) &&
              near(moved.vertices()[i].u, plain.vertices()[i].u);
    check(all, "text under a translation moves the glyph quads, UVs untouched");
}

void testTextSnapping()
{
    FontAtlas atlas;
    check(atlas.buildDefault(), "default atlas (snapping)");
    TextureHandle texture;
    texture.value = 1;
    atlas.setTexture(texture);
    const Font& font = atlas.defaultFont();
    DrawList snapped, loose;
    check(snapped.textSnapping(), "snapping is on by default");
    loose.setTextSnapping(false);
    snapped.addText(font, { 10.3f, 10 }, Red, "H");
    loose.addText(font, { 10.3f, 10 }, Red, "H");
    const float ps = font.pixelScale();
    const float sx = snapped.vertices()[0].x * ps, lx = loose.vertices()[0].x;
    check(near(sx, floorf(sx + 0.5f)), "snapping on: the glyph quad lands on a whole pixel");
    check(near(lx - loose.vertices()[0].x, 0) && fabsf(lx - (10.3f + font.findGlyph('H')->offset.x)) < 1e-3f,
            "snapping off: the quad stays at pen + offset (fractional)");
    loose.clear();
    check(!loose.textSnapping(), "clear() leaves the snapping flag alone");
}

void testPolygonsAndClear()
{
    DrawList list;
    const float move[6] = { 1, 0, 0, 1, 100, 0 };
    list.pushTransform(move);
    list.addCircleFilled({ 0, 0 }, 10, Red);
    list.addLine({ 0, 0 }, { 10, 0 }, Red, 2.0f);
    bool all = !list.vertices().empty();
    for (size_t i = 0; i < list.vertices().size(); ++i)
        all = all && list.vertices()[i].x > 85.0f;
    check(all, "filled paths and strokes are mapped (fringe included)");
    list.clear();
    check(!list.hasTransform() && near(list.currentTransform()[4], 0), "clear() resets");
    list.addRect({ 0, 0, 1, 1 }, Red);
    check(vertexAt(list, 0, 0, 0), "after clear nothing is mapped");
    list.popTransform(); // harmless
}

} // namespace

int main()
{
    testIdentity();
    testTranslateScale();
    testRotate();
    testNested();
    testClip();
    testText();
    testTextSnapping();
    testPolygonsAndClear();
    if (gFailures == 0) printf("transform tests ok\n");
    return gFailures == 0 ? 0 : 1;
}
