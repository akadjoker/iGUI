// Round 10, part 1 (docs/DRAW-CALLBACK.md): native draw callbacks, without a GL context.
//   - NativeDrawAvailable() is false with no renderer and with the software rasteriser; a NativeView then draws its fallback and
//     never calls the callback; a callback added by hand to a draw list is skipped by the software rasteriser (same pixels as
//     without it), never a crash;
//   - the command is where it was added in the draw order, carries the clip in force, and nothing merges across it: the shapes
//     before and after it are two commands, and without callbacks the draw list is what it was (the same commands);
//   - a callback whose clip is empty or whose rectangle is outside the clip leaves no command; a transform maps its rectangle;
//   - idle: a NativeView in a settled window paints 0 frames in 100; requestRepaint() paints exactly one and does not mark
//     anything dirty by itself.
// The callback running on a GL renderer, its state and its pixels: tests/test_zen_native_callback_gl.cpp.
// No standard library: ct and C only.

#include "RasterScreen.hpp"

#include "BasicWidgets.hpp"
#include "ViewWidgets.hpp"

using namespace rh;

namespace
{

int gCalls = 0;
void countingCallback(const NativeDrawInfo&)
{
    ++gCalls;
}

// one pass of the loop an application runs; true when it was painted
bool pass(Harness& h)
{
    NewFrame();
    h.app.update(GetIO());
    if (!h.app.needsFrame())
    {
        Render();
        return false;
    }
    h.app.paint(*GetDrawData(), &h.atlas().defaultFont(), nullptr);
    if (h.atlas().dynamicDirty()) h.atlas().clearDynamicDirty();
    Render();
    return true;
}

// children of a widget of their own: the stage gives its own children the whole window at every full layout
Widget* holderOf(Harness& h, float w, float hh)
{
    Widget* holder = h.root()->createChild<Widget>();
    holder->setRect({ 0, 0, w, hh });
    return holder;
}

int passes(Harness& h, int n)
{
    int painted = 0;
    for (int i = 0; i < n; ++i)
        if (pass(h)) ++painted;
    return painted;
}

void testAvailability()
{
    printf("-- availability and fallback\n");
    Screen screen(320.0f, 200.0f);
    Harness& h = screen.h;
    check(!NativeDrawAvailable(), "no renderer: native draw callbacks are not available");
    check(GetIO().nativeBackend == 0 && GetIO().nativeSamples == 0, "a fresh IO says no backend and no samples");

    auto* view = holderOf(h, 320.0f, 200.0f)->createChild<NativeView>();
    view->setRect({ 10, 10, 100, 80 });
    gCalls = 0;
    int fallbackCalls = 0;
    Rect fallbackRect = { 0, 0, 0, 0 };
    view->setNativeDraw(countingCallback);
    view->setFallback(
            [&](PaintContext& ctx, const Rect& b)
            {
                ++fallbackCalls;
                fallbackRect = b;
                ctx.fill.SetColor(Color(200, 40, 40, 255));
                ctx.fillRect(b.x, b.y, b.w, b.h);
            });
    check(!view->isNative(), "the view asks the renderer before painting: not native here");
    h.relayout();
    h.frames(3);
    check(gCalls == 0, "the callback is never called on a renderer that cannot run it");
    check(fallbackCalls >= 1, "the fallback paints instead");
    check(fallbackRect.x == 10.0f && fallbackRect.w == 100.0f, "... in the widget's rectangle");
    check(GetDrawList().specials().empty(), "and no callback command is in the draw list");

    // the same renderer now says it can: the view turns native by itself, with no change on its side
    GetIO().nativeBackend = static_cast<int>(NativeBackend::OpenGL33Core);
    check(NativeDrawAvailable() && view->isNative(), "a renderer that can run callbacks makes the same view native");
    const int before = fallbackCalls;
    h.relayout();
    h.frames(2);
    check(fallbackCalls == before, "... and the fallback no longer paints");
    check(GetDrawList().specials().size() == 1, "one callback command in the draw list");
    if (GetDrawList().specials().size() == 1)
    {
        const DrawList::Special& sp = GetDrawList().specials()[0];
        check(sp.fn == countingCallback && sp.restoreState, "it carries the function and the default restore flag");
        check(sp.rect.x == 10.0f && sp.rect.y == 10.0f && sp.rect.w == 100.0f && sp.rect.h == 80.0f, "and the widget's rectangle");
    }
    view->setNativeDraw(countingCallback, nullptr, false);
    h.frames(2);
    check(GetDrawList().specials().size() == 1 && !GetDrawList().specials()[0].restoreState, "restoreState false is carried");
    GetIO().nativeBackend = 0;
    h.frames(2);
    check(GetDrawList().specials().empty(), "back to a renderer without callbacks: the command is gone");
    check(gCalls == 0, "no renderer in this test ran a callback");
}

// a rectangle of one colour drawn through the draw list with the white texel of texture 1
void rect(DrawList& dl, float x, float y, float w, float h, Color c)
{
    dl.addRect({ x, y, w, h }, c);
}

void testOrderAndMerging()
{
    printf("-- order in the draw list and merging\n");
    DrawList dl;
    dl.setWhitePixel(handle(1), { 0.5f, 0.5f });
    // no callback: three rectangles of one clip are one command
    rect(dl, 0, 0, 10, 10, Color(255, 0, 0, 255));
    rect(dl, 20, 0, 10, 10, Color(0, 255, 0, 255));
    rect(dl, 40, 0, 10, 10, Color(0, 0, 255, 255));
    check(dl.commands().size() == 1, "without a callback the shapes are one command (nothing changed for windows that use none)");

    dl.clear();
    dl.setWhitePixel(handle(1), { 0.5f, 0.5f });
    rect(dl, 0, 0, 10, 10, Color(255, 0, 0, 255));
    dl.addNativeCallback(countingCallback, nullptr, { 0, 0, 100, 100 });
    rect(dl, 20, 0, 10, 10, Color(0, 255, 0, 255));
    rect(dl, 40, 0, 10, 10, Color(0, 0, 255, 255));
    check(dl.commands().size() == 3, "a callback splits the shapes before and after it: three commands, none merged across it");
    if (dl.commands().size() == 3)
    {
        check(!dl.commands()[0].isSpecial() && dl.commands()[1].isSpecial() && !dl.commands()[2].isSpecial(), "in the order they were added");
        check(dl.commands()[1].indexCount == 0 && dl.commands()[1].roundClipCount < 0, "the callback command has no triangles");
        check(dl.commands()[0].indexOffset + dl.commands()[0].indexCount == dl.commands()[2].indexOffset, "the shapes after it start where the ones before ended");
    }
    // the same, with the commands merging across clips switched on and a clip that does not matter
    dl.clear();
    dl.setWhitePixel(handle(1), { 0.5f, 0.5f });
    dl.pushClip({ 0, 0, 200, 200 });
    rect(dl, 0, 0, 10, 10, Color(255, 0, 0, 255));
    dl.pushClip({ 0, 0, 60, 60 });
    dl.addNativeCallback(countingCallback, nullptr, { 0, 0, 100, 100 });
    rect(dl, 20, 0, 10, 10, Color(0, 255, 0, 255));
    dl.popClip();
    rect(dl, 40, 0, 10, 10, Color(0, 0, 255, 255));
    dl.popClip();
    // (the two shapes after the callback do merge with each other, as shapes under such clips always did: 3 commands)
    check(dl.commands().size() == 3 && !dl.commands()[0].isSpecial() && dl.commands()[1].isSpecial() && !dl.commands()[2].isSpecial(),
            "under clips that merge shapes, the callback still keeps the ones before and after it apart (3 commands)");
    if (dl.commands().size() >= 2 && dl.commands()[1].isSpecial())
    {
        const Rect& c = dl.commands()[1].clip;
        check(c.x == 0.0f && c.y == 0.0f && c.w == 60.0f && c.h == 60.0f, "the callback's command carries the clip in force (the intersection)");
    }
    // two callbacks in a row
    dl.clear();
    dl.addNativeCallback(countingCallback, nullptr, { 0, 0, 10, 10 });
    dl.addNativeCallback(countingCallback, nullptr, { 0, 0, 10, 10 });
    check(dl.commands().size() == 2 && dl.specials().size() == 2, "two callbacks in a row are two commands");
    // nothing to draw
    dl.clear();
    dl.addNativeCallback(nullptr, nullptr, { 0, 0, 10, 10 });
    check(dl.commands().empty(), "no function: no command");
    dl.pushClip({ 0, 0, 0, 20 });
    dl.addNativeCallback(countingCallback, nullptr, { 0, 0, 10, 10 });
    dl.popClip();
    check(dl.commands().empty(), "an empty clip: no command");
    dl.pushClip({ 50, 50, 20, 20 });
    dl.addNativeCallback(countingCallback, nullptr, { 0, 0, 10, 10 });
    dl.popClip();
    check(dl.commands().empty(), "a rectangle outside the clip: no command");
    dl.addNativeCallback(countingCallback, nullptr, { 0, 0, 0, 10 });
    check(dl.commands().empty(), "a rectangle with no area: no command");
    // a transform maps the rectangle
    dl.clear();
    const float move[6] = { 2, 0, 0, 2, 5, 7 };
    dl.pushTransform(move);
    dl.addNativeCallback(countingCallback, nullptr, { 1, 1, 10, 10 });
    dl.popTransform();
    check(dl.specials().size() == 1 && dl.specials()[0].rect.x == 7.0f && dl.specials()[0].rect.y == 9.0f && dl.specials()[0].rect.w == 20.0f &&
                    dl.specials()[0].rect.h == 20.0f,
            "the rectangle is mapped by the transform in force");
    // clear() forgets them
    dl.clear();
    check(dl.specials().empty() && dl.commands().empty(), "clear() forgets the callbacks");
}

void testSoftwareSkips()
{
    printf("-- the software rasteriser skips a callback\n");
    Screen screen(200.0f, 120.0f);
    Harness& h = screen.h;
    Widget* holder = holderOf(h, 200.0f, 120.0f);
    auto* panel = holder->createChild<Panel>();
    panel->setRect({ 10, 10, 120, 80 });
    panel->setBgColor(Color(30, 90, 160, 255));
    h.relayout();
    h.frames(2);
    const Shot without = screen.take();

    GetIO().nativeBackend = static_cast<int>(NativeBackend::OpenGL33Core); // (as if the renderer could: the views turn native)
    auto* view = holder->createChild<NativeView>();
    view->setRect({ 40, 20, 100, 60 });
    gCalls = 0;
    view->setNativeDraw(countingCallback);
    h.relayout();
    h.frames(2);
    check(GetDrawList().specials().size() == 1, "the draw list has the callback");
    const Shot with = screen.take(); // the rasteriser it is given does not run it
    check(differing(without, with, 0, 0, 200, 120) == 0, "the software rasteriser draws the same pixels with and without it");
    check(gCalls == 0, "and does not call it");
    GetIO().nativeBackend = 0;
}

void testIdle()
{
    printf("-- idle\n");
    Harness h(320.0f, 200.0f);
    auto* view = holderOf(h, 320.0f, 200.0f)->createChild<NativeView>();
    view->setRect({ 10, 10, 100, 80 });
    view->setNativeDraw(countingCallback);
    view->setFallback([](PaintContext& ctx, const Rect& b) { ctx.fill.SetColor(Color(10, 120, 10, 255)); ctx.fillRect(b.x, b.y, b.w, b.h); });
    h.relayout();
    int quiet = 0;
    for (int i = 0; i < 50 && quiet < 3; ++i) quiet = pass(h) ? 0 : quiet + 1;
    check(passes(h, 100) == 0, "100 frames with nothing changed paint none (a window with a NativeView in it)");
    view->requestRepaint();
    check(passes(h, 6) == 1, "requestRepaint() paints exactly one frame");
    check(passes(h, 50) == 0, "... and then none: painting the view marked nothing dirty");
    view->setRect({ 10, 10, 120, 80 });
    check(passes(h, 6) == 1, "a change of the view paints one frame");
}

} // namespace

int main()
{
    testAvailability();
    testOrderAndMerging();
    testSoftwareSkips();
    testIdle();
    shutdown();
    if (failures())
    {
        printf("%d FAILED\n", failures());
        return 1;
    }
    printf("test_native_callback: ok\n");
    return 0;
}
