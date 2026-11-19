// Round 8 (docs/COMPARACAO.md, part 2): edge cases of the retained widgets, headless, through the real frame loop
// (tests/RetainedHarness.hpp), under AddressSanitizer + UndefinedBehaviorSanitizer + float-cast-overflow in tools/check.sh.
//
//   1. A handler that takes its own widget out of the tree inside the signals that rounds 5 to 7 left alone (the case runner is
//      tests/VictimHarness.hpp; the method is at the top of tests/test_retained_round7.cpp): GradientEditor, CurveEditor,
//      SpinSlider's own signals, ConsoleWidget, SymbolOutline, ChoiceDialog, InputBox.
//   2. Zero-size and huge-size rectangles for a wide set of widgets, laid out, painted, hovered and clicked.
//   3. Nothing in a list, a tree, a table, and a million items.
//   4. Empty, invalid UTF-8 and 64 KB strings in the text-bearing widgets.
//   5. Widgets shown, hidden, taken out and put back in a hurry, with the pointer and a stage switch in the middle.
//   6. The pixel scale of the atlas changed while the interface is up (0.5, 1, 2, 3).
//
//   igui_retained_round8_tests [--list] [--only=TEXT]
//
// No standard library: ct and C only.

#include "RetainedHarness.hpp"
#include "StageHarness.hpp"
#include "VictimHarness.hpp"

#include <igui/widgets/AppWidgets.hpp>
#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/ChartWidgets.hpp>
#include <igui/widgets/CodeEditor.hpp>
#include <igui/widgets/ComboBox.hpp>
#include <igui/widgets/ConsoleWidget.hpp>
#include <igui/widgets/DataWidgets.hpp>
#include <igui/widgets/DialogWidgets.hpp>
#include <igui/widgets/InputWidgets.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/ScrollWidgets.hpp>
#include <igui/widgets/SpinSlider.hpp>
#include <igui/widgets/SymbolOutline.hpp>
#include <igui/widgets/TextInputWidgets.hpp>
#include <igui/widgets/Timeline.hpp>
#include <igui/widgets/TreePropertyColorWidgets.hpp>
#include <igui/widgets/ViewWidgets.hpp>

using namespace rh;

