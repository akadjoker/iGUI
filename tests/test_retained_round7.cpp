// Round 7 (docs/ROUND7.md): a handler that takes its own widget out of the tree (parent->removeChild(widget)) inside the signal
// of EVERY widget that emits one, not only the four of round 6 (tests/test_retained_round6.cpp). Headless: the real frame loop
// (tests/RetainedHarness.hpp), the synthetic input of WidgetApp where a mouse or a key is the way to the signal and a call of the
// widget's API where it is a setter. No standard library: ct and C only.
//
//   igui_retained_round7_tests [--list] [--only=TEXT]
//
// --list prints the names of the cases, --only=TEXT runs the cases whose name has TEXT in it (under AddressSanitizer the
// program stops at the first widget that reads what it freed: one run for each case says which widgets do).
//
// What one case is. A widget in a holder, a handler connected to the signal under test, and something that makes the signal
// come: an event (a click, a key, the wheel) or a call of the API (setValue, setSelectedIndex). It runs
//
//   - once for each of two modes as a CONTROL, with a handler that only counts: the signal must have come (a case whose
//     driver does not make the signal come proves nothing), and so must the later signals and the global events the case
//     names as witnesses (what the widget does next, when nothing went wrong);
//   - once for each mode with the handler of the rule, `parent->removeChild(widget)`:
//       mode 0, IN A FRAME: the event goes through the dispatcher of WidgetApp (or the API call is made from a deferred call that
//         update() runs), so the deletion waits for the end of the phase and the memory is still there when the widget goes on;
//       mode 1, OUTSIDE A FRAME: the widget's handler or method is called by the program, so the removal deletes at once, and a
//         widget that touches itself after the signal reads freed memory, which AddressSanitizer stops.
//     The handler ran once, the widget is out of the tree, and none of the witnesses came: a widget that has been taken out
//     emits nothing more (WidgetApp::hasGone).

#include "RetainedHarness.hpp"
#include "StageHarness.hpp"

#include <unistd.h>

#include <igui/widgets/AppWidgets.hpp>
#include <igui/widgets/AssetBrowser.hpp>
#include <igui/widgets/AudioWidgets.hpp>
#include <igui/widgets/AutomotiveWidgets.hpp>
#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/ChartWidget.hpp>
#include <igui/widgets/ChartWidgets.hpp>
#include <igui/widgets/CodeEditor.hpp>
#include <igui/widgets/ComboBox.hpp>
#include <igui/widgets/ConsoleWidget.hpp>
#include <igui/widgets/DataWidgets.hpp>
#include <igui/widgets/DialogWidgets.hpp>
#include <igui/widgets/DockPanel.hpp>
#include <igui/widgets/FileDialog.hpp>
#include <igui/widgets/GizmoWidgets.hpp>
#include <igui/widgets/InputWidgets.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/MenuWidgets.hpp>
#include <igui/widgets/NodeEditor.hpp>
#include <igui/widgets/Range.hpp>
#include <igui/widgets/ScrollWidgets.hpp>
#include <igui/widgets/SideTabBar.hpp>
#include <igui/widgets/SpinSlider.hpp>
#include <igui/widgets/SymbolOutline.hpp>
#include <igui/widgets/TextInputWidgets.hpp>
#include <igui/widgets/Theme.hpp>
#include <igui/widgets/ThumbnailGrid.hpp>
#include <igui/widgets/Timeline.hpp>
#include <igui/widgets/ToolBar.hpp>
#include <igui/widgets/TreePropertyColorWidgets.hpp>
#include <igui/widgets/ViewWidgets.hpp>

using namespace rh;

