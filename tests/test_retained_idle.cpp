// The frame of a window nobody touches: it must not allocate. The applications run continuously and
// redraw the interface every frame, so a heap allocation per widget per frame is a steady cost (and
// memory churn) for nothing; the clip stacks, icon outlines and the timeline's row lists used to be
// made again each time. Counted with the allocation counter of tests/AllocCounter.hpp; the same
// window in the states an application sits in: plain, a text edit with the focus, a menu open, a
// drop-down open, a tooltip showing, a clip being dragged.

#include "AllocCounter.hpp"
#include "EditorTree.hpp"

#include <igui/widgets/AppWidgets.hpp>
#include <igui/widgets/InputWidgets.hpp>
#include <igui/widgets/SpinSlider.hpp>
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

// Text that is wider than its widget is cut with "..." to its room, and what that came to (the text, the room, the
// cut text and its width) is kept between frames (ElideMemo): the frame of a window nobody touches does not
// allocate for it. The cut text, and a copy of it, were made again in every frame.
void testSplitButtonTextDoesNotAllocate()
{
    printf("an idle frame with a SplitButton whose text does not fit does not allocate\n");
    Harness h(200.0f, 120.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->createChild<SplitButton>("Uma accao com um nome muito comprido demais para o botao");
    h.relayout();
    h.frames(5);
    check(allocsOfAFrame(h) == 0, "a SplitButton whose text is wider than its main button");
}

void testProgressBarTextDoesNotAllocate()
{
    printf("an idle frame with a ProgressBar whose text does not fit does not allocate\n");
    Harness h(200.0f, 120.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    ProgressBar* bar = box->createChild<ProgressBar>(0.0f, 1.0f, 0.5f);
    bar->setText("Um texto muito mais largo do que a barra toda, com a percentagem");
    h.relayout();
    h.frames(5);
    check(allocsOfAFrame(h) == 0, "a ProgressBar whose text is wider than the bar");
}

void testSpinBoxTextDoesNotAllocate()
{
    printf("an idle frame with a SpinBox whose value does not fit does not allocate\n");
    Harness h(200.0f, 120.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    SpinBox* spin = box->createChild<SpinBox>(0.0f, 1000000.0f, 123456.0f, 1.0f);
    spin->setPrefix("preco: ");
    spin->setSuffix(" euros e centimos de euro");
    h.relayout();
    h.frames(5);
    check(allocsOfAFrame(h) == 0, "a SpinBox whose value, with its prefix and suffix, is wider than the room between its buttons");
}

void testSpinSliderLabelDoesNotAllocate()
{
    printf("an idle frame with a SpinSlider whose label does not fit does not allocate\n");
    Harness h(200.0f, 120.0f);
    BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    SpinSlider* slider = box->createChild<SpinSlider>();
    slider->setLabel("Um nome de parametro muito comprido para o campo");
    h.relayout();
    h.frames(5);
    check(allocsOfAFrame(h) == 0, "a SpinSlider whose label is wider than the widget");
}

// A widget that counts how many times a layout pass reaches it.
class LayoutProbe : public Widget
{
public:
    IG_WIDGET_CLASS(LayoutProbe, Widget);
    void layout() override
    {
        ++laidOut;
        Widget::layout();
    }
    int laidOut = 0;
};

// The tree asks for its own layout when it changes (children, visibility, text...): an idle window, and one the pointer
// moves over, must not run a layout pass at all.
void testIdleFrameDoesNoLayout()
{
    printf("an idle frame, and a pointer moving over the editor window, do no layout\n");
    Harness h(1600.0f, 900.0f);
    EditorTree ed;
    ed.build(h, 300);
    LayoutProbe* probe = h.root()->createChild<LayoutProbe>();
    h.relayout();
    h.frames(10);
    const int settled = probe->laidOut;
    h.frames(30);
    check(probe->laidOut == settled, "idle frames do no layout");
    for (int i = 0; i < 60; ++i)
    {
        h.moveTo(40.0f + static_cast<float>(i) * 25.0f, 30.0f + static_cast<float>((i * 37) % 800));
        h.frame();
    }
    if (probe->laidOut != settled) printf("    %d layout passes while the pointer moved\n", probe->laidOut - settled);
    check(probe->laidOut == settled, "a pointer moving over the window does no layout");
}

} // namespace

int main()
{
    testIdleFrameDoesNotAllocate();
    testIdleFrameDoesNoLayout();
    testTextFieldsDoNotAllocateInAnIdleFrame();
    testSplitButtonTextDoesNotAllocate();
    testProgressBarTextDoesNotAllocate();
    testSpinBoxTextDoesNotAllocate();
    testSpinSliderLabelDoesNotAllocate();
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
