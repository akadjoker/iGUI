// The frame of a window nobody touches: it must not allocate. The applications run continuously and
// redraw the interface every frame, so a heap allocation per widget per frame is a steady cost (and
// memory churn) for nothing; the clip stacks, icon outlines and the timeline's row lists used to be
// made again each time. Counted with the allocation counter of tests/AllocCounter.hpp; the same
// window in the states an application sits in: plain, a text edit with the focus, a menu open, a
// drop-down open, a tooltip showing, a clip being dragged.

#include "AllocCounter.hpp"
#include "EditorTree.hpp"

#include <igui/widgets/TextInputWidgets.hpp>

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

// Text fields with text that does not fit the small string of ct::String (16 bytes): a text editor
// whose lines are long (a copy of each visible line, and another with the tabs expanded, were made in
// every frame) and a field with a long text (a copy of it for the paint and another of the text before
// the cursor), unfocused, focused and with a selection.
void testTextFieldsDoNotAllocateInAnIdleFrame()
{
    printf("an idle frame of text fields with long text does not allocate\n");
    Harness h(900.0f, 600.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    TextInput* field = box->createChild<TextInput>("um campo de texto com bastante texto dentro dele");
    String doc;
    for (int i = 0; i < 40; ++i) doc += String("\tuma linha de texto numero ") + String::number(i) + " do documento\n";
    TextEdit* editor = box->createChild<TextEdit>(doc);
    editor->setStretch(1.0f);
    h.relayout();
    h.frames(5);
    check(allocsOfAFrame(h) == 0, "a field and an editor with long text, nothing focused");
    h.clickWidget(field);
    h.frames(2);
    check(allocsOfAFrame(h) == 0, "the field focused (the cursor measured)");
    field->selectAll();
    check(allocsOfAFrame(h) == 0, "the field with everything selected");
    h.clickWidget(editor);
    h.frames(2);
    check(allocsOfAFrame(h) == 0, "the editor focused");
    editor->selectAll();
    h.frames(2);
    check(allocsOfAFrame(h) == 0, "the editor with everything selected");
}

} // namespace

int main()
{
    testIdleFrameDoesNotAllocate();
    testTextFieldsDoNotAllocateInAnIdleFrame();
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