namespace
{

// ---- 1. a handler that takes its widget out ----

void testSignalsOfEditors()
{
    auto gradient = [](Stage& st) {
        GradientEditor* w = put<GradientEditor>(st, 20.0f, 20.0f, 300.0f, 60.0f);
        w->addStop(0.2f, Color(255, 0, 0, 255));
        w->addStop(0.8f, Color(0, 0, 255, 255));
        return w;
    };
    victim<GradientEditor>("GradientEditor.onChanged by addStop", gradient,
            [](GradientEditor* w, auto kill, auto) { w->onChanged.connect(kill); },
            [](Stage& st, GradientEditor* w) { st.act([w]() { w->addStop(0.5f, Color(0, 255, 0, 255)); }); });
    victim<GradientEditor>("GradientEditor.onChanged by removeStop", gradient,
            [](GradientEditor* w, auto kill, auto) { w->onChanged.connect(kill); },
            [](Stage& st, GradientEditor* w) { st.act([w]() { w->removeStop(0); }); });
    victim<GradientEditor>("GradientEditor.onChanged by setStopColor", gradient,
            [](GradientEditor* w, auto kill, auto) { w->onChanged.connect(kill); },
            [](Stage& st, GradientEditor* w) { st.act([w]() { w->setStopColor(0, Color(1, 2, 3, 255)); }); });
    victim<GradientEditor>("GradientEditor.onChanged by setStopPos", gradient,
            [](GradientEditor* w, auto kill, auto) { w->onChanged.connect(kill); },
            [](Stage& st, GradientEditor* w) { st.act([w]() { w->setStopPos(0, 0.4f); }); });

    auto curve = [](Stage& st) {
        CurveEditor* w = put<CurveEditor>(st, 20.0f, 20.0f, 300.0f, 160.0f);
        const int c = w->addCurve("c", Color(255, 255, 0, 255));
        w->addKey(c, 0.0f, 0.0f);
        w->addKey(c, 1.0f, 1.0f);
        return w;
    };
    victim<CurveEditor>("CurveEditor.onKeyAdded by addKey", curve,
            [](CurveEditor* w, auto kill, auto) { w->onKeyAdded.connect(kill); },
            [](Stage& st, CurveEditor* w) { st.act([w]() { w->addKey(0, 0.5f, 0.5f); }); });
    victim<CurveEditor>("CurveEditor.onKeyRemoved by removeKey", curve,
            [](CurveEditor* w, auto kill, auto) { w->onKeyRemoved.connect(kill); },
            [](Stage& st, CurveEditor* w) { st.act([w]() { w->removeKey(0, 1); }); });
    victim<CurveEditor>("CurveEditor.onKeyChanged by setKey", curve,
            [](CurveEditor* w, auto kill, auto) { w->onKeyChanged.connect(kill); },
            [](Stage& st, CurveEditor* w) { st.act([w]() { w->setKey(0, 1, 0.9f, 0.9f); }); });
}

void testSignalsOfSpinSlider()
{
    auto spin = [](Stage& st) {
        SpinSlider* w = put<SpinSlider>(st, 20.0f, 20.0f, 160.0f, 28.0f);
        w->setMax(100.0);
        w->setValue(50.0);
        return w;
    };
    victim<SpinSlider>("SpinSlider.grabbed by a press on the field", spin,
            [](SpinSlider* w, auto kill, auto) { w->grabbed.connect(kill); },
            [](Stage& st, SpinSlider* w) {
                st.move(100.0f, 34.0f);
                st.down(w);
                st.up(w);
            });
    victim<SpinSlider>("SpinSlider.valueFocusEntered by a click on the field", spin,
            [](SpinSlider* w, auto kill, auto) { w->valueFocusEntered.connect(kill); },
            [](Stage& st, SpinSlider* w) {
                st.move(100.0f, 34.0f);
                st.down(w);
                st.up(w);
            });
}

void testSignalsOfTimeline()
{
    // three clips on a track, all selected (Ctrl+click), or three keyframes, all selected: dragging one moves them all and the
    // release reports each of them
    auto clips = [](Stage& st) {
        Timeline* w = put<Timeline>(st, 20.0f, 20.0f, 440.0f, 200.0f);
        const int track = w->addTrack("track");
        w->setTimeRange(0.0f, 20.0f);
        w->addClip(track, 1.0f, 3.0f, "a");
        w->addClip(track, 5.0f, 7.0f, "b");
        w->addClip(track, 9.0f, 11.0f, "c");
        st.h.relayout();
        st.h.frames(2);
        float x = 0, y = 0;
        for (int c = 0; c < 3; ++c)
        {
            const TimelineClip& cl = w->track(track).clips[static_cast<size_t>(c)];
            w->pointAt(track, (cl.start + cl.end) * 0.5f, x, y);
            st.h.io().keyCtrl = c > 0;
            st.h.click(x, y);
        }
        st.h.io().keyCtrl = false;
        return w;
    };
    auto dragFirstClip = [](Stage& st, Timeline* w) {
        float x = 0, y = 0;
        const TimelineClip& cl = w->track(0).clips[0];
        w->pointAt(0, (cl.start + cl.end) * 0.5f, x, y);
        st.move(x, y);
        st.down(w);
        st.drag(w, x + 20.0f, y);
        st.drag(w, x + 40.0f, y);
        st.up(w);
    };
    victim<Timeline>("Timeline.onClipEdited by dragging a group of three clips", clips,
            [](Timeline* w, auto kill, auto) { w->onClipEdited.connect(kill); }, dragFirstClip, 0, nullptr, 1);
    victim<Timeline>("Timeline.onClipEdited by dragging one clip",
            [](Stage& st) {
                Timeline* w = put<Timeline>(st, 20.0f, 20.0f, 440.0f, 200.0f);
                const int track = w->addTrack("track");
                w->setTimeRange(0.0f, 20.0f);
                w->addClip(track, 1.0f, 3.0f, "a");
                st.h.relayout();
                st.h.frames(2);
                return w;
            },
            [](Timeline* w, auto kill, auto) { w->onClipEdited.connect(kill); }, dragFirstClip);

    auto keys = [](Stage& st) {
        Timeline* w = put<Timeline>(st, 20.0f, 20.0f, 440.0f, 200.0f);
        const int track = w->addTrack("track");
        w->setTimeRange(0.0f, 20.0f);
        w->addClip(track, 0.0f, 0.0f, "");
        for (int k = 0; k < 3; ++k) w->addKeyframe(track, 2.0f + 4.0f * static_cast<float>(k));
        st.h.relayout();
        st.h.frames(2);
        float x = 0, y = 0;
        for (int k = 0; k < 3; ++k)
        {
            w->pointAt(track, 2.0f + 4.0f * static_cast<float>(k), x, y);
            st.h.io().keyCtrl = k > 0;
            st.h.click(x, y);
        }
        st.h.io().keyCtrl = false;
        return w;
    };
    victim<Timeline>("Timeline.onKeyframeDropped by dragging a group of three keyframes", keys,
            [](Timeline* w, auto kill, auto) { w->onKeyframeDropped.connect(kill); },
            [](Stage& st, Timeline* w) {
                float x = 0, y = 0;
                w->pointAt(0, 2.0f, x, y);
                st.move(x, y);
                st.down(w);
                st.drag(w, x + 20.0f, y);
                st.drag(w, x + 40.0f, y);
                st.up(w);
            });
}

// The callback of a property row (a ct::Function the application gave) is a handler too, and propertyChanged follows it: neither may
// come from a grid that the callback took out. The control is the same grid with a callback that does nothing.
template<class AddRow, class Drive>
void propertyRow(const char* name, AddRow addRow, Drive drive, int modes = 2)
{
    if (!wanted(name)) return;
    ++victimState().cases;
    for (int remove = 0; remove < 2; ++remove)
        for (int mode = 0; mode < modes; ++mode)
        {
            Stage st(mode);
            PropertyGrid* w = put<PropertyGrid>(st, 20.0f, 20.0f, 300.0f, 200.0f);
            int callbacks = 0, signals = 0;
            addRow(w, [&, w]() {
                ++callbacks;
                if (remove) st.holder->removeChild(w);
            });
            w->propertyChanged.connect([&](int) { ++signals; });
            st.h.relayout();
            st.h.frames(2);
            st.watch(w);
            drive(st, w);
            st.h.frames(3);
            const bool ok = callbacks >= 1 && (remove ? (callbacks == 1 && signals == 0) : signals >= 1) && st.gone() == (remove != 0);
            check(ok, say(remove ? name : (String("CONTROL ") + name).c_str(), mode).c_str());
            if (!ok) printf("    callback %d times, propertyChanged %d times, gone %d\n", callbacks, signals, st.gone());
        }
}

void testSignalsOfPropertyGrid()
{
    propertyRow("PropertyGrid.onChange callback of a float row, by a drag of its spin",
            [](PropertyGrid* w, ct::Function<void()> f) { w->addFloat("size", 0.5f, 0.0f, 1.0f, [f](float) { f(); }); },
            [](Stage& st, PropertyGrid* w) {
                const Rect r = w->absoluteRect();
                st.move(r.x + 225.0f, r.y + 12.0f);
                st.down(w);
                st.drag(w, r.x + 245.0f, r.y + 12.0f);
                st.drag(w, r.x + 265.0f, r.y + 12.0f);
                st.up(w);
            }, 1); // (a drag of the spin runs through the events of the app: there is no call of the grid's handlers that makes it)
    propertyRow("PropertyGrid.onChange callback of an int row, by a drag of its spin",
            [](PropertyGrid* w, ct::Function<void()> f) { w->addInt("count", 5, 0, 100, [f](int) { f(); }); },
            [](Stage& st, PropertyGrid* w) {
                const Rect r = w->absoluteRect();
                st.move(r.x + 225.0f, r.y + 12.0f);
                st.down(w);
                st.drag(w, r.x + 245.0f, r.y + 12.0f);
                st.drag(w, r.x + 265.0f, r.y + 12.0f);
                st.up(w);
            }, 1);
    propertyRow("PropertyGrid.onChange callback of a string row, by typing and Return",
            [](PropertyGrid* w, ct::Function<void()> f) { w->addString("name", "a", [f](const String&) { f(); }); },
            [](Stage& st, PropertyGrid* w) {
                st.click(w, 225.0f, 12.0f);
                st.type(w, "x");
                st.key(w, Key::Return);
            });
}

void testSignalsOfConsoleAndOutline()
{
    victim<ConsoleWidget>("ConsoleWidget.onCommand by Return",
            [](Stage& st) { return put<ConsoleWidget>(st, 10.0f, 10.0f, 440.0f, 220.0f); },
            [](ConsoleWidget* w, auto kill, auto) { w->onCommand.connect(kill); },
            [](Stage& st, ConsoleWidget* w) {
                st.click(w, 40.0f, 210.0f); // the input line, at the bottom
                st.type(w, "help");
                st.key(w, Key::Return);
            });
    victim<ConsoleWidget>("ConsoleWidget.entryActivated by a press on a line of the log",
            [](Stage& st) {
                ConsoleWidget* w = put<ConsoleWidget>(st, 10.0f, 10.0f, 440.0f, 220.0f);
                w->log(LogLevel::Info, "first line");
                w->log(LogLevel::Warn, "second line");
                st.h.frames(2);
                return w;
            },
            [](ConsoleWidget* w, auto kill, auto) { w->entryActivated.connect(kill); },
            [](Stage& st, ConsoleWidget* w) { st.click(w, 30.0f, 44.0f); });

    victim<SymbolOutline>("SymbolOutline.activated by a press on a row",
            [](Stage& st) {
                CodeEditor* ed = put<CodeEditor>(st, 10.0f, 10.0f, 220.0f, 220.0f);
                ed->setHighlighter<PythonHighlighter>();
                ed->setText("class A:\n    def f(self):\n        pass\n\ndef g():\n    return 1\n");
                SymbolOutline* w = put<SymbolOutline>(st, 240.0f, 10.0f, 220.0f, 220.0f);
                w->setEditor(ed);
                w->refresh();
                st.h.frames(2);
                return w;
            },
            [](SymbolOutline* w, auto kill, auto) { w->activated.connect(kill); },
            [](Stage& st, SymbolOutline* w) { st.click(w, 60.0f, 12.0f); });
}

void testSignalsOfDialogs()
{
    auto choice = [](Stage& st) {
        ChoiceDialog* w = st.app.addFloat<ChoiceDialog>("Title", "Message");
        w->addChoice("One");
        w->addChoice("Two");
        w->setFloatSize(300.0f, 140.0f);
        w->setFloatPos(40.0f, 40.0f);
        st.h.frames(2);
        return w;
    };
    victim<ChoiceDialog>("ChoiceDialog.chosen by choose()", choice,
            [](ChoiceDialog* w, auto kill, auto) { w->chosen.connect(kill); },
            [](Stage& st, ChoiceDialog* w) { st.act([w]() { w->choose(1); }); });
    auto input = [](Stage& st) {
        InputBox* w = st.app.addFloat<InputBox>("Title", "Prompt");
        w->setFloatSize(300.0f, 140.0f);
        w->setFloatPos(40.0f, 40.0f);
        w->setText("name");
        st.h.frames(2);
        return w;
    };
    victim<InputBox>("InputBox.accepted by Return", input,
            [](InputBox* w, auto kill, auto) { w->accepted.connect(kill); },
            [](Stage& st, InputBox* w) { st.key(w, Key::Return); });
    victim<InputBox>("InputBox.cancelled by Escape", input,
            [](InputBox* w, auto kill, auto) { w->cancelled.connect(kill); },
            [](Stage& st, InputBox* w) { st.key(w, Key::Escape); });
    victim<MessageBox>("MessageBox.result by Return",
            [](Stage& st) {
                MessageBox* w = st.app.addFloat<MessageBox>("Title", "Message", MessageBox::Ok);
                w->setFloatSize(300.0f, 140.0f);
                w->setFloatPos(40.0f, 40.0f);
                st.h.frames(2);
                return w;
            },
            [](MessageBox* w, auto kill, auto) { w->result.connect(kill); },
            [](Stage& st, MessageBox* w) { st.key(w, Key::Return); });
}

// ---- the widgets of the generic cases ----

typedef Widget* (*Maker)(Widget* parent);
struct Kind
{
    const char* name;
    Maker make;
};

template<class W>
Widget* plain(Widget* p)
{
    return p->createChild<W>();
}
Widget* mkLabel(Widget* p) { return p->createChild<Label>("A label"); }
Widget* mkButton(Widget* p) { return p->createChild<Button>("A button"); }
Widget* mkCheck(Widget* p) { return p->createChild<CheckBox>("A check box"); }
Widget* mkInput(Widget* p) { return p->createChild<TextInput>("some text"); }
Widget* mkEdit(Widget* p) { return p->createChild<TextEdit>("line one\nline two\n\nline four"); }
Widget* mkList(Widget* p)
{
    ListBox* l = p->createChild<ListBox>();
    for (int i = 0; i < 40; ++i) l->addItem(String("item ") + String::number(i));
    return l;
}
Widget* mkTree(Widget* p)
{
    TreeView* t = p->createChild<TreeView>();
    for (int i = 0; i < 20; ++i)
    {
        TreeNode* n = t->addRoot(String("node ") + String::number(i));
        for (int k = 0; k < 5; ++k) n->addChild(String("child ") + String::number(k));
    }
    return t;
}
Widget* mkCombo(Widget* p)
{
    ComboBox* c = p->createChild<ComboBox>();
    c->addItem("one");
    c->addItem("two");
    c->addItem("three");
    return c;
}
Widget* mkCollapsible(Widget* p)
{
    Collapsible* c = p->createChild<Collapsible>(String("Group"), true);
    c->createChild<Label>("inside");
    return c;
}
Widget* mkBox(Widget* p)
{
    BoxLayout* b = p->createChild<BoxLayout>(LayoutDir::Vertical);
    b->createChild<Label>("a");
    b->createChild<Button>("b");
    b->createChild<CheckBox>("c");
    return b;
}
Widget* mkGrid(Widget* p)
{
    GridLayout* g = p->createChild<GridLayout>(3);
    for (int i = 0; i < 7; ++i) g->createChild<Button>(String("b") + String::number(i));
    return g;
}
Widget* mkFlow(Widget* p)
{
    FlowLayout* g = p->createChild<FlowLayout>();
    for (int i = 0; i < 9; ++i) g->createChild<Button>(String("flow ") + String::number(i));
    return g;
}
Widget* mkScroll(Widget* p)
{
    ScrollView* s = p->createChild<ScrollView>();
    s->createChild<Label>("scrolled content");
    return s;
}
Widget* mkProps(Widget* p) { return p->createChild<PropertyGrid>(); }
Widget* mkConsole(Widget* p)
{
    ConsoleWidget* c = p->createChild<ConsoleWidget>();
    c->log(LogLevel::Info, "hello");
    c->log(LogLevel::Error, "an error");
    return c;
}

const Kind kKinds[] = {
    { "Label", mkLabel }, { "Button", mkButton }, { "CheckBox", mkCheck }, { "TextInput", mkInput }, { "TextEdit", mkEdit },
    { "ListBox", mkList }, { "TreeView", mkTree }, { "ComboBox", mkCombo }, { "Collapsible", mkCollapsible }, { "BoxLayout", mkBox },
    { "GridLayout", mkGrid }, { "FlowLayout", mkFlow }, { "ScrollView", mkScroll }, { "PropertyGrid", mkProps }, { "ConsoleWidget", mkConsole },
    { "Slider", plain<Slider> }, { "ProgressBar", plain<ProgressBar> }, { "SpinSlider", plain<SpinSlider> }, { "CodeEditor", plain<CodeEditor> },
    { "Timeline", plain<Timeline> }, { "GradientEditor", plain<GradientEditor> }, { "CurveEditor", plain<CurveEditor> },
};
const int kKindCount = static_cast<int>(sizeof kKinds / sizeof kKinds[0]);

// A run of frames with the pointer going over the widget and a click on it.
void touch(Harness& h, const Widget* w)
{
    const Rect r = w->absoluteRect();
    float x = r.x + r.w * 0.5f, y = r.y + r.h * 0.5f;
    if (!(x == x) || !(y == y) || x > 1e6f || y > 1e6f || x < -1e6f || y < -1e6f)
    {
        x = 10.0f;
        y = 10.0f;
    }
    h.moveTo(x, y);
    h.click(x, y);
    h.wheel(-1.0f);
    h.key(Key::Down);
    h.frames(2);
}

// ---- 2. zero-size and huge-size rectangles ----

void testSizes()
{
    if (!wanted("sizes of every widget")) return;
    ++victimState().cases;
    static const float sizes[][2] = { { 0.0f, 0.0f }, { 0.0f, 200.0f }, { 200.0f, 0.0f }, { 1.0f, 1.0f }, { 0.001f, 0.001f }, { 3.0f, 3000.0f },
        { 1e7f, 1e7f }, { 1e30f, 1e30f }, { 3.0e38f, 3.0e38f }, { -5.0f, -5.0f } };
    for (int k = 0; k < kKindCount; ++k)
    {
        for (const auto& sz : sizes)
        {
            Harness h(640.0f, 480.0f);
            Widget* w = kKinds[k].make(h.root());
            w->setRect(Rect(10.0f, 10.0f, sz[0], sz[1]));
            h.relayout();
            h.frames(2);
            touch(h, w);
            // the same inside a layout that gives it the size
            h.resize(640.0f, 480.0f);
            check(true, "survived");
            (void)kKinds[k].name;
        }
    }
    // the window itself: zero, tiny and huge
    static const float screens[][2] = { { 0.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 480.0f }, { 640.0f, 0.0f }, { 100000.0f, 100000.0f } };
    for (const auto& sc : screens)
    {
        Harness h(640.0f, 480.0f);
        for (int k = 0; k < kKindCount; ++k) kKinds[k].make(h.root())->setRect(Rect(10.0f, 10.0f, 200.0f, 100.0f));
        h.relayout();
        h.resize(sc[0], sc[1]);
        h.frames(2);
        h.moveTo(5.0f, 5.0f);
        h.resize(640.0f, 480.0f);
        check(true, "survived a window of a strange size");
    }
}

// ---- 3. nothing, and a million ----

void testItems()
{
    if (!wanted("no items and a million items")) return;
    ++victimState().cases;
    {
        Harness h(640.0f, 480.0f);
        ListBox* l = h.root()->createChild<ListBox>();
        l->setRect(Rect(0.0f, 0.0f, 300.0f, 300.0f));
        TreeView* t = h.root()->createChild<TreeView>();
        t->setRect(Rect(310.0f, 0.0f, 300.0f, 300.0f));
        ComboBox* c = h.root()->createChild<ComboBox>();
        c->setRect(Rect(0.0f, 310.0f, 200.0f, 28.0f));
        h.relayout();
        h.frames(3);
        for (int i = 0; i < 3; ++i)
        {
            touch(h, l);
            touch(h, t);
            touch(h, c);
        }
        check(l->itemCount() == 0 && l->selectedIndex() == -1, "an empty list has no selection");
        l->setSelectedIndex(0);
        l->setSelectedIndex(-1);
        l->setSelectedIndex(1000000);
        l->removeItem(0);
        l->clearItems();
        check(l->selectedIndex() == -1, "selecting what is not there selects nothing");
        c->openDropdown();
        h.frames(3);
        h.app.closePopup();
        h.frames(3);
    }
    {
        Harness h(640.0f, 480.0f);
        ListBox* l = h.root()->createChild<ListBox>();
        l->setRect(Rect(0.0f, 0.0f, 300.0f, 400.0f));
        const int n = 1000000;
        char buf[32];
        for (int i = 0; i < n; ++i)
        {
            snprintf(buf, sizeof buf, "item %d", i);
            l->addItem(String(buf));
        }
        h.relayout();
        h.frames(3);
        check(l->itemCount() == n, "a million items");
        l->setSelectedIndex(n - 1);
        h.frames(2);
        touch(h, l);
        for (int i = 0; i < 20; ++i) h.wheel(-30.0f);
        h.key(Key::End);
        h.key(Key::Home);
        h.key(Key::PageDown);
        l->removeItem(n / 2);
        h.frames(2);
        check(l->itemCount() == n - 1, "one item taken out of a million");
        l->clearItems();
        h.frames(2);
        check(l->itemCount() == 0, "a million items cleared");
    }
    {
        Harness h(640.0f, 480.0f);
        TreeView* t = h.root()->createChild<TreeView>();
        t->setRect(Rect(0.0f, 0.0f, 400.0f, 400.0f));
        char buf[32];
        for (int g = 0; g < 1000; ++g)
        {
            snprintf(buf, sizeof buf, "group %d", g);
            TreeNode* n = t->addRoot(String(buf));
            for (int k = 0; k < 999; ++k)
            {
                snprintf(buf, sizeof buf, "node %d.%d", g, k);
                n->addChild(String(buf));
            }
        }
        h.relayout();
        h.frames(2);
        touch(h, t);
        for (int i = 0; i < 5; ++i) h.wheel(-30.0f);
        for (TreeNode* n : t->roots()) n->setExpanded(false);
        h.frames(2);
        for (TreeNode* n : t->roots()) n->setExpanded(true);
        h.frames(2);
        t->clear();
        h.frames(2);
        check(t->roots().size() == 0, "a million tree nodes cleared");
    }
}

// ---- 4. strings ----

// the strings that every text-bearing widget gets
struct Str
{
    const char* name;
    String text;
};

ct::Vector<Str> hostileStrings()
{
    ct::Vector<Str> v;
    v.push_back({ "empty", String("") });
    v.push_back({ "lone continuation bytes", String("a\x80\x81\xbf z") });
    v.push_back({ "truncated two-byte", String("ab\xc3") });
    v.push_back({ "truncated three-byte", String("ab\xe2\x82") });
    v.push_back({ "truncated four-byte", String("ab\xf0\x9f\x98") });
    v.push_back({ "surrogate", String("ab\xed\xa0\x80\xed\xbf\xbf z") });
    v.push_back({ "overlong", String("\xc0\x80\xe0\x80\x80 z") });
    v.push_back({ "beyond U+10FFFF", String("\xf4\x90\x80\x80\xf7\xbf\xbf\xbf z") });
    v.push_back({ "0xFF 0xFE", String("\xff\xfe\xff") });
    v.push_back({ "tabs, controls and an embedded NUL", String("a\tb\x01\x7f\x1b[0m\r\n\r", 12) });
    String big;
    for (int i = 0; i < 65536; ++i) big += static_cast<char>('a' + i % 26);
    v.push_back({ "64 KB of letters", big });
    String sp;
    for (int i = 0; i < 65536 / 4; ++i) sp += "ab c";
    v.push_back({ "64 KB of short words", sp });
    String bad;
    for (int i = 0; i < 65536; ++i) bad += static_cast<char>(0x80 + (i * 7) % 128);
    v.push_back({ "64 KB of invalid bytes", bad });
    String multi;
    for (int i = 0; i < 65536 / 3; ++i) multi += "\xe2\x82\xac";
    v.push_back({ "64 KB of euro signs", multi });
    String lines;
    for (int i = 0; i < 65536 / 8; ++i) lines += "abcdefg\n";
    v.push_back({ "64 KB in 8192 lines", lines });
    return v;
}

void settleAndTouch(Harness& h, Widget* w)
{
    h.relayout();
    h.frames(2);
    touch(h, w);
    h.resize(300.0f, 200.0f); // a narrow window wraps what wraps
    h.resize(640.0f, 480.0f);
}

void testStrings()
{
    if (!wanted("hostile strings in text widgets")) return;
    ++victimState().cases;
    const ct::Vector<Str> strs = hostileStrings();
    const bool verbose = getenv("ROUND8_VERBOSE") != nullptr;
    for (const Str& s : strs)
    {
        if (getenv("ROUND8_STRING") && !strstr(s.name, getenv("ROUND8_STRING"))) continue; // (one string, to look at it alone)
        if (getenv("ROUND8_VERBOSE")) fprintf(stderr, "  string: %s\n", s.name);
        {
            Harness h(640.0f, 480.0f);
            Label* l = h.root()->createChild<Label>(s.text);
            l->setRect(Rect(10.0f, 10.0f, 300.0f, 40.0f));
            Button* b = h.root()->createChild<Button>(s.text);
            b->setRect(Rect(10.0f, 60.0f, 200.0f, 30.0f));
            CheckBox* c = h.root()->createChild<CheckBox>(s.text);
            c->setRect(Rect(10.0f, 100.0f, 200.0f, 30.0f));
            b->setTooltip(s.text);
            l->setText(s.text);
            h.relayout();
            touch(h, b);
            h.io().deltaTime = 1.0f;
            h.moveTo(20.0f, 70.0f);
            h.frames(4); // a tooltip
            settleAndTouch(h, l);
            check(true, "Label, Button, CheckBox and a tooltip took the string");
            if (verbose) fprintf(stderr, "    labels done\n");
        }
        {
            Harness h(640.0f, 480.0f);
            TextInput* t = h.root()->createChild<TextInput>(s.text);
            t->setRect(Rect(10.0f, 10.0f, 300.0f, 30.0f));
            TextEdit* e = h.root()->createChild<TextEdit>(s.text);
            e->setRect(Rect(10.0f, 60.0f, 300.0f, 200.0f));
            h.relayout();
            settleAndTouch(h, t);
            h.clickWidget(t);
            h.key(Key::End);
            h.type("x");
            h.key(Key::Backspace);
            h.key('a', false, true); // select all
            h.key(Key::Delete);
            t->setText(s.text);
            e->setText(s.text);
            h.clickWidget(e);
            h.key('a', false, true);
            h.type(String(s.text.c_str(), s.text.size() < 200 ? s.text.size() : 200).c_str()); // (typed: the first 200 bytes; the rest went in with setText)
            h.key(Key::End, false, true);
            h.key(Key::Return);
            h.key(Key::Up);
            h.key('z', false, true);
            h.key('y', false, true);
            settleAndTouch(h, e);
            check(e->lineCount() >= 1, "TextEdit has a line");
            if (verbose) fprintf(stderr, "    text input and edit done\n");
        }
        {
            Harness h(640.0f, 480.0f);
            CodeEditor* e = h.root()->createChild<CodeEditor>();
            e->setRect(Rect(10.0f, 10.0f, 400.0f, 300.0f));
            e->setText(s.text);
            if (verbose) fprintf(stderr, "    code editor: set\n");
            h.relayout();
            if (verbose) fprintf(stderr, "    code editor: laid out\n");
            settleAndTouch(h, e);
            if (verbose) fprintf(stderr, "    code editor: touched\n");
            h.clickWidget(e);
            h.type("a");
            if (verbose) fprintf(stderr, "    code editor: typed\n");
            h.key(Key::Return);
            if (verbose) fprintf(stderr, "    code editor: Return\n");
            h.key('z', false, true);
            if (verbose) fprintf(stderr, "    code editor: undone\n");
            h.frames(2);
            check(true, "CodeEditor took the string");
            if (verbose) fprintf(stderr, "    code editor done\n");
        }
        {
            Harness h(640.0f, 480.0f);
            ListBox* l = h.root()->createChild<ListBox>();
            l->setRect(Rect(10.0f, 10.0f, 200.0f, 200.0f));
            l->addItem(s.text);
            l->addItem(String(""));
            ComboBox* c = h.root()->createChild<ComboBox>();
            c->setRect(Rect(220.0f, 10.0f, 200.0f, 28.0f));
            c->addItem(s.text);
            TreeView* t = h.root()->createChild<TreeView>();
            t->setRect(Rect(10.0f, 220.0f, 200.0f, 200.0f));
            TreeNode* n = t->addRoot(s.text);
            n->addChild(s.text);
            Collapsible* co = h.root()->createChild<Collapsible>(s.text, true);
            co->setRect(Rect(220.0f, 60.0f, 200.0f, 100.0f));
            h.relayout();
            touch(h, l);
            touch(h, c);
            c->openDropdown();
            h.frames(3);
            h.app.closePopup();
            touch(h, t);
            touch(h, co);
            ConsoleWidget* cw = h.root()->createChild<ConsoleWidget>();
            cw->setRect(Rect(430.0f, 10.0f, 200.0f, 300.0f));
            cw->log(LogLevel::Info, s.text);
            cw->log(LogLevel::Error, s.text);
            h.relayout();
            touch(h, cw);
            cw->setSearchText(s.text);
            h.frames(2);
            check(true, "ListBox, ComboBox, TreeView, Collapsible and ConsoleWidget took the string");
            if (verbose) fprintf(stderr, "    lists and console done\n");
        }
        {
            Harness h(640.0f, 480.0f);
            FloatWindow* fw = h.app.addFloat<FloatWindow>(s.text);
            fw->setContent<Label>(s.text);
            fw->setFloatSize(240.0f, 120.0f);
            fw->setFloatPos(30.0f, 30.0f);
            h.relayout();
            h.frames(2);
            touch(h, fw);
            fw->setTitle(s.text);
            fw->setMinimized(true);
            h.frames(2);
            fw->setMinimized(false);
            h.frames(2);
            check(true, "FloatWindow took the string");
        }
    }
}

// ---- 5. shown, hidden, taken out and put back ----

void testShowHide()
{
    if (!wanted("shown, hidden, taken out and put back")) return;
    ++victimState().cases;
    Harness h(640.0f, 480.0f);
    Rng rng(7u);
    Stage* unused = nullptr;
    (void)unused;
    ct::Vector<Widget*> holders;
    for (int i = 0; i < 4; ++i)
    {
        Widget* p = h.root()->createChild<Widget>();
        p->setRect(Rect(10.0f + 150.0f * static_cast<float>(i % 2), 10.0f + 200.0f * static_cast<float>(i / 2), 140.0f, 190.0f));
        holders.push_back(p);
    }
    h.app.addStage("other");
    h.relayout();
    for (int step = 0; step < 600; ++step)
    {
        Widget* parent = holders[static_cast<size_t>(rng.below(4))];
        const int op = rng.below(7);
        if (op <= 1 || parent->children().size() == 0)
        {
            Widget* w = kKinds[rng.below(kKindCount)].make(parent);
            w->setRect(Rect(rng.range(0.0f, 60.0f), rng.range(0.0f, 100.0f), rng.range(0.0f, 140.0f), rng.range(0.0f, 190.0f)));
        }
        else if (op == 2)
        {
            Widget* w = parent->children()[static_cast<size_t>(rng.below(static_cast<int>(parent->children().size())))];
            w->setVisible(!w->isVisible());
        }
        else if (op == 3)
        {
            // taken out and put back somewhere else: removed here, made again there
            Widget* w = parent->children()[static_cast<size_t>(rng.below(static_cast<int>(parent->children().size())))];
            parent->removeChild(w);
            Widget* to = holders[static_cast<size_t>(rng.below(4))];
            kKinds[rng.below(kKindCount)].make(to)->setRect(Rect(5.0f, 5.0f, 100.0f, 60.0f));
        }
        else if (op == 4)
        {
            Widget* w = parent->children()[static_cast<size_t>(rng.below(static_cast<int>(parent->children().size())))];
            parent->moveChild(w, rng.below(static_cast<int>(parent->children().size())));
        }
        else if (op == 5)
        {
            h.moveTo(rng.range(0.0f, 640.0f), rng.range(0.0f, 480.0f));
            if (rng.chance(40)) h.click(rng.range(0.0f, 640.0f), rng.range(0.0f, 480.0f));
        }
        else
        {
            for (Widget* c : parent->children()) c->setVisible(false);
            h.frames(1);
            for (Widget* c : parent->children()) c->setVisible(true);
        }
        if (step % 50 == 49)
        {
            h.app.setStage("other", TransitionType::None);
            h.frames(2);
            h.app.setStage("harness", TransitionType::None);
        }
        h.relayout();
    }
    check(true, "survived 600 random steps");
}

// ---- 6. the pixel scale ----

void testScales()
{
    if (!wanted("pixel scale changed at run time")) return;
    ++victimState().cases;
    Harness h(800.0f, 600.0f);
    for (int k = 0; k < kKindCount; ++k)
    {
        Widget* w = kKinds[k].make(h.root());
        w->setRect(Rect(10.0f + 130.0f * static_cast<float>(k % 6), 10.0f + 130.0f * static_cast<float>(k / 6), 120.0f, 120.0f));
    }
    h.relayout();
    static const float scales[] = { 0.5f, 1.0f, 2.0f, 3.0f, 1.0f, 3.0f, 0.5f, 2.0f };
    for (const float s : scales)
    {
        h.atlas().setPixelScale(s);
        h.io().displayWidth = 800.0f / s;
        h.io().displayHeight = 600.0f / s;
        h.app.requestLayout();
        h.frames(3);
        h.moveTo(100.0f / s, 100.0f / s);
        h.click(200.0f / s, 150.0f / s);
        h.wheel(-2.0f);
        h.type("a");
        check(h.atlas().pixelScale() > 0.0f, "the atlas keeps a positive scale");
    }
    h.atlas().setPixelScale(1.0f);
    h.io().displayWidth = 800.0f;
    h.io().displayHeight = 600.0f;
    h.frames(2);
}

} // namespace

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--list") == 0) victimState().list = true;
        else if (strncmp(argv[i], "--only=", 7) == 0)
            victimState().only = argv[i] + 7;
    }
    testSignalsOfEditors();
    testSignalsOfSpinSlider();
    testSignalsOfTimeline();
    testSignalsOfPropertyGrid();
    testSignalsOfConsoleAndOutline();
    testSignalsOfDialogs();
    testSizes();
    testItems();
    testStrings();
    testShowHide();
    testScales();
    if (victimState().list) return 0;
    shutdown();
    if (failures() != 0)
    {
        printf("%d FAILED\n", failures());
        return 1;
    }
    printf("ok (%d cases)\n", victimState().cases);
    return 0;
}