namespace
{

// ---- the runner ----

const char* gOnly = nullptr;
int gCases = 0;
bool gList = false;

// The global events that a widget fires after its signal ("toggle" for the check box, "change" for the sliders); counted for the
// whole program (the handlers of the application stay registered), and a case that names one compares before and after.
int gToggleEvents = 0;
int gChangeEvents = 0;

bool wanted(const char* name)
{
    if (gList)
    {
        printf("%s\n", name);
        return false;
    }
    return !gOnly || strstr(name, gOnly) != nullptr;
}

// How a case was told about the widget's later signals: a witness is a signal of the widget (or a global event) that comes after
// the one the handler removes the widget in, when nothing removes it.
struct Witness
{
    int late = 0;
};

// Builds the widget in the holder of the stage.
// make(Stage&) -> W*; hook(W*, kill, late) connects kill to the signal under test and late to the later signals (may do neither);
// drive(Stage&, W*) makes the signal come. events: a global event counter that must come in the control and not in the case.
// killOn: the handler takes the widget out the killOn-th time the signal comes (a drag emits at each move, the first one may be
// the press); the widget must not emit again after it.
template<class W, class Make, class Hook, class Drive>
void victim(const char* name, Make make, Hook hook, Drive drive, int witnesses = 0, const int* events = nullptr, int killOn = 1)
{
    if (!wanted(name)) return;
    ++gCases;
    bool driverOk = true;
    for (int mode = 0; mode < 2; ++mode) // the control: the handler only counts
    {
        Stage st(mode);
        W* w = make(st);
        st.watch(w);
        int calls = 0, late = 0;
        const int e0 = events ? *events : 0;
        hook(w, [&calls](auto&&...) { ++calls; }, [&late](auto&&...) { ++late; });
        drive(st, w);
        st.h.frames(3);
        const bool ok = calls >= killOn && (witnesses == 0 || late >= 1) && (!events || *events > e0);
        check(ok, say((String("CONTROL ") + name + " (the signal comes, and what follows it)").c_str(), mode).c_str());
        if (!ok)
        {
            printf("    signal %d times, witnesses %d (%d wanted), global event %d times\n", calls, late, witnesses, events ? *events - e0 : -1);
            driverOk = false;
        }
    }
    if (!driverOk) return;
    for (int mode = 0; mode < 2; ++mode)
    {
        Stage st(mode);
        W* w = make(st);
        st.watch(w);
        const uint32_t seen = st.app.deletions();
        int calls = 0, late = 0;
        const int e0 = events ? *events : 0;
        Widget* parent = w->parent();
        FloatWindow* floating = parent ? nullptr : widget_cast<FloatWindow>(w); // (a float window is not in a parent: the app removes it)
        hook(w, [&calls, &st, w, parent, floating, killOn](auto&&...) {
                    if (++calls != killOn) return;
                    if (floating) st.app.removeFloat(floating);
                    else
                        parent->removeChild(w);
                }, [&late](auto&&...) { ++late; });
        drive(st, w);
        st.h.frames(3);
        const bool out = st.app.hasGone(w, seen);
        const bool ok = calls == killOn && out && late == 0 && (!events || *events == e0);
        check(ok, say(name, mode).c_str());
        if (!ok) printf("    handler ran %d times, out of the tree %d, later signals %d, global events %d\n", calls, out, late, events ? *events - e0 : 0);
    }
}

// A widget of the holder: its rectangle, then a layout pass so the widget has its geometry.
template<class W, class... A>
W* put(Stage& st, float x, float y, float w, float h, A&&... args)
{
    W* widget = st.holder->createChild<W>(static_cast<A&&>(args)...);
    widget->setRect(Rect(x, y, w, h));
    st.h.relayout();
    return widget;
}

// ---- BasicWidgets: CheckBox, RadioButton and RadioGroup, Switch ----

// A radio group for each run of a case (the slots of its signal hold references to the run's locals, so a group of the
// case as a whole would call them in the next run); the buttons of a run are gone before the next run makes its group.
struct GroupFor
{
    RadioGroup* group = nullptr;
    RadioGroup* fresh()
    {
        delete group;
        group = new RadioGroup;
        return group;
    }
    ~GroupFor()
    {
        delete group;
    }
};

void testBasic()
{
    victim<CheckBox>("CheckBox.toggled by a click",
            [](Stage& st) { return put<CheckBox>(st, 20.0f, 20.0f, 160.0f, 28.0f, "check"); },
            [](CheckBox* w, auto kill, auto) { w->toggled.connect(kill); },
            [](Stage& st, CheckBox* w) { st.click(w, 10.0f, 10.0f); }, 0, &gToggleEvents);
    {
        GroupFor g;
        victim<RadioButton>("RadioButton.toggled by a click, in a group",
                [&](Stage& st) { return put<RadioButton>(st, 20.0f, 20.0f, 160.0f, 28.0f, "radio", g.fresh()); },
                [](RadioButton* w, auto kill, auto) { w->toggled.connect(kill); },
                [](Stage& st, RadioButton* w) { st.click(w, 10.0f, 10.0f); });
        victim<RadioButton>("RadioGroup.selectionChanged taking the selected button out",
                [&](Stage& st) { return put<RadioButton>(st, 20.0f, 20.0f, 160.0f, 28.0f, "radio", g.fresh()); },
                [&](RadioButton*, auto kill, auto) { g.group->selectionChanged.connect(kill); },
                [](Stage& st, RadioButton* w) { st.act([w]() { w->setSelected(true); }); });
        victim<RadioButton>("RadioButton.toggled by setSelected(false)",
                [&](Stage& st) {
                    RadioButton* w = put<RadioButton>(st, 20.0f, 20.0f, 160.0f, 28.0f, "radio", g.fresh());
                    w->setSelected(true);
                    return w;
                },
                [](RadioButton* w, auto kill, auto) { w->toggled.connect(kill); },
                [](Stage& st, RadioButton* w) { st.act([w]() { w->setSelected(false); }); });
    }
    victim<RadioButton>("RadioButton.toggled by a click, without a group",
            [](Stage& st) { return put<RadioButton>(st, 20.0f, 20.0f, 160.0f, 28.0f, "radio"); },
            [](RadioButton* w, auto kill, auto) { w->toggled.connect(kill); },
            [](Stage& st, RadioButton* w) { st.click(w, 10.0f, 10.0f); });
    victim<Switch>("Switch.toggled by a click",
            [](Stage& st) { return put<Switch>(st, 20.0f, 20.0f, 160.0f, 28.0f, "switch"); },
            [](Switch* w, auto kill, auto) { w->toggled.connect(kill); },
            [](Stage& st, Switch* w) { st.click(w, 10.0f, 10.0f); });
    victim<Switch>("Switch.toggled by setOn",
            [](Stage& st) { return put<Switch>(st, 20.0f, 20.0f, 160.0f, 28.0f, "switch"); },
            [](Switch* w, auto kill, auto) { w->toggled.connect(kill); },
            [](Stage& st, Switch* w) { st.act([w]() { w->setOn(true); }); });
}

// ---- InputWidgets, ComboBox, ScrollBar ----

void testInput()
{
    victim<SpinBox>("SpinBox.onValueChanged by setValue",
            [](Stage& st) { return put<SpinBox>(st, 20.0f, 20.0f, 160.0f, 28.0f, 0.0f, 100.0f, 50.0f, 1.0f); },
            [](SpinBox* w, auto kill, auto) { w->onValueChanged.connect(kill); },
            [](Stage& st, SpinBox* w) { st.act([w]() { w->setValue(60.0f); }); });
    victim<SpinBox>("SpinBox.onValueChanged by a click on the plus button",
            [](Stage& st) { return put<SpinBox>(st, 20.0f, 20.0f, 160.0f, 28.0f, 0.0f, 100.0f, 50.0f, 1.0f); },
            [](SpinBox* w, auto kill, auto) { w->onValueChanged.connect(kill); },
            [](Stage& st, SpinBox* w) { st.click(w, 150.0f, 14.0f); });
    victim<SpinBox>("SpinBox.onValueChanged by the Up key",
            [](Stage& st) { return put<SpinBox>(st, 20.0f, 20.0f, 160.0f, 28.0f, 0.0f, 100.0f, 50.0f, 1.0f); },
            [](SpinBox* w, auto kill, auto) { w->onValueChanged.connect(kill); },
            [](Stage& st, SpinBox* w) { st.key(w, Key::Up); });
    victim<SpinBox>("SpinBox.onValueChanged by the wheel",
            [](Stage& st) { return put<SpinBox>(st, 20.0f, 20.0f, 160.0f, 28.0f, 0.0f, 100.0f, 50.0f, 1.0f); },
            [](SpinBox* w, auto kill, auto) { w->onValueChanged.connect(kill); },
            [](Stage& st, SpinBox* w) {
                st.app.setFocused(w); // (the wheel acts on a spin box that is hovered or has the focus)
                st.moveTo(w, 60.0f, 14.0f);
                st.wheel(w, 1.0f);
            });
    victim<ProgressBar>("ProgressBar.onValueChanged by setValue",
            [](Stage& st) { return put<ProgressBar>(st, 20.0f, 20.0f, 160.0f, 20.0f, 0.0f, 100.0f, 10.0f); },
            [](ProgressBar* w, auto kill, auto) { w->onValueChanged.connect(kill); },
            [](Stage& st, ProgressBar* w) { st.act([w]() { w->setValue(60.0f); }); }, 0, &gChangeEvents);
    victim<DatePicker>("DatePicker.onDateChanged by setDate",
            [](Stage& st) { return put<DatePicker>(st, 20.0f, 20.0f, 160.0f, 28.0f); },
            [](DatePicker* w, auto kill, auto) { w->onDateChanged.connect(kill); },
            [](Stage& st, DatePicker* w) { st.act([w]() { w->setDate(2031, 5, 17); }); });
    victim<TimePicker>("TimePicker.onTimeChanged by setTime",
            [](Stage& st) { return put<TimePicker>(st, 20.0f, 20.0f, 160.0f, 28.0f); },
            [](TimePicker* w, auto kill, auto) { w->onTimeChanged.connect(kill); },
            [](Stage& st, TimePicker* w) { st.act([w]() { w->setTime(7, 30, 15); }); });
    victim<ScrollBar>("ScrollBar.scrolled by a click on the track",
            [](Stage& st) {
                ScrollBar* w = put<ScrollBar>(st, 20.0f, 20.0f, 12.0f, 200.0f, ScrollBarOrientation::Vertical);
                w->setContentSize(1000.0f);
                w->setViewSize(100.0f);
                return w;
            },
            [](ScrollBar* w, auto kill, auto) { w->scrolled.connect(kill); },
            [](Stage& st, ScrollBar* w) { st.click(w, 6.0f, 150.0f); });
    victim<ScrollBar>("ScrollBar.scrolled by dragging the thumb",
            [](Stage& st) {
                ScrollBar* w = put<ScrollBar>(st, 20.0f, 20.0f, 12.0f, 200.0f, ScrollBarOrientation::Vertical);
                w->setContentSize(1000.0f);
                w->setViewSize(100.0f);
                return w;
            },
            [](ScrollBar* w, auto kill, auto) { w->scrolled.connect(kill); },
            [](Stage& st, ScrollBar* w) {
                st.moveTo(w, 6.0f, 5.0f);
                st.down(w);
                st.drag(w, w->absoluteRect().x + 6.0f, w->absoluteRect().y + 60.0f);
                st.up(w);
            });
    auto combo = [](Stage& st) {
        ComboBox* w = put<ComboBox>(st, 20.0f, 20.0f, 160.0f, 28.0f);
        w->addItem("one");
        w->addItem("two");
        w->addItem("three");
        w->setSelectedIndex(0);
        return w;
    };
    victim<ComboBox>("ComboBox.selectionChanged by setSelectedIndex", combo,
            [](ComboBox* w, auto kill, auto) { w->selectionChanged.connect(kill); },
            [](Stage& st, ComboBox* w) { st.act([w]() { w->setSelectedIndex(2); }); });
    victim<ComboBox>("ComboBox.selectionChanged by removing the selected item", combo,
            [](ComboBox* w, auto kill, auto) { w->selectionChanged.connect(kill); },
            [](Stage& st, ComboBox* w) { st.act([w]() { w->removeItem(0); }); });
    victim<ComboBox>("ComboBox.selectionChanged by a click on its list", combo,
            [](ComboBox* w, auto kill, auto) { w->selectionChanged.connect(kill); },
            [](Stage& st, ComboBox* w) {
                st.click(w, 10.0f, 10.0f); // opens the list
                st.h.frames(2);
                Widget* list = st.app.popup();
                if (!list) return;
                st.moveTo(list, 10.0f, Theme::instance().inputHeight * 1.5f);
                st.down(list);
                st.up(list);
            });
}

// ---- LayoutWidgets, SideTabBar, ToolBar, DockPanel ----

void testLayout()
{
    victim<Collapsible>("Collapsible.expandedChanged by setExpanded",
            [](Stage& st) { return put<Collapsible>(st, 20.0f, 20.0f, 200.0f, 100.0f, "title", true); },
            [](Collapsible* w, auto kill, auto) { w->expandedChanged.connect(kill); },
            [](Stage& st, Collapsible* w) { st.act([w]() { w->setExpanded(false); }); });
    victim<Collapsible>("Collapsible.expandedChanged by a click on the header",
            [](Stage& st) { return put<Collapsible>(st, 20.0f, 20.0f, 200.0f, 100.0f, "title", true); },
            [](Collapsible* w, auto kill, auto) { w->expandedChanged.connect(kill); },
            [](Stage& st, Collapsible* w) { st.click(w, 100.0f, 8.0f); });
    victim<Collapsible>("Collapsible.checkedChanged by a click on the check box",
            [](Stage& st) {
                Collapsible* w = put<Collapsible>(st, 20.0f, 20.0f, 200.0f, 100.0f, "title", true);
                w->setCheckable(true);
                return w;
            },
            [](Collapsible* w, auto kill, auto) { w->checkedChanged.connect(kill); },
            [](Stage& st, Collapsible* w) { st.click(w, 22.0f + Theme::instance().fontSize * 0.5f, 10.0f); });
    victim<StackLayout>("StackLayout.currentChanged by setCurrentIndex",
            [](Stage& st) {
                StackLayout* w = put<StackLayout>(st, 20.0f, 20.0f, 200.0f, 100.0f);
                w->createChild<Label>("a");
                w->createChild<Label>("b");
                return w;
            },
            [](StackLayout* w, auto kill, auto) { w->currentChanged.connect(kill); },
            [](Stage& st, StackLayout* w) { st.act([w]() { w->setCurrentIndex(1); }); });
    auto splitter = [](Stage& st) {
        Splitter* w = put<Splitter>(st, 20.0f, 20.0f, 300.0f, 100.0f, LayoutDir::Horizontal);
        w->createChild<Label>("a");
        w->createChild<Label>("b");
        st.h.relayout();
        return w;
    };
    victim<Splitter>("Splitter.ratioChanged by setRatio", splitter,
            [](Splitter* w, auto kill, auto) { w->ratioChanged.connect(kill); },
            [](Stage& st, Splitter* w) { st.act([w]() { w->setRatio(0.3f); }); });
    victim<Splitter>("Splitter.ratioChanged by dragging the handle", splitter,
            [](Splitter* w, auto kill, auto) { w->ratioChanged.connect(kill); },
            [](Stage& st, Splitter* w) {
                const Rect r = w->absoluteRect();
                st.move(r.x + 150.0f, r.y + 50.0f);
                st.down(w);
                st.drag(w, r.x + 200.0f, r.y + 50.0f);
                st.up(w);
            });
    auto tabs = [](Stage& st) {
        TabLayout* w = put<TabLayout>(st, 20.0f, 20.0f, 300.0f, 150.0f);
        w->addTabWidget("One", new Label("a"));
        w->addTabWidget("Two", new Label("b"));
        w->addTabWidget("Three", new Label("c"));
        w->setTabsClosable(true);
        st.h.relayout();
        return w;
    };
    victim<TabLayout>("TabLayout.currentChanged by setCurrentIndex", tabs,
            [](TabLayout* w, auto kill, auto) { w->currentChanged.connect(kill); },
            [](Stage& st, TabLayout* w) { st.act([w]() { w->setCurrentIndex(1); }); });
    victim<TabLayout>("TabLayout.currentChanged by removing the current tab", tabs,
            [](TabLayout* w, auto kill, auto) { w->currentChanged.connect(kill); },
            [](Stage& st, TabLayout* w) { st.act([w]() { w->removeTab(0); }); });
    victim<TabLayout>("TabLayout.currentChanged by a click on a tab", tabs,
            [](TabLayout* w, auto kill, auto) { w->currentChanged.connect(kill); },
            [](Stage& st, TabLayout* w) { st.click(w, 90.0f, 14.0f); });
    victim<TabLayout>("TabLayout.tabClosed by a click on the close button", tabs,
            [](TabLayout* w, auto kill, auto) { w->tabClosed.connect(kill); },
            [](Stage& st, TabLayout* w) {
                const float tw = Math::max2(60.0f, 3.0f * Theme::instance().fontSize * 0.55f + Theme::instance().padding * 2.0f + 18.0f);
                st.click(w, 2.0f + tw - 11.0f, 14.0f);
            });
    auto carousel = [](Stage& st) {
        Carousel* w = put<Carousel>(st, 20.0f, 20.0f, 200.0f, 100.0f);
        w->addPage<Label>("a");
        w->addPage<Label>("b");
        w->addPage<Label>("c");
        st.h.relayout();
        return w;
    };
    victim<Carousel>("Carousel.pageChanged by next", carousel,
            [](Carousel* w, auto kill, auto) { w->pageChanged.connect(kill); },
            [](Stage& st, Carousel* w) { st.act([w]() { w->next(); }); });
    victim<Carousel>("Carousel.pageChanged by setCurrentPage", carousel,
            [](Carousel* w, auto kill, auto) { w->pageChanged.connect(kill); },
            [](Stage& st, Carousel* w) { st.act([w]() { w->setCurrentPage(2); }); });
    victim<SlidePanel>("SlidePanel.openChanged by open",
            [](Stage& st) { return put<SlidePanel>(st, 20.0f, 20.0f, 260.0f, 200.0f); },
            [](SlidePanel* w, auto kill, auto) { w->openChanged.connect(kill); },
            [](Stage& st, SlidePanel* w) { st.act([w]() { w->open(); }); });
    victim<SlidePanel>("SlidePanel.openChanged by close",
            [](Stage& st) {
                SlidePanel* w = put<SlidePanel>(st, 20.0f, 20.0f, 260.0f, 200.0f);
                w->open();
                return w;
            },
            [](SlidePanel* w, auto kill, auto) { w->openChanged.connect(kill); },
            [](Stage& st, SlidePanel* w) { st.act([w]() { w->close(); }); });
}

void testTabsToolsDock()
{
    auto side = [](Stage& st) {
        SideTabBar* w = put<SideTabBar>(st, 0.0f, 0.0f, 480.0f, 260.0f);
        w->addPage<Label>(IconId::Folder, "One", "a");
        w->addPage<Label>(IconId::File, "Two", "b");
        st.h.relayout();
        st.h.frames(2);
        return w;
    };
    victim<SideTabBar>("SideTabBar.currentChanged by setCurrentIndex", side,
            [](SideTabBar* w, auto kill, auto) { w->currentChanged.connect(kill); },
            [](Stage& st, SideTabBar* w) { st.act([w]() { w->setCurrentIndex(1); }); });
    victim<SideTabBar>("SideTabBar.currentChanged by removing the current page", side,
            [](SideTabBar* w, auto kill, auto) { w->currentChanged.connect(kill); },
            [](Stage& st, SideTabBar* w) { st.act([w]() { w->removePage(0); }); });
    victim<SideTabBar>("SideTabBar.buttonClicked by a click on a rail button", side,
            [](SideTabBar* w, auto kill, auto) { w->buttonClicked.connect(kill); },
            [](Stage& st, SideTabBar* w) {
                Rect r;
                if (!w->buttonRect(1, r)) return;
                st.move(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
                st.down(w);
                st.up(w);
            });
    auto toolbar = [](Stage& st) {
        ToolBar* w = put<ToolBar>(st, 20.0f, 20.0f, 300.0f, 36.0f);
        w->addButton(IconId::Folder, "open");
        w->addToggle(IconId::File, "toggle", false);
        st.h.relayout();
        st.h.frames(2);
        return w;
    };
    auto clickItem = [](Stage& st, ToolBar* w, int id) {
        Rect r;
        if (!w->itemRect(id, r)) return;
        st.move(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
        st.down(w);
        st.up(w);
    };
    victim<ToolBar>("ToolBar.buttonClicked by a click on a button", toolbar,
            [](ToolBar* w, auto kill, auto) { w->buttonClicked.connect(kill); },
            [&](Stage& st, ToolBar* w) { clickItem(st, w, w->itemIdAt(0)); });
    victim<ToolBar>("ToolBar.toggleChanged by a click on a toggle", toolbar,
            [](ToolBar* w, auto kill, auto) { w->toggleChanged.connect(kill); },
            [&](Stage& st, ToolBar* w) { clickItem(st, w, w->itemIdAt(1)); });
    auto dock = [](Stage& st) {
        DockPanel* w = put<DockPanel>(st, 0.0f, 0.0f, 480.0f, 260.0f);
        w->addPanel<Label>("A", "a");
        w->addPanel<Label>("B", "b");
        st.h.relayout();
        st.h.frames(2);
        return w;
    };
    victim<DockPanel>("DockPanel.panelActivated by showPanel", dock,
            [](DockPanel* w, auto kill, auto) { w->panelActivated.connect(kill); },
            [](Stage& st, DockPanel* w) { st.act([w]() { w->showPanel("A"); }); });
    victim<DockPanel>("DockPanel.panelActivated by a click on another tab",
            [&](Stage& st) {
                DockPanel* w = dock(st);
                w->showPanel("A");
                st.h.frames(2);
                return w;
            },
            [](DockPanel* w, auto kill, auto) { w->panelActivated.connect(kill); },
            [](Stage& st, DockPanel* w) { st.click(w, 70.0f, 10.0f); });
    victim<DockPanel>("DockPanel.panelClosed by closePanel", dock,
            [](DockPanel* w, auto kill, auto) { w->panelClosed.connect(kill); },
            [](Stage& st, DockPanel* w) { st.act([w]() { w->closePanel("B"); }); });
    victim<DockPanel>("DockPanel.panelClosed by a middle click on a tab", dock,
            [](DockPanel* w, auto kill, auto) { w->panelClosed.connect(kill); },
            [](Stage& st, DockPanel* w) { st.click(w, 20.0f, 10.0f, 2); });
    victim<DockPanel>("DockPanel.tabMenuRequested by a right click on a tab", dock,
            [](DockPanel* w, auto kill, auto) { w->tabMenuRequested.connect(kill); },
            [](Stage& st, DockPanel* w) { st.click(w, 30.0f, 10.0f, 1); });
}

// ---- TextInput and TextEdit ----

void testText()
{
    victim<TextInput>("TextInput.textChanged by setText",
            [](Stage& st) { return put<TextInput>(st, 20.0f, 20.0f, 200.0f, 28.0f, "abc"); },
            [](TextInput* w, auto kill, auto) { w->textChanged.connect(kill); },
            [](Stage& st, TextInput* w) { st.act([w]() { w->setText("xyz"); }); });
    victim<TextInput>("TextInput.textChanged by typing",
            [](Stage& st) { return put<TextInput>(st, 20.0f, 20.0f, 200.0f, 28.0f, "abc"); },
            [](TextInput* w, auto kill, auto) { w->textChanged.connect(kill); },
            [](Stage& st, TextInput* w) { st.type(w, "d"); });
    victim<TextInput>("TextInput.textChanged by Backspace",
            [](Stage& st) { return put<TextInput>(st, 20.0f, 20.0f, 200.0f, 28.0f, "abc"); },
            [](TextInput* w, auto kill, auto) { w->textChanged.connect(kill); },
            [](Stage& st, TextInput* w) { st.key(w, Key::Backspace); });
    victim<TextInput>("TextInput.textChanged by Delete",
            [](Stage& st) {
                TextInput* w = put<TextInput>(st, 20.0f, 20.0f, 200.0f, 28.0f, "abc");
                w->setCursorPos(0);
                return w;
            },
            [](TextInput* w, auto kill, auto) { w->textChanged.connect(kill); },
            [](Stage& st, TextInput* w) { st.key(w, Key::Delete); });
    victim<TextInput>("TextInput.textChanged by cutting the selection",
            [](Stage& st) {
                TextInput* w = put<TextInput>(st, 20.0f, 20.0f, 200.0f, 28.0f, "abc");
                w->selectAll();
                return w;
            },
            [](TextInput* w, auto kill, auto) { w->textChanged.connect(kill); },
            [](Stage& st, TextInput* w) { st.key(w, Key::X, false, true); });
    victim<TextInput>("TextInput.submitted by Return",
            [](Stage& st) { return put<TextInput>(st, 20.0f, 20.0f, 200.0f, 28.0f, "abc"); },
            [](TextInput* w, auto kill, auto) { w->submitted.connect(kill); },
            [](Stage& st, TextInput* w) { st.key(w, Key::Return); });
    auto edit = [](Stage& st) {
        TextEdit* w = put<TextEdit>(st, 20.0f, 20.0f, 300.0f, 120.0f, "one\ntwo\nthree");
        st.h.frames(2);
        return w;
    };
    victim<TextEdit>("TextEdit.textChanged by typing", edit,
            [](TextEdit* w, auto kill, auto late) { w->textChanged.connect(kill); w->cursorMoved.connect(late); },
            [](Stage& st, TextEdit* w) { st.type(w, "d"); }, 1);
    victim<TextEdit>("TextEdit.textChanged by Return", edit,
            [](TextEdit* w, auto kill, auto late) { w->textChanged.connect(kill); w->cursorMoved.connect(late); },
            [](Stage& st, TextEdit* w) { st.key(w, Key::Return); }, 1);
    victim<TextEdit>("TextEdit.textChanged by Backspace",
            [&](Stage& st) {
                TextEdit* w = edit(st);
                w->setCursorPos(1, 2);
                return w;
            },
            [](TextEdit* w, auto kill, auto late) { w->textChanged.connect(kill); w->cursorMoved.connect(late); },
            [](Stage& st, TextEdit* w) { st.key(w, Key::Backspace); }, 1);
    victim<TextEdit>("TextEdit.textChanged by Delete",
            [&](Stage& st) {
                TextEdit* w = edit(st);
                w->setCursorPos(0, 1);
                return w;
            },
            [](TextEdit* w, auto kill, auto late) { w->textChanged.connect(kill); w->cursorMoved.connect(late); },
            [](Stage& st, TextEdit* w) { st.key(w, Key::Delete); }, 1);
    victim<TextEdit>("TextEdit.textChanged by cutting the selection",
            [&](Stage& st) {
                TextEdit* w = edit(st);
                w->selectAll();
                return w;
            },
            [](TextEdit* w, auto kill, auto late) { w->textChanged.connect(kill); w->cursorMoved.connect(late); },
            [](Stage& st, TextEdit* w) { st.key(w, Key::X, false, true); }, 1);
    victim<TextEdit>("TextEdit.cursorMoved by the Left key", edit,
            [](TextEdit* w, auto kill, auto) { w->cursorMoved.connect(kill); },
            [](Stage& st, TextEdit* w) { st.key(w, Key::Left); });
    victim<TextEdit>("TextEdit.cursorMoved by setCursorPos", edit,
            [](TextEdit* w, auto kill, auto) { w->cursorMoved.connect(kill); },
            [](Stage& st, TextEdit* w) { st.act([w]() { w->setCursorPos(2, 1); }); });
}

// ---- AudioWidgets (the piano roll is the round 6's) and AutomotiveWidgets ----

void testAudio()
{
    victim<Knob>("Knob.onChanged by setValue",
            [](Stage& st) { return put<Knob>(st, 20.0f, 20.0f, 64.0f, 64.0f); },
            [](Knob* w, auto kill, auto) { w->onChanged.connect(kill); },
            [](Stage& st, Knob* w) { st.act([w]() { w->setValue(0.8f); }); });
    victim<Knob>("Knob.onChanged by dragging",
            [](Stage& st) { return put<Knob>(st, 20.0f, 20.0f, 64.0f, 64.0f); },
            [](Knob* w, auto kill, auto) { w->onChanged.connect(kill); },
            [](Stage& st, Knob* w) {
                const Rect r = w->absoluteRect();
                st.move(r.x + 32.0f, r.y + 32.0f);
                st.down(w);
                st.drag(w, r.x + 32.0f, r.y + 2.0f);
                st.up(w);
            });
    victim<Knob>("Knob.onChanged by the wheel",
            [](Stage& st) { return put<Knob>(st, 20.0f, 20.0f, 64.0f, 64.0f); },
            [](Knob* w, auto kill, auto) { w->onChanged.connect(kill); },
            [](Stage& st, Knob* w) {
                st.moveTo(w, 32.0f, 32.0f);
                st.wheel(w, 1.0f);
            });
    victim<Fader>("Fader.onChanged by setValue",
            [](Stage& st) { return put<Fader>(st, 20.0f, 20.0f, 40.0f, 160.0f); },
            [](Fader* w, auto kill, auto) { w->onChanged.connect(kill); },
            [](Stage& st, Fader* w) { st.act([w]() { w->setValue(0.3f); }); });
    victim<Fader>("Fader.onChanged by a press on the track and a drag",
            [](Stage& st) { return put<Fader>(st, 20.0f, 20.0f, 40.0f, 160.0f); },
            [](Fader* w, auto kill, auto) { w->onChanged.connect(kill); },
            [](Stage& st, Fader* w) {
                const Rect r = w->absoluteRect();
                st.move(r.x + 20.0f, r.y + 100.0f);
                st.down(w);
                st.drag(w, r.x + 20.0f, r.y + 40.0f);
                st.up(w);
            });
    victim<LinearKnob>("LinearKnob.onChanged by setValue",
            [](Stage& st) { return put<LinearKnob>(st, 20.0f, 20.0f, 160.0f, 24.0f); },
            [](LinearKnob* w, auto kill, auto) { w->onChanged.connect(kill); },
            [](Stage& st, LinearKnob* w) { st.act([w]() { w->setValue(0.3f); }); });
    victim<LinearKnob>("LinearKnob.onChanged by a press on the track",
            [](Stage& st) { return put<LinearKnob>(st, 20.0f, 20.0f, 160.0f, 24.0f); },
            [](LinearKnob* w, auto kill, auto) { w->onChanged.connect(kill); },
            [](Stage& st, LinearKnob* w) {
                st.moveTo(w, 120.0f, 12.0f);
                st.down(w);
                st.up(w);
            });
    victim<ADSRWidget>("ADSRWidget.onChanged by dragging the attack handle",
            [](Stage& st) {
                ADSRWidget* w = put<ADSRWidget>(st, 20.0f, 20.0f, 300.0f, 120.0f);
                w->setADSR(1.0f, 1.0f, 0.5f, 1.0f);
                return w;
            },
            [](ADSRWidget* w, auto kill, auto) { w->onChanged.connect(kill); },
            [](Stage& st, ADSRWidget* w) {
                const Rect r = w->absoluteRect();
                st.move(r.x + 8.0f + 66.27f, r.y + 8.0f + 1.0f);
                st.down(w);
                st.drag(w, r.x + 8.0f + 100.0f, r.y + 20.0f);
                st.up(w);
            });
    victim<WaveformView>("WaveformView.onScrub by a press",
            [](Stage& st) { return put<WaveformView>(st, 20.0f, 20.0f, 300.0f, 100.0f); },
            [](WaveformView* w, auto kill, auto) { w->onScrub.connect(kill); },
            [](Stage& st, WaveformView* w) {
                st.moveTo(w, 100.0f, 50.0f);
                st.down(w);
                st.up(w);
            });
    victim<WaveformView>("WaveformView.onScrub by a drag (the second signal: the press scrubs too)",
            [](Stage& st) { return put<WaveformView>(st, 20.0f, 20.0f, 300.0f, 100.0f); },
            [](WaveformView* w, auto kill, auto) { w->onScrub.connect(kill); },
            [](Stage& st, WaveformView* w) {
                const Rect r = w->absoluteRect();
                st.move(r.x + 100.0f, r.y + 50.0f);
                st.down(w);
                st.drag(w, r.x + 120.0f, r.y + 50.0f);
                st.up(w);
            }, 0, nullptr, 2);
}

void testAutomotive()
{
    victim<RadialGauge>("RadialGauge.onValueChanged by setValue",
            [](Stage& st) { return put<RadialGauge>(st, 20.0f, 20.0f, 160.0f, 160.0f); },
            [](RadialGauge* w, auto kill, auto) { w->onValueChanged.connect(kill); },
            [](Stage& st, RadialGauge* w) { st.act([w]() { w->setValue(120.0f); }); });
    victim<PowerBar>("PowerBar.onValueChanged by setValue",
            [](Stage& st) { return put<PowerBar>(st, 20.0f, 20.0f, 160.0f, 24.0f); },
            [](PowerBar* w, auto kill, auto) { w->onValueChanged.connect(kill); },
            [](Stage& st, PowerBar* w) { st.act([w]() { w->setValue(50.0f); }); });
    victim<DriveMode>("DriveMode.onModeChanged by setMode",
            [](Stage& st) {
                DriveMode* w = put<DriveMode>(st, 20.0f, 20.0f, 200.0f, 28.0f);
                w->addMode("Eco", Color(60, 200, 80, 255));
                w->addMode("Sport", Color(220, 60, 60, 255));
                return w;
            },
            [](DriveMode* w, auto kill, auto) { w->onModeChanged.connect(kill); },
            [](Stage& st, DriveMode* w) { st.act([w]() { w->setMode(1); }); });
    victim<DigitalSpeed>("DigitalSpeed.onValueChanged by setValue",
            [](Stage& st) { return put<DigitalSpeed>(st, 20.0f, 20.0f, 160.0f, 60.0f); },
            [](DigitalSpeed* w, auto kill, auto) { w->onValueChanged.connect(kill); },
            [](Stage& st, DigitalSpeed* w) { st.act([w]() { w->setValue(88.0f); }); });
    victim<BatteryGauge>("BatteryGauge.onPercentChanged by setPercent",
            [](Stage& st) { return put<BatteryGauge>(st, 20.0f, 20.0f, 80.0f, 24.0f); },
            [](BatteryGauge* w, auto kill, auto) { w->onPercentChanged.connect(kill); },
            [](Stage& st, BatteryGauge* w) { st.act([w]() { w->setPercent(40.0f); }); });
}

// ---- ViewWidgets, AppWidgets, GizmoWidgets ----

void testViews()
{
    auto floatWindow = [](Stage& st) {
        FloatWindow* w = st.app.addFloat<FloatWindow>("window");
        w->setFloatPos(40.0f, 40.0f);
        w->setFloatSize(200.0f, 120.0f);
        w->setClosable(true);
        w->setMinimizable(true);
        st.h.frames(2);
        return w;
    };
    victim<FloatWindow>("FloatWindow.minimizedChanged by setMinimized", floatWindow,
            [](FloatWindow* w, auto kill, auto) { w->minimizedChanged.connect(kill); },
            [](Stage& st, FloatWindow* w) { st.act([w]() { w->setMinimized(true); }); });
    victim<FloatWindow>("FloatWindow.minimizedChanged by a click on the minimize button", floatWindow,
            [](FloatWindow* w, auto kill, auto) { w->minimizedChanged.connect(kill); },
            [](Stage& st, FloatWindow* w) {
                const float tb = w->titleBarHeight(); // (the window is at 40, 40 and 200 wide; the close button is the last one in the bar)
                st.move(40.0f + 200.0f - tb * 1.5f, 40.0f + tb * 0.5f);
                st.down(w);
                st.up(w);
            });
    victim<FloatWindow>("FloatWindow.closed by a click on the close button", floatWindow,
            [](FloatWindow* w, auto kill, auto) { w->closed.connect(kill); },
            [](Stage& st, FloatWindow* w) {
                const float tb = w->titleBarHeight();
                st.move(40.0f + 200.0f - tb * 0.5f, 40.0f + tb * 0.5f);
                st.down(w);
                st.up(w);
            });
    victim<PageView>("PageView.pageChanged by setPage",
            [](Stage& st) {
                PageView* w = put<PageView>(st, 20.0f, 20.0f, 200.0f, 100.0f);
                w->addPage<Label>("a");
                w->addPage<Label>("b");
                st.h.relayout();
                return w;
            },
            [](PageView* w, auto kill, auto) { w->pageChanged.connect(kill); },
            [](Stage& st, PageView* w) { st.act([w]() { w->setPage(1); }); });
    victim<VideoCanvas>("VideoCanvas.viewChanged by setViewTransform",
            [](Stage& st) { return put<VideoCanvas>(st, 20.0f, 20.0f, 300.0f, 200.0f); },
            [](VideoCanvas* w, auto kill, auto) { w->viewChanged.connect(kill); },
            [](Stage& st, VideoCanvas* w) { st.act([w]() { w->setViewTransform(2.0f, 10.0f, 10.0f); }); });
}

void testApp()
{
    victim<Breadcrumbs>("Breadcrumbs.itemClicked by a click on a segment",
            [](Stage& st) {
                Breadcrumbs* w = put<Breadcrumbs>(st, 20.0f, 20.0f, 300.0f, 28.0f);
                ct::Vector<String> path;
                path.push_back("home");
                path.push_back("music");
                path.push_back("album");
                w->setPath(path);
                st.h.frames(2);
                return w;
            },
            [](Breadcrumbs* w, auto kill, auto) { w->itemClicked.connect(kill); },
            [](Stage& st, Breadcrumbs* w) { st.click(w, 10.0f, 14.0f); });
    auto search = [](Stage& st) {
        SearchBar* w = put<SearchBar>(st, 20.0f, 20.0f, 240.0f, 28.0f);
        w->setText("abc");
        return w;
    };
    victim<SearchBar>("SearchBar.onTextChanged by typing", search,
            [](SearchBar* w, auto kill, auto) { w->onTextChanged.connect(kill); },
            [](Stage& st, SearchBar* w) { st.type(w, "d"); });
    victim<SearchBar>("SearchBar.onTextChanged by Backspace", search,
            [](SearchBar* w, auto kill, auto) { w->onTextChanged.connect(kill); },
            [](Stage& st, SearchBar* w) { st.key(w, Key::Backspace); });
    victim<SearchBar>("SearchBar.onSearch by Return", search,
            [](SearchBar* w, auto kill, auto) { w->onSearch.connect(kill); },
            [](Stage& st, SearchBar* w) { st.key(w, Key::Return); });
    victim<SplitButton>("SplitButton.clicked by a click on the main button",
            [](Stage& st) { return put<SplitButton>(st, 20.0f, 20.0f, 160.0f, 28.0f, "Go"); },
            [](SplitButton* w, auto kill, auto) { w->clicked.connect(kill); },
            [](Stage& st, SplitButton* w) { st.click(w, 20.0f, 14.0f); });
    auto outliner = [](Stage& st) {
        Outliner* w = put<Outliner>(st, 20.0f, 20.0f, 300.0f, 200.0f);
        TreeNode* root = w->addRoot("scene");
        root->addChild("child");
        w->addRoot("other");
        st.h.relayout();
        st.h.frames(2);
        return w;
    };
    victim<Outliner>("Outliner.selectionChanged by a click on a row", outliner,
            [](Outliner* w, auto kill, auto late) { w->selectionChanged.connect(kill); w->nodeActivated.connect(late); },
            [](Stage& st, Outliner* w) {
                st.moveTo(w, 80.0f, 11.0f);
                MouseEvent unused;
                (void) unused;
                st.down(w);
                st.up(w);
            });
    victim<Outliner>("Outliner.visibilityChanged by a click on the eye", outliner,
            [](Outliner* w, auto kill, auto) { w->visibilityChanged.connect(kill); },
            [](Stage& st, Outliner* w) { st.click(w, 300.0f - 40.0f + 4.0f, 11.0f); });
    victim<Outliner>("Outliner.lockChanged by a click on the lock", outliner,
            [](Outliner* w, auto kill, auto) { w->lockChanged.connect(kill); },
            [](Stage& st, Outliner* w) { st.click(w, 300.0f - 20.0f + 4.0f, 11.0f); });
    victim<RichText>("RichText.linkClicked by a click on a link",
            [](Stage& st) {
                RichText* w = put<RichText>(st, 20.0f, 20.0f, 300.0f, 100.0f);
                w->setMarkdown("[a link](https://example.org/x) and text");
                st.h.relayout();
                st.h.frames(2);
                return w;
            },
            [](RichText* w, auto kill, auto) { w->linkClicked.connect(kill); },
            [](Stage& st, RichText* w) { st.click(w, 14.0f, 10.0f); });
    victim<Viewport3D>("Viewport3D.onResize by the first paint",
            [](Stage& st) { return put<Viewport3D>(st, 20.0f, 20.0f, 300.0f, 200.0f); },
            [](Viewport3D* w, auto kill, auto) { w->onResize.connect(kill); },
            [](Stage& st, Viewport3D* w) { w->setRect(Rect(20.0f, 20.0f, 320.0f, 210.0f)); st.h.frames(2); });
    auto viewport = [](Stage& st) {
        Viewport3D* w = put<Viewport3D>(st, 20.0f, 20.0f, 300.0f, 200.0f);
        st.h.frames(2);
        return w;
    };
    victim<Viewport3D>("Viewport3D.onOrbit by a drag", viewport,
            [](Viewport3D* w, auto kill, auto) { w->onOrbit.connect(kill); },
            [](Stage& st, Viewport3D* w) {
                const Rect r = w->absoluteRect();
                st.move(r.x + 100.0f, r.y + 100.0f);
                st.down(w, 1);
                st.drag(w, r.x + 140.0f, r.y + 110.0f);
                st.up(w, 1);
            });
    victim<Viewport3D>("Viewport3D.onPan by a right drag", viewport,
            [](Viewport3D* w, auto kill, auto) { w->onPan.connect(kill); },
            [](Stage& st, Viewport3D* w) {
                const Rect r = w->absoluteRect();
                st.move(r.x + 100.0f, r.y + 100.0f);
                st.down(w, 2);
                st.drag(w, r.x + 140.0f, r.y + 110.0f);
                st.up(w, 2);
            });
    victim<Viewport3D>("Viewport3D.onZoom by the wheel", viewport,
            [](Viewport3D* w, auto kill, auto) { w->onZoom.connect(kill); },
            [](Stage& st, Viewport3D* w) {
                st.moveTo(w, 100.0f, 100.0f);
                st.wheel(w, 1.0f);
            });
    auto gizmo = [](Stage& st) {
        Gizmo2D* w = put<Gizmo2D>(st, 0.0f, 0.0f, 300.0f, 200.0f);
        w->setPosition(100.0f, 100.0f);
        st.h.frames(2);
        return w;
    };
    victim<Gizmo2D>("Gizmo2D.onDragStart by a press on the handle", gizmo,
            [](Gizmo2D* w, auto kill, auto) { w->onDragStart.connect(kill); },
            [](Stage& st, Gizmo2D* w) {
                st.move(100.0f, 100.0f);
                st.down(w);
                st.up(w);
            });
    victim<Gizmo2D>("Gizmo2D.onTranslate by a drag", gizmo,
            [](Gizmo2D* w, auto kill, auto) { w->onTranslate.connect(kill); },
            [](Stage& st, Gizmo2D* w) {
                st.move(100.0f, 100.0f);
                st.down(w);
                st.drag(w, 130.0f, 110.0f);
                st.up(w);
            });
    victim<Gizmo2D>("Gizmo2D.onDragEnd by the release", gizmo,
            [](Gizmo2D* w, auto kill, auto) { w->onDragEnd.connect(kill); },
            [](Stage& st, Gizmo2D* w) {
                st.move(100.0f, 100.0f);
                st.down(w);
                st.up(w);
            });
}

// ---- Range (SpinSlider), ThumbnailGrid, AssetBrowser, NodeEditor, Chart, Menu ----

ct::Vector<String> labelsOf(const char* a, const char* b, const char* c)
{
    ct::Vector<String> v;
    v.push_back(a);
    v.push_back(b);
    v.push_back(c);
    return v;
}

ct::Vector<float> valuesOf(float a, float b, float c)
{
    ct::Vector<float> v;
    v.push_back(a);
    v.push_back(b);
    v.push_back(c);
    return v;
}

void testRangeGridsEditors()
{
    auto spin = [](Stage& st) {
        SpinSlider* w = put<SpinSlider>(st, 20.0f, 20.0f, 160.0f, 28.0f);
        w->setMin(0.0);
        w->setMax(100.0);
        w->setValue(50.0);
        return w;
    };
    victim<SpinSlider>("Range.valueChanged by setValue", spin,
            [](SpinSlider* w, auto kill, auto) { w->valueChanged.connect(kill); },
            [](Stage& st, SpinSlider* w) { st.act([w]() { w->setValue(60.0); }); });
    victim<SpinSlider>("Range.valueChanged by setMax below the value", spin,
            [](SpinSlider* w, auto kill, auto late) { w->valueChanged.connect(kill); w->changed.connect(late); },
            [](Stage& st, SpinSlider* w) { st.act([w]() { w->setMax(20.0); }); }, 1);
    victim<SpinSlider>("Range.changed by setStep", spin,
            [](SpinSlider* w, auto kill, auto) { w->changed.connect(kill); },
            [](Stage& st, SpinSlider* w) { st.act([w]() { w->setStep(5.0); }); });
    victim<SpinSlider>("Range.changed by setPage", spin,
            [](SpinSlider* w, auto kill, auto) { w->changed.connect(kill); },
            [](Stage& st, SpinSlider* w) { st.act([w]() { w->setPage(10.0); }); });
    auto grid = [](Stage& st) {
        ThumbnailGrid* w = put<ThumbnailGrid>(st, 20.0f, 20.0f, 300.0f, 200.0f);
        w->addItem("one");
        w->addItem("two");
        w->addItem("three");
        st.h.frames(2);
        return w;
    };
    victim<ThumbnailGrid>("ThumbnailGrid.selectionChanged by setSelected", grid,
            [](ThumbnailGrid* w, auto kill, auto) { w->selectionChanged.connect(kill); },
            [](Stage& st, ThumbnailGrid* w) { st.act([w]() { w->setSelected(1); }); });
    victim<ThumbnailGrid>("ThumbnailGrid.selectionChanged by a click on an item", grid,
            [](ThumbnailGrid* w, auto kill, auto late) { w->selectionChanged.connect(kill); w->onItemClicked.connect(late); },
            [](Stage& st, ThumbnailGrid* w) { st.click(w, 20.0f, 20.0f); }, 1);
    victim<ThumbnailGrid>("ThumbnailGrid.onItemClicked by a click on an item", grid,
            [](ThumbnailGrid* w, auto kill, auto late) { w->onItemClicked.connect(kill); w->onItemDoubleClicked.connect(late); },
            [](Stage& st, ThumbnailGrid* w) {
                st.moveTo(w, 20.0f, 20.0f);
                MouseEvent dbl = st.event(w, 0);
                dbl.clickCount = 2;
                st.down(w);
                if (st.mode == 1 && !st.gone()) w->onMousePress(dbl); // the second press of a double click
                st.up(w);
            });
    victim<ThumbnailGrid>("ThumbnailGrid.onItemContextMenu by a right click on an item", grid,
            [](ThumbnailGrid* w, auto kill, auto) { w->onItemContextMenu.connect(kill); },
            [](Stage& st, ThumbnailGrid* w) { st.click(w, 20.0f, 20.0f, 1); });
    auto assets = [](Stage& st) {
        AssetBrowser* w = put<AssetBrowser>(st, 20.0f, 20.0f, 300.0f, 200.0f);
        AssetItem a;
        a.name = "clip";
        w->addItem(a);
        AssetItem b;
        b.name = "other";
        w->addItem(b);
        st.h.frames(2);
        return w;
    };
    victim<AssetBrowser>("AssetBrowser.onSelect by a click on an item", assets,
            [](AssetBrowser* w, auto kill, auto) { w->onSelect.connect(kill); },
            [](Stage& st, AssetBrowser* w) { st.click(w, 30.0f, 50.0f); });
    victim<AssetBrowser>("AssetBrowser.onOpen by a second click on the same item", assets,
            [](AssetBrowser* w, auto kill, auto) { w->onOpen.connect(kill); },
            [](Stage& st, AssetBrowser* w) {
                st.click(w, 30.0f, 50.0f);
                st.click(w, 30.0f, 50.0f);
            });
    auto nodes = [](Stage& st) {
        NodeEditor* w = put<NodeEditor>(st, 0.0f, 0.0f, 480.0f, 260.0f);
        w->addNode("Add", 60.0f, 60.0f);
        w->addNode("Mul", 260.0f, 60.0f);
        st.h.frames(2);
        return w;
    };
    victim<NodeEditor>("NodeEditor.onNodeSelected by a click on a node", nodes,
            [](NodeEditor* w, auto kill, auto) { w->onNodeSelected.connect(kill); },
            [](Stage& st, NodeEditor* w) {
                st.moveTo(w, 120.0f, 90.0f);
                st.down(w);
                st.up(w);
            });
    victim<Chart>("Chart.hoverChanged by moving onto a bar",
            [](Stage& st) {
                Chart* w = put<Chart>(st, 20.0f, 20.0f, 400.0f, 220.0f);
                w->bars(labelsOf("a", "b", "c"), valuesOf(1.0f, 2.0f, 3.0f));
                st.h.frames(2);
                return w;
            },
            [](Chart* w, auto kill, auto) { w->hoverChanged.connect(kill); },
            [](Stage& st, Chart* w) {
                const Rect r = w->barRect(0, 2);
                st.move(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
                st.drag(w, r.x + r.w * 0.5f, r.y + r.h * 0.5f);
            });
    {
        // a menu is no widget in a parent: its owner deletes it, from the handler of aboutToShow too (before it is shown)
        for (int control = 1; control >= 0; --control)
        {
            if (!wanted("Menu.aboutToShow deleting the menu")) break;
            ++gCases;
            Harness h(300.0f, 200.0f, false);
            Menu* menu = new Menu();
            menu->addAction("Go");
            int shown = 0;
            menu->aboutToShow.connect([&]() {
                ++shown;
                if (!control)
                {
                    delete menu;
                    menu = nullptr;
                }
            });
            menu->exec(10.0f, 10.0f);
            h.frames(3);
            check(shown == 1, (String("Menu.aboutToShow deleting the menu") + (control ? " (control)" : "")).c_str());
            delete menu;
        }
    }
}

// ---- TreeView, PropertyGrid, ColorPicker and ColorButton ----

void testTreeProperty()
{
    // a tree whose first root is open (a node is open when it is made) or closed
    auto openTree = [](Stage& st) {
        TreeView* w = put<TreeView>(st, 20.0f, 20.0f, 300.0f, 200.0f);
        TreeNode* root = w->addRoot("root");
        root->addChild("a");
        root->addChild("b");
        w->addRoot("other");
        st.h.relayout();
        st.h.frames(2);
        return w;
    };
    auto tree = [&](Stage& st) {
        TreeView* w = openTree(st);
        w->roots()[0]->setExpanded(false);
        st.h.frames(2);
        return w;
    };
    auto rowCentre = [](Stage& st, TreeView* w, TreeNode* n, float dx) {
        Rect r;
        if (!w->rowRect(n, r)) return;
        st.move(r.x + dx, r.y + r.h * 0.5f);
    };
    victim<TreeView>("TreeView.selectionChanged by setSelectedNode", tree,
            [](TreeView* w, auto kill, auto) { w->selectionChanged.connect(kill); },
            [](Stage& st, TreeView* w) { st.act([w]() { w->setSelectedNode(w->roots()[1]); }); });
    victim<TreeView>("TreeView.selectionChanged by a click on a row", tree,
            [](TreeView* w, auto kill, auto late) { w->selectionChanged.connect(kill); w->nodeActivated.connect(late); },
            [&](Stage& st, TreeView* w) {
                rowCentre(st, w, w->roots()[1], 80.0f);
                st.down(w);
                st.up(w);
            });
    victim<TreeView>("TreeView.nodeActivated by a double click", tree,
            [](TreeView* w, auto kill, auto) { w->nodeActivated.connect(kill); },
            [&](Stage& st, TreeView* w) {
                w->setSelectedNode(w->roots()[1]);
                rowCentre(st, w, w->roots()[1], 80.0f);
                MouseEvent dbl = st.event(w, 0);
                dbl.clickCount = 2;
                if (st.mode == 1) w->onMousePress(dbl);
                else
                {
                    st.app.feedMouseButton(0, true);
                    st.app.feedMouseButton(0, false);
                    st.app.feedMouseButton(0, true);
                }
            });
    victim<TreeView>("TreeView.nodeExpanded by a click on the triangle", tree,
            [](TreeView* w, auto kill, auto) { w->nodeExpanded.connect(kill); },
            [&](Stage& st, TreeView* w) {
                rowCentre(st, w, w->roots()[0], 8.0f);
                st.down(w);
                st.up(w);
            });
    victim<TreeView>("TreeView.nodeCollapsed by a click on the triangle", openTree,
            [](TreeView* w, auto kill, auto) { w->nodeCollapsed.connect(kill); },
            [&](Stage& st, TreeView* w) {
                rowCentre(st, w, w->roots()[0], 8.0f);
                st.down(w);
                st.up(w);
            });
    victim<TreeView>("TreeView.nodeExpanded by the Right key",
            [&](Stage& st) {
                TreeView* w = tree(st);
                w->setSelectedNode(w->roots()[0]);
                st.h.frames(2);
                return w;
            },
            [](TreeView* w, auto kill, auto) { w->nodeExpanded.connect(kill); },
            [](Stage& st, TreeView* w) { st.key(w, Key::Right); });
    victim<TreeView>("TreeView.nodeCollapsed by the Left key",
            [&](Stage& st) {
                TreeView* w = openTree(st);
                w->setSelectedNode(w->roots()[0]);
                st.h.frames(2);
                return w;
            },
            [](TreeView* w, auto kill, auto) { w->nodeCollapsed.connect(kill); },
            [](Stage& st, TreeView* w) { st.key(w, Key::Left); });
    victim<TreeView>("TreeView.selectionChanged by the Down key",
            [&](Stage& st) {
                TreeView* w = openTree(st);
                w->setSelectedNode(w->roots()[0]);
                st.h.frames(2);
                return w;
            },
            [](TreeView* w, auto kill, auto) { w->selectionChanged.connect(kill); },
            [](Stage& st, TreeView* w) { st.key(w, Key::Down); });
    victim<TreeView>("TreeView.selectionSetChanged by a ctrl click in a multi selection",
            [&](Stage& st) {
                TreeView* w = openTree(st);
                w->setMultiSelect(true);
                w->setSelectedNode(w->roots()[0]);
                st.h.frames(2);
                return w;
            },
            [](TreeView* w, auto kill, auto) { w->selectionSetChanged.connect(kill); },
            [&](Stage& st, TreeView* w) {
                rowCentre(st, w, w->roots()[1], 80.0f);
                MouseEvent e = st.event(w, 0);
                e.ctrl = true;
                if (st.mode == 1) w->onMousePress(e);
                else
                {
                    st.app.feedMouseButton(0, true, false, true);
                    st.app.feedMouseButton(0, false, false, true);
                }
            });
    auto props = [](Stage& st) {
        PropertyGrid* w = put<PropertyGrid>(st, 20.0f, 20.0f, 300.0f, 200.0f);
        w->addBool("visible", false);
        w->addButton("apply", ct::Function<void()>());
        st.h.relayout();
        st.h.frames(2);
        return w;
    };
    victim<PropertyGrid>("PropertyGrid.propertyChanged by a click on a bool", props,
            [](PropertyGrid* w, auto kill, auto) { w->propertyChanged.connect(kill); },
            [](Stage& st, PropertyGrid* w) { st.click(w, 225.0f, 12.0f); });
    if (wanted("PropertyGrid.onChange callback of a bool row taking the grid out"))
    {
        // the callback of a row (a ct::Function the application gave) is a handler too: propertyChanged does not follow the one
        // that took the grid out; the control is the same grid with a callback that does nothing
        ++gCases;
        for (int remove = 0; remove < 2; ++remove)
            for (int mode = 0; mode < 2; ++mode)
            {
                Stage st(mode);
                PropertyGrid* w = put<PropertyGrid>(st, 20.0f, 20.0f, 300.0f, 200.0f);
                int callbacks = 0, signals = 0;
                w->addBool("visible", false, [&, w](bool) {
                    ++callbacks;
                    if (remove) st.holder->removeChild(w);
                });
                w->propertyChanged.connect([&](int) { ++signals; });
                st.h.relayout();
                st.h.frames(2);
                st.watch(w);
                st.click(w, 225.0f, 12.0f);
                st.h.frames(3);
                const bool ok = callbacks == 1 && signals == (remove ? 0 : 1) && st.gone() == (remove != 0);
                check(ok, say(remove ? "PropertyGrid.onChange callback of a bool row taking the grid out" : "CONTROL PropertyGrid.onChange callback of a bool row", mode).c_str());
                if (!ok) printf("    callback %d times, propertyChanged %d times, gone %d\n", callbacks, signals, st.gone());
            }
    }
    victim<ColorPicker>("ColorPicker.colorChanged by a press on the value bar",
            [](Stage& st) {
                ColorPicker* w = put<ColorPicker>(st, 20.0f, 20.0f, 220.0f, 200.0f);
                st.h.frames(2);
                return w;
            },
            [](ColorPicker* w, auto kill, auto) { w->colorChanged.connect(kill); },
            [](Stage& st, ColorPicker* w) { st.click(w, 179.0f, 60.0f); });
    victim<ColorPicker>("ColorPicker.colorChanged by a press on the alpha bar",
            [](Stage& st) {
                ColorPicker* w = put<ColorPicker>(st, 20.0f, 20.0f, 220.0f, 200.0f);
                w->setShowAlpha(true);
                st.h.frames(2);
                return w;
            },
            [](ColorPicker* w, auto kill, auto) { w->colorChanged.connect(kill); },
            [](Stage& st, ColorPicker* w) { st.click(w, 205.0f, 60.0f); });
    victim<ColorPicker>("ColorPicker.colorChanged by a press on the wheel",
            [](Stage& st) {
                ColorPicker* w = put<ColorPicker>(st, 20.0f, 20.0f, 220.0f, 200.0f);
                st.h.frames(2);
                return w;
            },
            [](ColorPicker* w, auto kill, auto) { w->colorChanged.connect(kill); },
            [](Stage& st, ColorPicker* w) {
                st.click(w, 90.0f, 80.0f);
            });
    victim<ColorPicker>("ColorPicker.colorCommitted by a drag",
            [](Stage& st) {
                ColorPicker* w = put<ColorPicker>(st, 20.0f, 20.0f, 220.0f, 200.0f);
                st.h.frames(2);
                return w;
            },
            [](ColorPicker* w, auto kill, auto) { w->colorCommitted.connect(kill); },
            [](Stage& st, ColorPicker* w) {
                const Rect r = w->absoluteRect();
                st.move(r.x + 90.0f, r.y + 80.0f);
                st.down(w);
                st.drag(w, r.x + 60.0f, r.y + 70.0f);
                st.up(w);
            });
}

// ---- DataGrid and TreeGrid ----

ct::Vector<String> rowOf(const char* a, const char* b)
{
    ct::Vector<String> v;
    v.push_back(a);
    v.push_back(b);
    return v;
}

void testGrids()
{
    auto data = [](Stage& st) {
        DataGrid* w = put<DataGrid>(st, 20.0f, 20.0f, 360.0f, 220.0f);
        w->addColumn("name", 160.0f);
        w->addColumn("value", 160.0f);
        w->addRow(rowOf("one", "1"));
        w->addRow(rowOf("two", "2"));
        w->addRow(rowOf("three", "3"));
        w->setShowCheckboxes(true);
        st.h.relayout();
        st.h.frames(2);
        return w;
    };
    victim<DataGrid>("DataGrid.selectionChanged by a click on a row", data,
            [](DataGrid* w, auto kill, auto) { w->selectionChanged.connect(kill); },
            [](Stage& st, DataGrid* w) { st.click(w, 100.0f, 56.0f); });
    victim<DataGrid>("DataGrid.selectionChanged by the Down key",
            [&](Stage& st) {
                DataGrid* w = data(st);
                w->setSelectedRow(0);
                return w;
            },
            [](DataGrid* w, auto kill, auto) { w->selectionChanged.connect(kill); },
            [](Stage& st, DataGrid* w) { st.key(w, Key::Down); });
    victim<DataGrid>("DataGrid.columnSorted by a click on a header", data,
            [](DataGrid* w, auto kill, auto) { w->columnSorted.connect(kill); },
            [](Stage& st, DataGrid* w) { st.click(w, 100.0f, 10.0f); });
    victim<DataGrid>("DataGrid.rowCheckChanged by a click on a check box", data,
            [](DataGrid* w, auto kill, auto) { w->rowCheckChanged.connect(kill); },
            [](Stage& st, DataGrid* w) { st.click(w, 8.0f, 56.0f); });
    victim<DataGrid>("DataGrid.rowCheckChanged by the Space key",
            [&](Stage& st) {
                DataGrid* w = data(st);
                w->setSelectedRow(0);
                return w;
            },
            [](DataGrid* w, auto kill, auto) { w->rowCheckChanged.connect(kill); },
            [](Stage& st, DataGrid* w) { st.key(w, Key::Space); });
    victim<DataGrid>("DataGrid.cellEdited by typing into a cell and Return",
            [&](Stage& st) {
                DataGrid* w = data(st);
                w->setSelectedRow(0);
                return w;
            },
            [](DataGrid* w, auto kill, auto) { w->cellEdited.connect(kill); },
            [](Stage& st, DataGrid* w) {
                st.key(w, Key::Return);
                st.type(w, "x");
                st.key(w, Key::Return);
            });
    auto treeGrid = [](Stage& st) {
        TreeGrid* w = put<TreeGrid>(st, 20.0f, 20.0f, 360.0f, 220.0f);
        w->addColumn("name", 200.0f);
        w->addColumn("value", 120.0f);
        TreeGrid::Node* root = w->addRoot(rowOf("root", "r"));
        root->addChild(rowOf("a", "1"));
        root->addChild(rowOf("b", "2"));
        w->addRoot(rowOf("other", "o"));
        st.h.relayout();
        st.h.frames(2);
        return w;
    };
    victim<TreeGrid>("TreeGrid.selectionChanged by a click on a row", treeGrid,
            [](TreeGrid* w, auto kill, auto) { w->selectionChanged.connect(kill); },
            [](Stage& st, TreeGrid* w) { st.click(w, 100.0f, 56.0f); });
    victim<TreeGrid>("TreeGrid.selectionChanged by the Down key",
            [&](Stage& st) {
                TreeGrid* w = treeGrid(st);
                st.click(w, 100.0f, 40.0f);
                return w;
            },
            [](TreeGrid* w, auto kill, auto) { w->selectionChanged.connect(kill); },
            [](Stage& st, TreeGrid* w) { st.key(w, Key::Down); });
    victim<TreeGrid>("TreeGrid.columnSorted by a click on a header", treeGrid,
            [](TreeGrid* w, auto kill, auto) { w->columnSorted.connect(kill); },
            [](Stage& st, TreeGrid* w) { st.click(w, 100.0f, 10.0f); });
    victim<TreeGrid>("TreeGrid.nodeCollapsed by a click on the arrow", treeGrid,
            [](TreeGrid* w, auto kill, auto) { w->nodeCollapsed.connect(kill); },
            [](Stage& st, TreeGrid* w) { st.click(w, 8.0f, 40.0f); });
    victim<TreeGrid>("TreeGrid.nodeExpanded by the Right key",
            [&](Stage& st) {
                TreeGrid* w = treeGrid(st);
                st.click(w, 100.0f, 40.0f);
                w->roots()[0]->setExpanded(false);
                return w;
            },
            [](TreeGrid* w, auto kill, auto) { w->nodeExpanded.connect(kill); },
            [](Stage& st, TreeGrid* w) { st.key(w, Key::Right); });
    victim<TreeGrid>("TreeGrid.cellEdited by typing into a cell and Return",
            [&](Stage& st) {
                TreeGrid* w = treeGrid(st);
                st.click(w, 100.0f, 40.0f);
                return w;
            },
            [](TreeGrid* w, auto kill, auto) { w->cellEdited.connect(kill); },
            [](Stage& st, TreeGrid* w) {
                st.key(w, Key::Return);
                st.type(w, "x");
                st.key(w, Key::Return);
            });
    victim<TreeGrid>("TreeGrid.nodeCollapsed by the Left key",
            [&](Stage& st) {
                TreeGrid* w = treeGrid(st);
                st.click(w, 100.0f, 40.0f);
                return w;
            },
            [](TreeGrid* w, auto kill, auto) { w->nodeCollapsed.connect(kill); },
            [](Stage& st, TreeGrid* w) { st.key(w, Key::Left); });
}

// ---- Timeline ----

void testTimeline()
{
    auto timeline = [](Stage& st) {
        Timeline* w = put<Timeline>(st, 20.0f, 20.0f, 440.0f, 200.0f);
        const int track = w->addTrack("track");
        w->addClip(track, 1.0f, 9.0f, "clip");
        w->addTrack("other");
        st.h.relayout();
        st.h.frames(2);
        return w;
    };
    victim<Timeline>("Timeline.onTrackMuteChanged by setTrackMuted", timeline,
            [](Timeline* w, auto kill, auto) { w->onTrackMuteChanged.connect(kill); },
            [](Stage& st, Timeline* w) { st.act([w]() { w->setTrackMuted(0, true); }); });
    victim<Timeline>("Timeline.onTrackSelected by setSelectedTrack", timeline,
            [](Timeline* w, auto kill, auto) { w->onTrackSelected.connect(kill); },
            [](Stage& st, Timeline* w) { st.act([w]() { w->setSelectedTrack(1); }); });
    victim<Timeline>("Timeline.onPlayheadChanged by a click on the ruler", timeline,
            [](Timeline* w, auto kill, auto) { w->onPlayheadChanged.connect(kill); },
            [](Stage& st, Timeline* w) { st.click(w, 300.0f, 10.0f); });
    victim<Timeline>("Timeline.onPlayheadChanged by dragging along the ruler (the second signal)", timeline,
            [](Timeline* w, auto kill, auto) { w->onPlayheadChanged.connect(kill); },
            [](Stage& st, Timeline* w) {
                const Rect r = w->absoluteRect();
                st.move(r.x + 300.0f, r.y + 10.0f);
                st.down(w);
                st.drag(w, r.x + 330.0f, r.y + 10.0f);
                st.up(w);
            }, 0, nullptr, 2);
    victim<Timeline>("Timeline.onClipSelected by a click on a clip", timeline,
            [](Timeline* w, auto kill, auto late) { w->onClipSelected.connect(kill); w->onClipGrabbed.connect(late); },
            [](Stage& st, Timeline* w) { st.click(w, 300.0f, 38.0f); });
    victim<Timeline>("Timeline.onClipSelected by a right click on a clip", timeline,
            [](Timeline* w, auto kill, auto late) { w->onClipSelected.connect(kill); w->onClipContextMenu.connect(late); },
            [](Stage& st, Timeline* w) { st.click(w, 300.0f, 38.0f, 1); }, 1);
    victim<Timeline>("Timeline.onClipChanged by dragging a clip",
            timeline,
            [](Timeline* w, auto kill, auto) { w->onClipChanged.connect(kill); },
            [](Stage& st, Timeline* w) {
                const Rect r = w->absoluteRect();
                st.move(r.x + 300.0f, r.y + 38.0f);
                st.down(w);
                st.drag(w, r.x + 330.0f, r.y + 38.0f);
                st.up(w);
            });
    victim<Timeline>("Timeline.onTrackLockChanged by a click on the lock button", timeline,
            [](Timeline* w, auto kill, auto) { w->onTrackLockChanged.connect(kill); },
            [](Stage& st, Timeline* w) {
                // the lock button is the second of the header's two, at its right end
                st.click(w, 160.0f - 14.0f, 24.0f + 14.0f);
            });
}

} // namespace

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--list") == 0) gList = true;
        else if (strncmp(argv[i], "--only=", 7) == 0)
            gOnly = argv[i] + 7;
        else
        {
            printf("usage: %s [--list] [--only=TEXT]\n", argv[0]);
            return 2;
        }
    }
    // the global events a widget fires after its signal, counted
    {
        Harness h(100.0f, 100.0f, false); // a context for the app to exist in
        WidgetApp::instance().onAny("toggle", [](Widget*) { ++gToggleEvents; });
        WidgetApp::instance().onAny("change", [](Widget*) { ++gChangeEvents; });
    }
    testBasic();
    testInput();
    testLayout();
    testTabsToolsDock();
    testText();
    testAudio();
    testAutomotive();
    testViews();
    testApp();
    testRangeGridsEditors();
    testTreeProperty();
    testGrids();
    testTimeline();
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else if (!gList)
        printf("ok (%d cases)\n", gCases);
    return failures() ? 1 : 0;
}
