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
    h.relayout(); // the harness still paints with its (stale) pointer? it re-reads the atlas
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

void testComboDeletedFromItsSignal()
{
    printf("combo box deleted or cleared from selectionChanged\n");
    for (int variant = 0; variant < 3; ++variant)
    {
        Harness h;
        BoxLayout* box = column(h);
        ComboBox* combo = box->createChild<ComboBox>();
        combo->addItem("Media files");
        combo->addItem("Transições");
        combo->addItem("Títulos");
        h.relayout();
        ComboBox* self = combo;
        combo->selectionChanged.connect(
                [&, variant](int)
                {
                    if (!self) return;
                    ComboBox* dead = self;
                    self = nullptr;
                    if (variant == 0) box->removeChild(dead);
                    else if (variant == 1) dead->clear();
                    else
                        dead->setVisible(false);
                });
        h.clickWidget(combo); // open
        h.relayout();
        check(combo->isOpen() || variant != 3, "opened");
        // Click the second row of the dropdown.
        const Rect r = combo->absoluteRect();
        h.click(r.x + r.w * 0.5f, r.y + r.h + 24.0f);
        h.frames(3);
        h.moveTo(3.0f, 3.0f);
    }
}

void testMenuActionDeletesBar()
{
    printf("a menu action deletes the window's tree\n");
    Harness h;
    BoxLayout* box = column(h);
    MenuBar* bar = box->createChild<MenuBar>();
    Menu* file = bar->addMenu("File");
    bool ran = false;
    file->addAction("Close all",
            [&]
            {
                ran = true;
                h.root()->removeChild(box);
                box = nullptr;
            });
    file->addAction("Other");
    h.relayout();
    bar->openMenu(0);
    h.relayout();
    const Rect item = file->actionRect(0);
    h.click(item.x + item.w * 0.5f, item.y + item.h * 0.5f);
    h.frames(3);
    check(ran, "the action ran");
    check(h.root()->children().empty(), "the tree is gone");
    h.moveTo(10.0f, 10.0f);
    h.click(10.0f, 10.0f);
}

void testMenuDeletedWhileOpen()
{
    printf("a context menu deleted while it is the popup\n");
    Harness h;
    BoxLayout* box = column(h);
    Panel* panel = box->createChild<Panel>();
    panel->setStretch(1.0f);
    auto* menu = new Menu();
    menu->addAction("One");
    menu->addAction("Two");
    panel->setContextMenu(menu);
    h.relayout();
    float x = 0, y = 0;
    Harness::centre(panel, x, y);
    h.click(x, y, 1);
    h.relayout();
    check(h.app.popup() != nullptr, "the menu is open");
    panel->setContextMenu(nullptr); // the application drops it while it is shown
    h.frames(3);
    h.click(x + 3.0f, y + 3.0f);
    h.relayout();
}

void testTextEditDeletedFromChange()
{
    printf("text edit deleted from textChanged, keys and text input\n");
    for (int variant = 0; variant < 3; ++variant)
    {
        Harness h;
        BoxLayout* box = column(h);
        TextEdit* edit = box->createChild<TextEdit>();
        edit->setStretch(1.0f);
        TextEdit* self = edit;
        auto kill = [&]
        {
            if (!self) return;
            TextEdit* dead = self;
            self = nullptr;
            box->removeChild(dead);
        };
        edit->textChanged.connect([&] { kill(); });
        h.relayout();
        h.clickWidget(edit);
        if (variant == 0) h.type("a");
        else if (variant == 1)
            h.type("ç\xF0\x9F\x8E\xB5");
        else
        {
            h.type("ab");
            h.key(Key::Backspace);
        }
        h.frames(3);
        check(self == nullptr, "the text edit removed itself");
        h.key(Key::Return);
        h.type("x");
    }
}

void testUnhandledKeyDeletes()
{
    printf("an unhandled-key handler deletes widgets\n");
    Harness h;
    BoxLayout* box = column(h);
    Button* b = box->createChild<Button>("focus me");
    h.relayout();
    h.clickWidget(b);
    h.app.unhandledKey.connect(
            [&](KeyEvent&)
            {
                if (box)
                {
                    h.root()->removeChild(box);
                    box = nullptr;
                }
            });
    h.key(Key::F5);
    check(box == nullptr, "the handler ran");
    h.key(Key::F5);
    h.type("z");
}

void testStageSwitchAndRemoveInEvent()
{
    printf("a click switches the stage and removes the one it came from\n");
    Harness h;
    WidgetApp& app = h.app;
    Widget* other = app.addStage("other");
    other->createChild<Label>("other");
    BoxLayout* box = column(h);
    Button* b = box->createChild<Button>("go");
    b->clicked.connect(
            [&]
            {
                app.setStage("other", TransitionType::None);
                app.removeStage("harness");
            });
    h.relayout();
    h.clickWidget(b);
    h.frames(3);
    check(app.currentStageName() == "other", "now on the other stage");
    h.moveTo(20.0f, 20.0f);
    h.click(20.0f, 20.0f);
    app.setStage("harness", TransitionType::None); // gone: false, no crash
}

void testFloatClosedByItsButton()
{
    printf("a float window closed from its own button, twice\n");
    Harness h;
    auto* fw = h.app.addFloat<FloatWindow>("Dialog");
    fw->setFloatPos(100.0f, 100.0f);
    fw->setFloatSize(300.0f, 200.0f);
    auto* box = new BoxLayout(LayoutDir::Vertical);
    fw->setContentWidget(box);
    Button* ok = box->createChild<Button>("OK");
    ok->clicked.connect(
            [&]
            {
                h.app.removeFloat(fw);
                h.app.removeFloat(fw);
            });
    h.relayout();
    h.clickWidget(ok);
    h.frames(3);
    check(h.app.floats().empty(), "the window is closed");
    h.moveTo(150.0f, 150.0f);
}

