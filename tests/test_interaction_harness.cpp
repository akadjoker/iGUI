// Tests for the harness itself, plus the click-overlap cases it exists to
// catch.

#include "InteractionHarness.hpp"

#include <assert.h>
#include <stdio.h>

namespace
{

// button(label, Rect)'s Rect is window-relative, not screen-relative -
// beginWindow's bounds place the window, and the button's local rect is added
// to the layout cursor on top of that. Get this wrong and every widget in the
// window silently fails to react to a click, which looks like a hit-testing
// bug until you check where the geometry actually landed. Window at the
// origin here so an offset mistake can't cancel out and hide it.
void test_click_hits_only_its_own_button()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool clickedA = false;
    bool clickedB = false;
    auto build = [&](ig::Context &c) {
        clickedA = false;
        clickedB = false;
        c.beginWindow("w", ig::Rect(0.0f, 0.0f, 300.0f, 200.0f));
        if (c.button("A", ig::Rect(0.0f, 0.0f, 80.0f, 24.0f)))
            clickedA = true;
        // button(label, Rect) is manual placement, not auto-layout - it
        // doesn't feed sameLine(), so B needs its own explicit x.
        if (c.button("B", ig::Rect(90.0f, 0.0f, 80.0f, 24.0f)))
            clickedB = true;
        c.endWindow();
    };

    harness.frame(build);
    // Window padding places the first widget at (8, 32), not (0, 0).
    assert(harness.hasVertexAt(8.0f, 32.0f));
    assert(harness.hasVertexAt(98.0f, 32.0f));

    harness.click(18.0f, 42.0f, build);
    assert(clickedA && !clickedB);

    harness.click(108.0f, 42.0f, build);
    assert(clickedB && !clickedA);

    harness.click(93.0f, 42.0f, build);
    assert(!clickedA && !clickedB);
}

// Two plain button() calls sharing screen area: the FIRST one drawn claims the
// click, not the last - itemClicked() latches activeWidget_ on whichever
// widget the press lands in first, and a later widget with activeWidget_
// already set to someone else is locked out for that press. A real overlay
// (a popup, a context menu) needs raiseWindow()/its own modal handling to win
// that fight; two ordinary buttons in the same window do not get it for free.
void test_first_button_at_a_point_wins_the_overlap()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool clickedBase = false;
    bool clickedOverlay = false;
    auto build = [&](ig::Context &c) {
        clickedBase = false;
        clickedOverlay = false;
        c.beginWindow("w", ig::Rect(0.0f, 0.0f, 300.0f, 200.0f));
        if (c.button("base", ig::Rect(10.0f, 10.0f, 100.0f, 40.0f)))
            clickedBase = true;
        if (c.button("overlay", ig::Rect(60.0f, 10.0f, 50.0f, 40.0f)))
            clickedOverlay = true;
        c.endWindow();
    };

    harness.frame(build);
    // base at (18, 42)-(118, 82), overlay at (68, 42)-(118, 82): they share
    // (68, 42)-(118, 82).
    assert(harness.hasVertexAt(18.0f, 42.0f));
    assert(harness.hasVertexAt(68.0f, 42.0f));

    harness.click(80.0f, 55.0f, build); // shared area: base was drawn first
    assert(clickedBase && !clickedOverlay);

    harness.click(30.0f, 55.0f, build); // base-only area
    assert(clickedBase && !clickedOverlay);
}

void test_drag_reports_intermediate_state()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    float value = 0.0f;

    int frameCount = 0;
    auto build = [&](ig::Context &c) {
        ++frameCount;
        c.beginWindow("w", ig::Rect(0.0f, 0.0f, 300.0f, 200.0f));
        c.sliderFloat("v", value, 0.0f, 1.0f, ig::Rect(8.0f, 8.0f, 160.0f, 28.0f));
        c.endWindow();
    };

    harness.frame(build);
    assert(harness.hasVertexAt(32.0f, 51.2f)); // slider track top-left

    const int before = frameCount;
    harness.drag(32.0f, 54.0f, 114.0f, 54.0f, build, 5);
    assert(frameCount - before == 6); // press + 3 moves + final move + release
    assert(value > 0.7f);
}

void test_vertex_and_color_queries()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    harness.frame([](ig::Context &c) {
        c.beginWindow("w", ig::Rect(0.0f, 0.0f, 300.0f, 200.0f));
        c.drawRectFilledRounded(ig::Rect(20.0f, 20.0f, 40.0f, 40.0f), 0.0f,
                                ig::Color(255u, 0u, 0u, 255u));
        c.endWindow();
    });

    assert(harness.hasVertexAt(28.0f, 52.0f));
    assert(!harness.hasVertexAt(999.0f, 999.0f));
    assert(harness.colorAtVertex(28.0f, 52.0f) == ig::Color(255u, 0u, 0u, 255u));
    assert(harness.colorAtVertex(999.0f, 999.0f).a == 0u);
    assert(harness.vertexCount() > 0u);
}

// beginDialog() centres itself, paints a scrim, blocks the windows behind it,
// and closes on Escape - the parts an application would otherwise hand-roll
// around beginWindow() + setModalWindow().
void test_dialog_blocks_and_closes()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);

    bool open = true;
    bool clickedBehind = false;
    bool clickedInside = false;
    auto build = [&](ig::Context &c) {
        clickedBehind = false;
        clickedInside = false;
        c.beginMainWindow("main");
        if (c.button("behind", ig::Rect(0.0f, 0.0f, 200.0f, 40.0f)))
            clickedBehind = true;
        c.endWindow();
        if (c.beginDialog("Settings", open, ig::Vec2(380.0f, 280.0f)))
        {
            if (c.button("inside", ig::Rect(0.0f, 0.0f, 120.0f, 30.0f)))
                clickedInside = true;
            c.endDialog();
        }
    };

    harness.frame(build);
    // Centred in the 1280x800 viewport: (450, 260)-(830, 540).
    assert(harness.hasVertexAt(450.0f, 260.0f));
    // Content starts below the title bar, at (450, 288).
    assert(harness.hasVertexAt(450.0f, 288.0f));

    harness.click(100.0f, 100.0f, build);
    assert(!clickedBehind);

    harness.click(510.0f, 303.0f, build);
    assert(clickedInside);

    harness.queue(ig::Event::keyDown(ig::KeyCode::Escape));
    harness.frame(build);
    assert(!open);
}

} // namespace

int main()
{
    test_click_hits_only_its_own_button();
    test_first_button_at_a_point_wins_the_overlap();
    test_drag_reports_intermediate_state();
    test_vertex_and_color_queries();
    test_dialog_blocks_and_closes();
    printf("test_interaction_harness: all tests passed\n");
    return 0;
}
