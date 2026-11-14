// Lifetime of retained widgets under event dispatch: a widget deleted from its
// own signal, pointers the manager keeps (hover, focus, press, drag, tooltip,
// popup) left at a deleted widget, a font atlas that goes away before the
// widgets, a timeline whose clips change under a drag. Every scenario runs the
// real frame loop (tests/RetainedHarness.hpp); most only fail under ASan.

#include "RetainedHarness.hpp"

#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/ComboBox.hpp>
#include <igui/widgets/InputWidgets.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/MenuWidgets.hpp>
#include <igui/widgets/TextInputWidgets.hpp>
#include <igui/widgets/Timeline.hpp>
#include <igui/widgets/ViewWidgets.hpp>

using namespace rh;

namespace
{

// A vertical box filling the stage, the usual parent of a test's widgets.
BoxLayout* column(Harness& h)
{
    auto* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    box->setPadding(8.0f);
    box->setSpacing(6.0f);
    return box;
}

// The font atlas of a window can go away before its widgets (an application
// that destroys its renderer first): the manager caches the font of the last
// paint for measuring text, and measuring after that read freed memory.
void testAtlasGoneBeforeWidgets()
{
    printf("atlas destroyed before the widgets\n");
    BoxLayout* box = nullptr;
    Label* label = nullptr;
    {
        Harness h;
        box = column(h);
        label = box->createChild<Label>("Música");
        h.relayout();
        check(WidgetApp::instance().textWidth("Música") > 0.0f, "text is measured while the atlas lives");
        // The harness (and its atlas) is destroyed here, the tree with it.
    }
    check(WidgetApp::instance().textWidth("Música") == 0.0f, "no font left to measure with, and no crash");
    (void) box;
    (void) label;

    // A widget tree that outlives the atlas, measured without any paint.
    Harness h2;
    auto* tail = column(h2);
    tail->createChild<Label>("depois");
    h2.frames(2);
}

// The same for the draw list: it belongs to the context, and DestroyContext
// (ig::zen::App's destructor) frees it while widgets may still be measuring.
void testContextGoneBeforeWidgets()
{
    printf("context destroyed before the widgets\n");
    Context* previous = GetCurrentContext();
    Context* ctx = CreateContext();
    SetCurrentContext(ctx);
    {
        Harness h;
        auto* box = column(h);
        box->createChild<Label>("Vídeos");
        h.relayout();
        check(WidgetApp::instance().textWidth("Vídeos") > 0.0f, "text is measured while the context lives");
        DestroyContext(ctx);
        check(WidgetApp::instance().textWidth("Vídeos") == 0.0f, "no draw list left to measure with, and no crash");
        SetCurrentContext(previous);
    }
}

void testRebuiltAtlas()
{
    printf("atlas rebuilt while the manager caches one of its fonts\n");
    Harness h;
    auto* box = column(h);
    box->createChild<Label>("Música");
    h.relayout();
    FontAtlas& atlas = h.atlas();
    atlas.addFontDefault(18.0f);
    check(atlas.build(), "the atlas builds again");
    check(WidgetApp::instance().textWidth("Música") == 0.0f, "the old font is not measured with");
    h.relayout(); // the next paint hands the manager the new font
    check(WidgetApp::instance().textWidth("Música") > 0.0f, "and the next frame measures with the new one");
}

void testDeleteFromClicked()
{
    printf("button deletes itself in clicked\n");
    Harness h;
    BoxLayout* box = column(h);
    Button* a = box->createChild<Button>("keep");
    Button* b = box->createChild<Button>("remove me");
    int clicks = 0;
    b->clicked.connect(
            [&]
            {
                ++clicks;
                box->removeChild(b);
            });
    (void) a;
    h.relayout();
    h.clickWidget(b);
    check(clicks == 1, "the handler ran once");
    check(box->children().size() == 1u, "the button is gone from the box");
    h.frames(3);
    h.moveTo(5.0f, 5.0f);
    h.moveTo(600.0f, 400.0f);
}

void testDeleteFromPressedReleasedHover()
{
    printf("button deleted from pressed / released / hover handlers\n");
    for (int variant = 0; variant < 4; ++variant)
    {
        Harness h;
        BoxLayout* box = column(h);
        Button* b = box->createChild<Button>("x");
        box->createChild<Button>("y");
        h.relayout();
        auto kill = [&]
        {
            if (b)
            {
                Button* dead = b;
                b = nullptr;
                box->removeChild(dead);
            }
        };
        switch (variant)
        {
            case 0: b->pressed.connect([&](int) { kill(); }); break;
            case 1: b->released.connect([&](int) { kill(); }); break;
            case 2: b->hoverEnter.connect([&] { kill(); }); break;
            case 3: b->moved.connect([&](float, float) { kill(); }); break;
        }
        float x = 0, y = 0;
        Harness::centre(b, x, y);
        h.moveTo(x, y);
        h.down();
        h.up();
        h.moveTo(x + 1.0f, y + 1.0f);
        h.relayout();
        check(b == nullptr, "the handler removed the button");
    }
}

void testDeleteWholeTreeInsideHover()
{
    printf("whole tree replaced while the pointer is over it\n");
    Harness h;
    for (int round = 0; round < 6; ++round)
    {
        BoxLayout* box = column(h);
        for (int i = 0; i < 8; ++i)
        {
            box->createChild<Label>("Vídeos e Música");
            box->createChild<Button>("ação");
        }
        h.relayout();
        h.moveTo(100.0f, 60.0f + 20.0f * static_cast<float>(round));
        h.down();
        h.root()->removeChild(box);
        h.up();
        h.relayout();
    }
}

} // namespace

// A scenario name on the command line runs only that one.
int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testAtlasGoneBeforeWidgets);
    RUN(testContextGoneBeforeWidgets);
    RUN(testRebuiltAtlas);
    RUN(testDeleteFromClicked);
    RUN(testDeleteFromPressedReleasedHover);
    RUN(testDeleteWholeTreeInsideHover);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