void testTooltipOnRemovedWidget()
{
    printf("tooltip of a widget removed while it shows\n");
    Harness h;
    BoxLayout* box = column(h);
    Button* b = box->createChild<Button>("tip");
    b->setTooltip("a tooltip that must not outlive the button");
    h.relayout();
    float x = 0, y = 0;
    Harness::centre(b, x, y);
    h.moveTo(x, y);
    h.io().deltaTime = 1.0f;
    h.frames(3);
    h.io().deltaTime = 1.0f / 60.0f;
    box->removeChild(b);
    h.frames(3);
}

// A widget deleted while a drag started from it is in flight.
class DragSource : public Widget
{
public:
    DragSource() { cursor_ = CursorType::Hand; }
    bool isDragSource() const override { return true; }
    DragPayload onDragBegin() override
    {
        DragPayload p;
        p.type = "thing";
        p.data = 1;
        p.source = this;
        return p;
    }
    void onDragEnd(bool) override { ++ended; }
    Vec2f sizeHint() const override { return { 80.0f, 40.0f }; }
    int ended = 0;
};

class DropTarget : public Widget
{
public:
    bool acceptsDrop(const DragPayload&) override { return true; }
    void onDropReceive(const DragPayload&) override
    {
        ++received;
        if (onDrop) onDrop();
    }
    Vec2f sizeHint() const override { return { 80.0f, 40.0f }; }
    int received = 0;
    ct::Function<void()> onDrop;
};

void testDragSourceDeleted()
{
    printf("drag source deleted mid-drag, drop target deleting itself\n");
    {
        Harness h;
        BoxLayout* box = column(h);
        auto* src = box->createChild<DragSource>();
        auto* dst = box->createChild<DropTarget>();
        h.relayout();
        float sx, sy, dx, dy;
        Harness::centre(src, sx, sy);
        Harness::centre(dst, dx, dy);
        h.moveTo(sx, sy);
        h.down();
        h.moveTo(sx + 20.0f, sy + 20.0f);
        h.moveTo(dx, dy);
        check(h.app.isDragging(), "dragging");
        box->removeChild(src);
        h.relayout();
        h.up();
        h.relayout();
    }
    {
        Harness h;
        BoxLayout* box = column(h);
        auto* src = box->createChild<DragSource>();
        auto* dst = box->createChild<DropTarget>();
        dst->onDrop = [&] { box->removeChild(dst); };
        h.relayout();
        float sx, sy, dx, dy;
        Harness::centre(src, sx, sy);
        Harness::centre(dst, dx, dy);
        h.moveTo(sx, sy);
        h.down();
        h.moveTo(sx + 20.0f, sy + 20.0f);
        h.moveTo(dx, dy);
        h.up();
        h.frames(3);
        check(box->children().size() == 1u, "the target removed itself on drop");
    }
}

void testSliderDeletedWhileDragging()
{
    printf("slider deleted from onValueChanged while it drags\n");
    Harness h;
    BoxLayout* box = column(h);
    Slider* s = box->createChild<Slider>(0.0f, 1.0f, 0.0f);
    s->setStretch(0.0f);
    int changes = 0;
    s->onValueChanged.connect(
            [&](float)
            {
                if (++changes == 2) box->removeChild(s);
            });
    h.relayout();
    const Rect r = s->absoluteRect();
    h.moveTo(r.x + 10.0f, r.y + r.h * 0.5f);
    h.down();
    h.moveTo(r.x + 40.0f, r.y + r.h * 0.5f);
    h.moveTo(r.x + 70.0f, r.y + r.h * 0.5f);
    h.moveTo(r.x + 90.0f, r.y + r.h * 0.5f);
    h.up();
    h.relayout();
    check(changes >= 2, "the slider changed");
}

void testSplitterChildrenRemovedWhileDragging()
{
    printf("splitter loses a child while its handle is dragged\n");
    Harness h;
    auto* split = h.root()->createChild<Splitter>(LayoutDir::Horizontal);
    auto* left = split->createChild<Panel>();
    auto* right = split->createChild<Panel>();
    h.relayout();
    const Rect r = split->absoluteRect();
    const float hx = r.x + r.w * split->ratio();
    h.moveTo(hx, r.y + 50.0f);
    h.down();
    h.moveTo(hx + 40.0f, r.y + 50.0f);
    split->removeChild(left);
    h.moveTo(hx + 80.0f, r.y + 50.0f);
    split->removeChild(right);
    h.moveTo(hx - 80.0f, r.y + 50.0f);
    h.up();
    h.frames(3);
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
    RUN(testComboDeletedFromItsSignal);
    RUN(testMenuActionDeletesBar);
    RUN(testMenuDeletedWhileOpen);
    RUN(testTextEditDeletedFromChange);
    RUN(testUnhandledKeyDeletes);
    RUN(testStageSwitchAndRemoveInEvent);
    RUN(testFloatClosedByItsButton);
    RUN(testTooltipOnRemovedWidget);
    RUN(testDragSourceDeleted);
    RUN(testSliderDeletedWhileDragging);
    RUN(testSplitterChildrenRemovedWhileDragging);
    RUN(testDeleteWholeTreeInsideHover);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
