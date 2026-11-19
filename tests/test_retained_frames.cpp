// Round 9, part 2 (docs/ROUND9.md): the frames that are not painted.
//   - nothing changed for 100 frames: no frame is painted;
//   - each kind of change makes exactly one frame painted (a property, a hover, a click, a key, a layout, a theme, a resize, a request);
//   - what runs on time keeps on time: an animation paints every frame until it ends, a caret flips twice a second, a tooltip comes on
//     at its delay, a request made for later is made good when due;
//   - skipping frames never leaves a stale picture: the same scripts, run painting every frame and skipping, end each step with the
//     same picture.
// A frame here is one pass of the loop an application runs (what ig::zen::App::frameWindow does): NewFrame, update, and only when
// WidgetApp::needsFrame says so, paint and Render. No standard library: ct and C only.

#include "Round9Window.hpp"

#include <igui/widgets/Animation.hpp>

using namespace rh;

namespace
{

int gPainted = 0;       // frames painted since the counter was cleared
uint64_t gLastSum = 0;  // the checksum of the last frame painted

// One pass of the loop; true when the frame was painted.
bool pass(Harness& h, bool skipping)
{
    NewFrame();
    h.app.update(GetIO());
    if (skipping && !h.app.needsFrame())
    {
        Render();
        return false;
    }
    h.app.paint(*GetDrawData(), &h.atlas().defaultFont(), nullptr);
    if (h.atlas().dynamicDirty()) h.atlas().clearDynamicDirty();
    Render();
    ++gPainted;
    gLastSum = drawChecksum();
    return true;
}

// `n` passes; how many were painted.
int passes(Harness& h, int n, bool skipping = true)
{
    int painted = 0;
    for (int i = 0; i < n; ++i)
        if (pass(h, skipping)) ++painted;
    return painted;
}

// A window with a widget of most types in it, settled: passes until a pass is not painted twice in a row.
struct Settled
{
    Harness h;
    EveryWidget w;
    Label* label = nullptr;
    Button* button = nullptr;
    TextInput* input = nullptr;
    Widget* extra_ = nullptr;
    Settled(float width = 1280.0f, float height = 720.0f) : h(width, height)
    {
        Theme::instance() = Theme::dark();
        w = buildEveryWidget(h, false);
        // (children of a widget of their own: the stage gives its own children the whole window at every full layout)
        Widget* extra = h.root()->createChild<Widget>();
        extra->setRect({ 0, 0, width, height });
        label = extra->createChild<Label>("a label");
        label->setRect({ 20, 640, 200, 24 });
        button = extra->createChild<Button>("a button");
        button->setRect({ 240, 636, 120, 28 });
        input = extra->createChild<TextInput>("text");
        input->setRect({ 380, 636, 200, 26 });
        extra_ = extra;
        h.app.setFocused(nullptr); // (a focused text widget blinks: its own cases)
        h.relayout();
        int quiet = 0;
        for (int i = 0; i < 400 && quiet < 3; ++i) quiet = pass(h, true) ? 0 : quiet + 1;
    }
};

void testNothingChanged()
{
    printf("-- nothing changed: nothing painted\n");
    Settled s;
    check(!s.h.app.needsFrame(), "a settled window needs no frame");
    check(s.h.app.nextFrameInMs() == -1, "... and waits for nothing");
    const int painted = passes(s.h, 100);
    char what[100];
    snprintf(what, sizeof what, "100 frames with nothing dirty paint nothing (painted %d)", painted);
    check(painted == 0, what);
}

void testEachChangePaintsOnce()
{
    printf("-- each kind of change paints exactly one frame\n");
    Settled s;
    auto once = [&](const char* what, auto change)
    {
        change();
        const int painted = passes(s.h, 6);
        char text[160];
        snprintf(text, sizeof text, "%s: one frame painted (painted %d)", what, painted);
        check(painted == 1, text);
        const int after = passes(s.h, 20);
        snprintf(text, sizeof text, "%s: and then nothing (painted %d)", what, after);
        check(after == 0, text);
    };
    once("a property (Label::setText)", [&] { s.label->setText("another text"); });
    once("a size (Widget::setRect)", [&] { s.button->setRect({ 240, 636, 140, 28 }); });
    once("a widget made invisible", [&] { s.button->setVisible(false); });
    once("... and visible again", [&] { s.button->setVisible(true); });
    once("a layout asked for", [&] { s.h.app.requestLayout(); });
    once("a relayout asked for", [&] { s.h.app.requestRelayout(); });
    once("a request (requestFrame)", [&] { s.h.app.requestFrame(); });
    once("a request from another thread", [&] { s.h.app.requestFrameFromAnyThread(); });
    once("a theme replaced by name", [&] { s.h.app.setThemePreset("light"); });
    once("a theme replaced by hand", [&] { Theme::instance() = Theme::dark(); });
    once("the pointer moving", [&] { s.h.io().mouseX = 250.0f; s.h.io().mouseY = 650.0f; });
    once("the pointer moving again (a hover)", [&] { s.h.io().mouseX = 255.0f; s.h.io().mouseY = 650.0f; });
    once("a button pressed", [&] { s.h.io().mouseDown[0] = true; });
    once("a button released", [&] { s.h.io().mouseDown[0] = false; });
    once("the wheel", [&] { s.h.io().mouseWheelY = 1.0f; });
    once("the window resized", [&] { s.h.io().displayWidth = 1200.0f; });
    once("the window resized back", [&] { s.h.io().displayWidth = 1280.0f; });
    once("a widget added", [&] { s.extra_->createChild<Label>("new")->setRect({ 700, 640, 80, 24 }); s.h.app.requestRelayout(); });

    // a key into a text input that has the focus
    s.h.app.setFocused(s.input);
    passes(s.h, 4);
    // the caret blinks: its own case; here, only that a key is one frame at the moment it comes (a blink flip may fall in the six)
    s.h.io().addInputCharacter('x');
    const int typed = passes(s.h, 1);
    check(typed == 1, "a typed letter paints the frame it comes in");
    s.h.app.setFocused(nullptr);
    passes(s.h, 4);
}

void testTimeKeepsTime()
{
    printf("-- what runs on time\n");
    // an animation paints every frame until it ends
    {
        Settled s;
        float seen = 0.0f;
        Animator::animate(0.0f, 1.0f, 0.5f, EaseType::Linear, [&seen](float v) { seen = v; });
        const int painted = passes(s.h, 60);
        char what[120];
        snprintf(what, sizeof what, "an animation of half a second paints a frame for each pass it runs (painted %d of 60, ended at %.2f)", painted, seen);
        check(painted >= 27 && painted <= 34 && seen == 1.0f, what);
        check(passes(s.h, 30) == 0, "... and nothing after it ended");
    }
    // a caret flips twice a second, by the application's clock
    {
        Settled s;
        s.h.app.setFocused(s.input);
        int firstFlip = -1;
        ct::Vector<int> flips;
        passes(s.h, 1);
        uint64_t previous = gLastSum;
        for (int i = 1; i <= 190; ++i)
            if (pass(s.h, true))
            {
                if (gLastSum != previous) flips.push_back(i);
                previous = gLastSum;
                if (firstFlip < 0) firstFlip = i;
            }
        char what[200];
        snprintf(what, sizeof what, "the caret flips %d times in 190 frames of 1/60 s (a half second is 30 frames): at", static_cast<int>(flips.size()));
        for (size_t i = 0; i < flips.size(); ++i) snprintf(what + strlen(what), sizeof what - strlen(what), " %d", flips[i]);
        printf("  %s\n", what);
        check(flips.size() >= 5 && flips.size() <= 7, "the caret flips about six times in three seconds (frames painted are the flips and not every frame)");
        for (size_t i = 0; i < flips.size(); ++i)
        {
            const int expected = static_cast<int>((i + 1)) * 30;
            check(flips[i] >= expected - 3 && flips[i] <= expected + 3, "a flip lands within 3 frames of its half second");
        }
        s.h.app.setFocused(nullptr);
    }
    // a tooltip comes on at its delay
    {
        Settled s;
        s.button->setTooltip("a tip");
        s.h.moveTo(300.0f, 650.0f);
        passes(s.h, 1);
        const float delay = s.button->tooltipDelay();
        int shownAt = -1;
        for (int i = 1; i <= 120 && shownAt < 0; ++i)
            if (pass(s.h, true)) shownAt = i;
        char what[140];
        snprintf(what, sizeof what, "the tooltip comes on at its delay of %.2f s (frame %d, 1/60 s each)", delay, shownAt);
        check(shownAt > 0 && fabsf(static_cast<float>(shownAt) / 60.0f - delay) < 0.1f, what);
        check(passes(s.h, 30) == 0, "... and then nothing paints");
        s.h.io().mouseX = 900.0f;
        s.h.io().mouseY = 600.0f;
        check(passes(s.h, 4) >= 1, "moving away paints (the tooltip goes)");
    }
    // a request for later is made good when due
    {
        Settled s;
        s.h.app.requestFrameIn(0.25f);
        int at = -1;
        for (int i = 1; i <= 60 && at < 0; ++i)
            if (pass(s.h, true)) at = i;
        char what[100];
        snprintf(what, sizeof what, "requestFrameIn(0.25) paints at frame %d of 1/60 s (15 is a quarter second)", at);
        check(at >= 14 && at <= 17, what);
        check(passes(s.h, 30) == 0, "... once");
    }
    // nextFrameInMs says when
    {
        Settled s;
        s.h.app.requestFrameIn(0.5f);
        passes(s.h, 1);
        const int ms = s.h.app.nextFrameInMs();
        char what[100];
        snprintf(what, sizeof what, "nextFrameInMs for a frame asked in half a second is about 500 (%d)", ms);
        check(ms > 450 && ms <= 520, what);
    }
}

// ── skipping leaves no stale picture ────────────────────────────────────────────────────────────────────────────────

// A script of changes, each followed by a few passes; the picture after each step (the last frame that was painted).
void script(bool skipping, ct::Vector<uint64_t>& pictures)
{
    Settled s;
    Rng rng(12345u);
    for (int step = 0; step < 60; ++step)
    {
        switch (rng.below(9))
        {
            case 0: s.label->setText(String("text ") + String::number(step)); break;
            case 1: s.h.io().mouseX = rng.range(0.0f, 1270.0f); s.h.io().mouseY = rng.range(0.0f, 700.0f); break;
            case 2: s.h.io().mouseDown[0] = true; break;
            case 3: s.h.io().mouseDown[0] = false; break;
            case 4: s.h.io().mouseWheelY = rng.chance(50) ? 1.0f : -1.0f; break;
            case 5: s.button->setRect({ 240.0f + static_cast<float>(rng.below(60)), 636, 120, 28 }); break;
            case 6: s.h.app.setThemePreset(rng.chance(50) ? "light" : "dark"); break;
            case 7: s.h.io().displayWidth = rng.chance(50) ? 1280.0f : 1100.0f; break;
            case 8: s.h.app.requestLayout(); break;
        }
        s.h.app.setFocused(nullptr); // (a click may have focused a text widget: its caret is on time, not in step)
        passes(s.h, 4, skipping);
        pictures.push_back(gLastSum);
    }
    Theme::instance() = Theme::dark();
}

void testNoStalePicture()
{
    printf("-- the same pictures, painting every frame and skipping\n");
    ct::Vector<uint64_t> every, skipped;
    script(false, every);
    script(true, skipped);
    int differing = 0;
    for (size_t i = 0; i < every.size(); ++i)
        if (every[i] != skipped[i]) ++differing;
    char what[100];
    snprintf(what, sizeof what, "60 steps of random changes end in the same picture (%d of 60 differ)", differing);
    check(differing == 0, what);
}

} // namespace

int main()
{
    testNothingChanged();
    testEachChangePaintsOnce();
    testTimeKeepsTime();
    testNoStalePicture();
    Theme::instance() = Theme::dark();
    shutdown();
    if (failures() == 0) printf("PASS\n");
    return failures() == 0 ? 0 : 1;
}
