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
template<class W, class Make, class Hook, class Drive>
void victim(const char* name, Make make, Hook hook, Drive drive, int witnesses = 0, const int* events = nullptr)
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
        const bool ok = calls >= 1 && (witnesses == 0 || late >= 1) && (!events || *events > e0);
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
        hook(w, [&calls, &st, w, parent](auto&&...) {
                    if (++calls == 1) parent->removeChild(w);
                }, [&late](auto&&...) { ++late; });
        drive(st, w);
        st.h.frames(3);
        const bool out = st.app.hasGone(w, seen);
        const bool ok = calls == 1 && out && late == 0 && (!events || *events == e0);
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
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else if (!gList)
        printf("ok (%d cases)\n", gCases);
    return failures() ? 1 : 0;
}
