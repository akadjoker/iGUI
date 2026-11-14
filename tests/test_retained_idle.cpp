// The frame of a window nobody touches: it must not allocate. The applications run continuously and
// redraw the interface every frame, so a heap allocation per widget per frame is a steady cost (and
// memory churn) for nothing; the clip stacks, icon outlines and the timeline's row lists used to be
// made again each time. Counted with the allocation counter of tests/AllocCounter.hpp; the same
// window in the states an application sits in: plain, a text edit with the focus, a menu open, a
// drop-down open, a tooltip showing, a clip being dragged.

#include "AllocCounter.hpp"
#include "EditorTree.hpp"

using namespace rh;

namespace
{

unsigned long long allocsOfAFrame(Harness& h)
{
    h.frames(3); // settle: lists grow to their size once
    const unsigned long long before = gAllocs;
    for (int i = 0; i < 10; ++i) h.frame();
    return (gAllocs - before) / 10;
}

void testIdleFrameDoesNotAllocate()
{
    printf("an idle frame of the editor window does not allocate\n");
    Harness h(1600.0f, 900.0f);
    EditorTree ed;
    ed.build(h, 300);
    h.relayout();
    h.frames(5);
    check(allocsOfAFrame(h) == 0, "plain");

    h.clickWidget(ed.notes);
    h.frames(2);
    check(allocsOfAFrame(h) == 0, "a text edit with the focus");

    ed.menuBar->openMenu(1);
    h.frames(3);
    check(allocsOfAFrame(h) == 0, "a menu open");
    ed.menuBar->closeMenu();
    h.frames(30);

    ed.combos[1]->openDropdown();
    h.frames(30);
    check(allocsOfAFrame(h) == 0, "a drop-down open");
    h.app.closePopup();
    h.frames(30);

    ed.sliders[0]->setTooltip("a tooltip");
    float x = 0, y = 0;
    Harness::centre(ed.sliders[0], x, y);
    h.moveTo(x, y);
    h.io().deltaTime = 1.0f;
    h.frames(3);
    h.io().deltaTime = 1.0f / 60.0f;
    check(allocsOfAFrame(h) == 0, "a tooltip showing");
    h.moveTo(5.0f, 5.0f);

    ed.timeline->pointAt(0, 3.0f, x, y);
    h.moveTo(x, y);
    h.down();
    h.moveTo(x + 40.0f, y);
    check(allocsOfAFrame(h) == 0, "a clip held down mid-drag");
    h.up();
}

} // namespace

int main()
{
    testIdleFrameDoesNotAllocate();
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
