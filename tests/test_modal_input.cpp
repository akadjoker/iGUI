// Input isolation: while something owns the pointer/keyboard (a message box,
// a dialog, a file dialog, a modal window, an open menu/combo/context menu),
// no widget outside it may react. Every immediate-mode widget is placed
// outside the blocker, then driven with the input that normally changes it.
//
// Each case first runs WITHOUT a blocker and must change its widget - that
// proves the synthetic input really lands on the widget, so a blocked run
// that changes nothing means "blocked", not "missed".

#include "InteractionHarness.hpp"

#include <igui/CodeEditor.hpp>
#include <igui/FileDialog.hpp>

#include <stdio.h>
#include <string.h>

namespace
{

enum class Blocker
{
    None,
    MessageBox,
    Dialog,
    FileDialog,
    ModalWindow,
    Menu,
    Combo,
    ContextMenu,
    Count
};

const char *blockerName(Blocker blocker)
{
    switch (blocker)
    {
    case Blocker::None: return "none";
    case Blocker::MessageBox: return "messageBox";
    case Blocker::Dialog: return "beginDialog";
    case Blocker::FileDialog: return "fileDialog";
    case Blocker::ModalWindow: return "setModalWindow";
    case Blocker::Menu: return "open menu";
    case Blocker::Combo: return "open combo";
    case Blocker::ContextMenu: return "open context menu";
    default: return "?";
    }
}

bool blockerIsPopup(Blocker blocker)
{
    return blocker == Blocker::Menu || blocker == Blocker::Combo || blocker == Blocker::ContextMenu;
}

class Provider : public ig::FileDialogProvider
{
public:
    bool listDirectory(ig::StringView path, ct::Vector<ig::FileDialogEntry> &entries, bool = true) override
    {
        ig::FileDialogEntry entry;
        entry.name = "file.txt";
        entry.path = ig::String(path.data(), path.size()) + "/file.txt";
        entries.push_back(entry);
        return true;
    }
    ig::String parentDirectory(ig::StringView) override { return "/"; }
    ig::String homeDirectory() override { return "/home/test"; }
};

// ---------------------------------------------------------------------------
// The background widgets and their state.

enum Widget
{
    WButton,
    WSmallButton,
    WCheckbox,
    WToggle,
    WRadio,
    WSelectable,
    WCollapsing,
    WTreeNode,
    WTreeItem,
    WInvisibleButton,
    WIsClicked,
    WImageButton,
    WTabBar,
    WSliderFloat,
    WSliderInt,
    WDragFloat,
    WDragInt,
    WStepper,
    WSplitter,
    WCombo,
    WListBox,
    WInputText,
    WInputInt,
    WInputFloat,
    WMultiline,
    WCodeEditor,
    WColorEdit,
    WGradient,
    WCurve,
    WTimeline,
    WCount
};

enum class Action
{
    Click,     // press + release at (fx, fy)
    Drag,      // press at fx, move to fx2, release (same fy)
    ClickType, // click at (fx, fy), then type "7"
    ComboPick  // click at (fx, fy), then click the second popup row
};

struct Case
{
    const char *name;
    ig::Rect screen; // absolute rect the widget occupies
    Action action;
    float fx;
    float fy;
    float fx2;
};

// Left column x 10..210 and right column x 1060..1260 stay clear of every
// blocker: the 820x560 file dialog, the biggest, spans x 230..1050.
const Case kCases[WCount] = {
    {"button", ig::Rect(10, 10, 200, 28), Action::Click, 0.5f, 0.5f, 0.0f},
    {"smallButton", ig::Rect(10, 50, 200, 28), Action::Click, 0.5f, 0.5f, 0.0f},
    {"checkbox", ig::Rect(10, 90, 200, 28), Action::Click, 0.1f, 0.5f, 0.0f},
    {"toggleSwitch", ig::Rect(10, 130, 200, 28), Action::Click, 0.1f, 0.5f, 0.0f},
    {"radioButton", ig::Rect(10, 170, 200, 28), Action::Click, 0.1f, 0.5f, 0.0f},
    {"selectable", ig::Rect(10, 210, 200, 28), Action::Click, 0.5f, 0.5f, 0.0f},
    {"collapsingHeader", ig::Rect(10, 250, 200, 28), Action::Click, 0.5f, 0.5f, 0.0f},
    {"treeNode", ig::Rect(10, 290, 200, 28), Action::Click, 0.5f, 0.5f, 0.0f},
    {"treeItem", ig::Rect(10, 330, 200, 28), Action::Click, 0.5f, 0.5f, 0.0f},
    {"invisibleButton", ig::Rect(10, 370, 200, 28), Action::Click, 0.5f, 0.5f, 0.0f},
    {"isClicked", ig::Rect(10, 410, 200, 28), Action::Click, 0.5f, 0.5f, 0.0f},
    {"imageButton", ig::Rect(10, 450, 200, 28), Action::Click, 0.5f, 0.5f, 0.0f},
    {"tabBar", ig::Rect(10, 490, 200, 28), Action::Click, 0.9f, 0.5f, 0.0f},
    {"sliderFloat", ig::Rect(10, 530, 200, 28), Action::Click, 0.4f, 0.5f, 0.0f},
    {"sliderInt", ig::Rect(10, 570, 200, 28), Action::Click, 0.4f, 0.5f, 0.0f},
    {"dragFloat", ig::Rect(10, 610, 200, 28), Action::Drag, 0.3f, 0.5f, 0.9f},
    {"dragInt", ig::Rect(10, 650, 200, 28), Action::Drag, 0.3f, 0.5f, 0.9f},
    {"stepperInt", ig::Rect(10, 690, 200, 28), Action::Click, 0.97f, 0.5f, 0.0f},
    {"splitter", ig::Rect(10, 730, 200, 28), Action::Drag, 0.5f, 0.5f, 0.9f},
    {"comboBox", ig::Rect(1060, 10, 200, 28), Action::ComboPick, 0.5f, 0.5f, 0.0f},
    {"listBox", ig::Rect(1060, 200, 200, 100), Action::Click, 0.5f, 0.8f, 0.0f},
    {"inputText", ig::Rect(1060, 310, 200, 28), Action::ClickType, 0.5f, 0.5f, 0.0f},
    {"inputInt", ig::Rect(1060, 350, 200, 28), Action::ClickType, 0.5f, 0.5f, 0.0f},
    {"inputFloat", ig::Rect(1060, 390, 200, 28), Action::ClickType, 0.5f, 0.5f, 0.0f},
    {"inputTextMultiline", ig::Rect(1060, 430, 200, 50), Action::ClickType, 0.5f, 0.5f, 0.0f},
    {"codeEditor", ig::Rect(1060, 490, 200, 60), Action::ClickType, 0.5f, 0.3f, 0.0f},
    {"colorEdit", ig::Rect(1060, 560, 200, 60), Action::Drag, 0.03f, 0.65f, 0.15f},
    {"gradientEditor", ig::Rect(1060, 630, 200, 30), Action::Click, 0.5f, 0.5f, 0.0f},
    {"curveEditor", ig::Rect(1060, 670, 200, 60), Action::Click, 0.5f, 0.5f, 0.0f},
    {"timeline", ig::Rect(1060, 740, 200, 50), Action::Click, 0.6f, 0.7f, 0.0f},
};

struct Scene
{
    int clicks[WCount];
    bool checkbox = false;
    bool toggle = false;
    bool collapsing = false;
    bool treeNode = false;
    bool treeItemExpanded = false;
    int tab = 0;
    float sliderFloat = 0.0f;
    int sliderInt = 0;
    float dragFloat = 0.0f;
    int dragInt = 0;
    int stepper = 0;
    float splitter = 100.0f;
    int combo = 0;
    int list = 0;
    ig::String text;
    int inputInt = 0;
    float inputFloat = 0.0f;
    ig::String multiline;
    ig::CodeEditorState code;
    ig::Color color = ig::Color(10, 20, 30, 255);
    ct::Vector<ig::GradientStop> stops;
    int selectedStop = -1;
    ct::Vector<ig::CurvePoint> curve;
    int selectedPoint = -1;
    ct::Vector<ig::TimelineTrack> tracks;
    int timelineFrame = 0;
    int timelineTrack = -1;
    int timelineKey = -1;
    int childClicked = -1;

    // Blockers.
    Blocker blocker = Blocker::None;
    bool blockerOn = false;
    bool messageOpen = true;
    bool dialogOpen = true;
    bool fileOpen = true;
    ig::FileDialogState fileState;
    Provider provider;
    int hostCombo = 0;

    Scene()
    {
        for (int i = 0; i < WCount; ++i)
            clicks[i] = 0;
        stops.push_back(ig::GradientStop(0.0f, ig::Color(0, 0, 0, 255)));
        stops.push_back(ig::GradientStop(1.0f, ig::Color(255, 255, 255, 255)));
        curve.push_back(ig::CurvePoint(0.0f, 0.0f));
        curve.push_back(ig::CurvePoint(1.0f, 1.0f));
        ig::TimelineTrack track;
        track.label = "track";
        tracks.push_back(track);
        code.setText("abc");
    }
};

uint64_t mix(uint64_t hash, const void *data, size_t size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    for (size_t i = 0; i < size; ++i)
    {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

template <typename T>
uint64_t mixValue(uint64_t hash, const T &value)
{
    return mix(hash, &value, sizeof(value));
}

uint64_t mixString(uint64_t hash, const ig::String &value)
{
    hash = mixValue(hash, value.size());
    return mix(hash, value.data(), value.size());
}

// What the widget "is" - anything a user could observe changing.
uint64_t signature(const Scene &s, int widget)
{
    uint64_t h = 1469598103934665603ull;
    h = mixValue(h, s.clicks[widget]);
    switch (widget)
    {
    case WCheckbox: return mixValue(h, s.checkbox);
    case WToggle: return mixValue(h, s.toggle);
    case WCollapsing: return mixValue(h, s.collapsing);
    case WTreeNode: return mixValue(h, s.treeNode);
    case WTreeItem: return mixValue(h, s.treeItemExpanded);
    case WTabBar: return mixValue(h, s.tab);
    case WSliderFloat: return mixValue(h, s.sliderFloat);
    case WSliderInt: return mixValue(h, s.sliderInt);
    case WDragFloat: return mixValue(h, s.dragFloat);
    case WDragInt: return mixValue(h, s.dragInt);
    case WStepper: return mixValue(h, s.stepper);
    case WSplitter: return mixValue(h, s.splitter);
    case WCombo: return mixValue(h, s.combo);
    case WListBox: return mixValue(h, s.list);
    case WInputText: return mixString(h, s.text);
    case WInputInt: return mixValue(h, s.inputInt);
    case WInputFloat: return mixValue(h, s.inputFloat);
    case WMultiline: return mixString(h, s.multiline);
    case WCodeEditor: return mixString(h, s.code.text());
    case WColorEdit: return mixValue(mixValue(mixValue(mixValue(h, s.color.r), s.color.g), s.color.b), s.color.a);
    case WGradient:
        h = mixValue(h, s.stops.size());
        return mixValue(h, s.selectedStop);
    case WCurve:
        h = mixValue(h, s.curve.size());
        return mixValue(h, s.selectedPoint);
    case WTimeline:
        h = mixValue(h, s.tracks[0].keys.size());
        h = mixValue(h, s.timelineFrame);
        h = mixValue(h, s.timelineTrack);
        return mixValue(h, s.timelineKey);
    default: return h;
    }
}

// Rect-based widgets are placed relative to the window's content origin;
// beginMainWindow's client area has no title bar or padding, so that origin
// is the screen origin (test_controls_reach_their_widget would catch a shift).
ig::Rect local(const ig::Context &, int widget)
{
    return kCases[widget].screen;
}

const ig::StringView kTabs[] = {"one", "two", "three"};
const ig::StringView kItems[] = {"alpha", "beta", "gamma", "delta", "epsilon", "zeta"};

void drawBackground(ig::Context &c, Scene &s)
{
    c.beginMainWindow("main");
    ig::TreeItemStyle style;
    if (c.button("button", local(c, WButton))) ++s.clicks[WButton];
    if (c.smallButton("smallButton", local(c, WSmallButton))) ++s.clicks[WSmallButton];
    c.checkbox("checkbox", s.checkbox, local(c, WCheckbox));
    c.toggleSwitch("toggle", s.toggle, local(c, WToggle));
    if (c.radioButton("radio", false, local(c, WRadio))) ++s.clicks[WRadio];
    if (c.selectable("selectable", false, local(c, WSelectable))) ++s.clicks[WSelectable];
    c.collapsingHeader("collapsing", s.collapsing, local(c, WCollapsing));
    c.treeNode("treeNode", s.treeNode, local(c, WTreeNode));
    if (c.treeItem("treeItem", s.treeItemExpanded, style, local(c, WTreeItem))) ++s.clicks[WTreeItem];
    if (c.invisibleButton("invisible", local(c, WInvisibleButton))) ++s.clicks[WInvisibleButton];
    if (c.isClicked("isClicked", local(c, WIsClicked))) ++s.clicks[WIsClicked];
    if (c.imageButton("imageButton", ig::TextureId(1u), local(c, WImageButton))) ++s.clicks[WImageButton];
    c.tabBar("tabs", s.tab, ig::Span<const ig::StringView>(kTabs, 3), local(c, WTabBar));
    c.sliderFloat("sf", s.sliderFloat, 0.0f, 1.0f, local(c, WSliderFloat));
    c.sliderInt("si", s.sliderInt, 0, 100, local(c, WSliderInt));
    c.dragFloat("dragFloat", s.dragFloat, -1000.0f, 1000.0f, 1.0f, local(c, WDragFloat));
    c.dragInt("dragInt", s.dragInt, -1000, 1000, 1, local(c, WDragInt));
    c.stepperInt("stepper", s.stepper, -100, 100, local(c, WStepper));
    c.splitter("splitter", s.splitter, 0.0f, 200.0f, ig::SplitterAxis::Vertical, local(c, WSplitter));
    c.comboBox("combo", s.combo, ig::Span<const ig::StringView>(kItems, 6), local(c, WCombo));
    c.listBox("list", s.list, ig::Span<const ig::StringView>(kItems, 6), local(c, WListBox));
    c.inputText("inputText", s.text, local(c, WInputText));
    c.inputInt("inputInt", s.inputInt, local(c, WInputInt));
    c.inputFloat("inputFloat", s.inputFloat, local(c, WInputFloat));
    c.inputTextMultiline("multiline", s.multiline, local(c, WMultiline));
    c.codeEditor("code", s.code, local(c, WCodeEditor));
    c.colorEdit("color", s.color, local(c, WColorEdit));
    c.gradientEditor("gradient", s.stops, s.selectedStop, local(c, WGradient));
    c.curveEditor("curve", s.curve, s.selectedPoint, local(c, WCurve));
    c.timeline("timeline", s.tracks, 0, 100, s.timelineFrame, s.timelineTrack, s.timelineKey,
               local(c, WTimeline));
    c.endWindow();
}

// The popup blockers live in a window over the middle of the screen, so
// the menu/combo/context popups open away from both widget columns.
const ig::Rect kHost(450.0f, 200.0f, 380.0f, 300.0f);
const ig::StringView kHostItems[] = {"first", "second", "third"};

void drawBlocker(ig::Context &c, Scene &s)
{
    if (!s.blockerOn)
        return;
    switch (s.blocker)
    {
    case Blocker::MessageBox:
        c.messageBox("Warning", "Something happened", s.messageOpen, true);
        break;
    case Blocker::Dialog:
        if (c.beginDialog("Settings", s.dialogOpen, ig::Vec2(380.0f, 280.0f)))
        {
            c.button("inside", ig::Rect(0.0f, 0.0f, 120.0f, 30.0f));
            c.endDialog();
        }
        break;
    case Blocker::FileDialog:
    {
        ig::FileDialogOptions options;
        c.fileDialog("files", s.fileOpen, s.fileState, options, s.provider);
        break;
    }
    case Blocker::ModalWindow:
        if (c.beginWindow("Tool", kHost))
        {
            c.button("tool button", ig::Rect(0.0f, 0.0f, 120.0f, 30.0f));
            c.endWindow();
        }
        c.setModalWindow("Tool", true);
        break;
    case Blocker::Menu:
    case Blocker::Combo:
    case Blocker::ContextMenu:
        if (c.beginWindow("Host", kHost))
        {
            if (c.beginMenuBar(ig::Rect(0.0f, 0.0f, 360.0f, 24.0f)))
            {
                if (c.beginMenu("File"))
                {
                    c.menuItem("Open");
                    c.menuItem("Save");
                    c.endMenu();
                }
                c.endMenuBar();
            }
            c.comboBox("hostCombo", s.hostCombo, ig::Span<const ig::StringView>(kHostItems, 3),
                       ig::Rect(0.0f, 40.0f, 200.0f, 28.0f));
            if (c.beginContextMenu("hostContext", ig::Rect(0.0f, 100.0f, 360.0f, 150.0f)))
            {
                c.menuItem("Rename");
                c.menuItem("Delete");
                c.endContextMenu();
            }
            c.endWindow();
        }
        break;
    default:
        break;
    }
}

struct Fixture
{
    ig::TestBackend backend;
    ig::Context context;
    ig::Harness harness;
    Scene scene;
    void (*background)(ig::Context &, Scene &);

    Fixture() : backend(), context(backend), harness(context), scene(), background(drawBackground) {}

    void frame() { harness.frame([this](ig::Context &c) { build(c); }); }
    void click(float x, float y, ig::PointerButton button = ig::PointerButton::Left)
    {
        harness.click(x, y, [this](ig::Context &c) { build(c); }, button);
    }
    void drag(float x0, float y0, float x1, float y1)
    {
        harness.drag(x0, y0, x1, y1, [this](ig::Context &c) { build(c); }, 4);
    }
    void type(const char *text)
    {
        context.pushEvent(ig::Event::textInput(text));
        frame();
    }
    void key(ig::KeyCode code, bool control = false, bool shift = false)
    {
        context.pushEvent(ig::Event::keyDown(code, control, shift));
        frame();
    }

    void wheel(float x, float y, float delta)
    {
        context.pushEvent(ig::Event::pointerMove(x, y));
        ig::Event event;
        event.type = ig::EventType::PointerWheel;
        event.wheelY = delta;
        context.pushEvent(event);
        frame();
    }

    // Takes the blocker down so the background can be probed again.
    void lower()
    {
        if (blockerIsPopup(scene.blocker))
            key(ig::KeyCode::Escape);
        scene.blockerOn = false;
        frame();
        frame();
    }

    void build(ig::Context &c)
    {
        background(c, scene);
        drawBlocker(c, scene);
    }

    // Raises the blocker and, for popups, opens it. Returns false if the
    // popup could not be confirmed open (the test itself is then broken).
    bool raise(Blocker blocker)
    {
        scene.blocker = blocker;
        scene.blockerOn = blocker != Blocker::None;
        frame();
        frame();
        const float pad = context.theme().windowPadding;
        const float title = context.theme().titleBarHeight;
        const float originX = kHost.x + pad;
        const float originY = kHost.y + title + pad;
        switch (blocker)
        {
        case Blocker::Menu:
            click(originX + 12.0f, originY + 12.0f);
            frame();
            return context.isMenuOpen();
        case Blocker::ContextMenu:
            click(originX + 180.0f, originY + 170.0f, ig::PointerButton::Right);
            frame();
            return context.isMenuOpen();
        case Blocker::Combo:
        {
            frame();
            const size_t closed = harness.vertexCount();
            click(originX + 100.0f, originY + 54.0f);
            frame();
            return harness.vertexCount() > closed;
        }
        default:
            return true;
        }
    }

    // Returns the number of interaction steps actually run.
    void act(int widget)
    {
        const Case &c = kCases[widget];
        const float x = c.screen.x + c.screen.width * c.fx;
        const float y = c.screen.y + c.screen.height * c.fy;
        switch (c.action)
        {
        case Action::Click:
            click(x, y);
            break;
        case Action::Drag:
            drag(x, y, c.screen.x + c.screen.width * c.fx2, y);
            break;
        case Action::ClickType:
            click(x, y);
            type("7");
            break;
        case Action::ComboPick:
            click(x, y);
            frame();
            // A popup blocker is closed by the first click; the second
            // one is then legitimately free to hit whatever is there.
            if (!blockerIsPopup(scene.blocker))
                click(x, c.screen.y + c.screen.height * 2.5f);
            else
                frame();
            break;
        }
        frame();
    }
};

int gFailures = 0;

void fail(const char *what, const char *blocker, const char *widget)
{
    ++gFailures;
    printf("FAIL %-28s blocker=%-18s widget=%s\n", what, blocker, widget);
}

// 1. Without a blocker each case must change its own widget and nothing else.
void test_controls_reach_their_widget()
{
    for (int w = 0; w < WCount; ++w)
    {
        Fixture f;
        f.raise(Blocker::None);
        uint64_t before[WCount];
        for (int i = 0; i < WCount; ++i)
            before[i] = signature(f.scene, i);
        f.act(w);
        if (signature(f.scene, w) == before[w])
            fail("control: input missed", "none", kCases[w].name);
        for (int i = 0; i < WCount; ++i)
        {
            if (i != w && signature(f.scene, i) != before[i])
            {
                char message[96];
                snprintf(message, sizeof(message), "control: also changed %s", kCases[i].name);
                fail(message, "none", kCases[w].name);
            }
        }
    }
}

// 2. Pointer input aimed at a background widget while a blocker is up.
void test_pointer_is_blocked()
{
    for (int b = 1; b < static_cast<int>(Blocker::Count); ++b)
    {
        const Blocker blocker = static_cast<Blocker>(b);
        for (int w = 0; w < WCount; ++w)
        {
            Fixture f;
            if (!f.raise(blocker))
            {
                fail("setup: blocker did not open", blockerName(blocker), kCases[w].name);
                continue;
            }
            uint64_t before[WCount];
            for (int i = 0; i < WCount; ++i)
                before[i] = signature(f.scene, i);
            f.act(w);
            for (int i = 0; i < WCount; ++i)
            {
                if (signature(f.scene, i) != before[i])
                {
                    char message[96];
                    snprintf(message, sizeof(message), "pointer leak -> %s", kCases[i].name);
                    fail(message, blockerName(blocker), kCases[w].name);
                }
            }
        }
    }
}

// 3. A text widget focused BEFORE the blocker opened must stop receiving
//    keyboard input once it is up.
void test_prefocused_keyboard_is_blocked()
{
    const int textWidgets[] = {WInputText, WInputInt, WInputFloat, WMultiline, WCodeEditor};
    for (int b = 1; b < static_cast<int>(Blocker::Count); ++b)
    {
        const Blocker blocker = static_cast<Blocker>(b);
        for (int t = 0; t < 5; ++t)
        {
            const int w = textWidgets[t];
            Fixture f;
            f.raise(Blocker::None);
            const Case &c = kCases[w];
            f.click(c.screen.x + c.screen.width * c.fx, c.screen.y + c.screen.height * c.fy);
            f.frame();
            // Popup blockers are opened by a click elsewhere, which already
            // takes focus away legitimately - only the true modals matter here.
            if (blockerIsPopup(blocker))
                continue;
            if (!f.raise(blocker))
            {
                fail("setup: blocker did not open", blockerName(blocker), c.name);
                continue;
            }
            const uint64_t before = signature(f.scene, w);
            f.type("9");
            f.key(ig::KeyCode::Backspace);
            f.key(ig::KeyCode::Backspace);
            if (signature(f.scene, w) != before)
                fail("keyboard leak (pre-focused)", blockerName(blocker), c.name);
        }
    }
}

// 4. Enter on a focused button is a click - it must not fire under a modal.
void test_prefocused_enter_is_blocked()
{
    for (int b = 1; b < static_cast<int>(Blocker::Count); ++b)
    {
        const Blocker blocker = static_cast<Blocker>(b);
        if (blockerIsPopup(blocker))
            continue;
        Fixture f;
        f.raise(Blocker::None);
        const Case &c = kCases[WButton];
        f.click(c.screen.x + c.screen.width * 0.5f, c.screen.y + c.screen.height * 0.5f);
        const int clicks = f.scene.clicks[WButton];
        f.raise(blocker);
        f.key(ig::KeyCode::Enter);
        if (f.scene.clicks[WButton] != clicks)
            fail("Enter leak (pre-focused)", blockerName(blocker), c.name);
    }
}

// 5. Tab must not walk focus into the background and let typing land there.
void test_tab_does_not_reach_background()
{
    for (int b = 1; b < static_cast<int>(Blocker::Count); ++b)
    {
        const Blocker blocker = static_cast<Blocker>(b);
        if (blockerIsPopup(blocker))
            continue;
        Fixture f;
        f.raise(blocker);
        uint64_t before[WCount];
        for (int i = 0; i < WCount; ++i)
            before[i] = signature(f.scene, i);
        for (int step = 0; step < 40; ++step)
        {
            f.key(ig::KeyCode::Tab);
            f.type("Z");
        }
        for (int i = 0; i < WCount; ++i)
        {
            if (signature(f.scene, i) != before[i])
                fail("Tab+type leak", blockerName(blocker), kCases[i].name);
        }
    }
}

// 6. Arrow keys on a list/tab/combo focused BEFORE the modal opened.
void test_prefocused_navigation_keys_are_blocked()
{
    struct Nav
    {
        int widget;
        ig::KeyCode key;
        bool reclick; // combo: the first click opens its popup, the second closes it
    };
    const Nav navs[] = {
        {WTabBar, ig::KeyCode::Right, false},
        {WListBox, ig::KeyCode::Down, false},
        {WCombo, ig::KeyCode::Down, true},
    };
    for (int b = 0; b < static_cast<int>(Blocker::Count); ++b)
    {
        const Blocker blocker = static_cast<Blocker>(b);
        if (blockerIsPopup(blocker))
            continue;
        for (int n = 0; n < 3; ++n)
        {
            const Case &c = kCases[navs[n].widget];
            // Focus with a click on the widget's first cell/row.
            const float x = c.screen.x + 8.0f;
            const float y = c.screen.y + 8.0f;
            Fixture f;
            f.raise(Blocker::None);
            f.click(x, y);
            if (navs[n].reclick)
                f.click(x, y);
            f.frame();
            if (!f.raise(blocker))
            {
                fail("setup: blocker did not open", blockerName(blocker), c.name);
                continue;
            }
            const uint64_t before = signature(f.scene, navs[n].widget);
            f.key(navs[n].key);
            const bool changed = signature(f.scene, navs[n].widget) != before;
            if (blocker == Blocker::None && !changed)
                fail("control: key missed", "none", c.name);
            if (blocker != Blocker::None && changed)
                fail("navigation key leak (pre-focused)", blockerName(blocker), c.name);
        }
    }
}

void drawWheelScene(ig::Context &c, Scene &s)
{
    c.beginMainWindow("main");
    c.listBox("list", s.list, ig::Span<const ig::StringView>(kItems, 6), ig::Rect(10.0f, 10.0f, 200.0f, 84.0f));
    c.codeEditor("code", s.code, ig::Rect(10.0f, 120.0f, 200.0f, 100.0f));
    c.setCursor(ig::Vec2(10.0f, 240.0f));
    if (c.beginChild("child", 150.0f, true, 200.0f))
    {
        for (int i = 0; i < 20; ++i)
        {
            char label[8];
            snprintf(label, sizeof(label), "r%d", i);
            if (c.button(label))
                s.childClicked = i;
        }
        c.endChild();
    }
    c.endWindow();
}

// 7. The wheel over a scrollable background widget while a blocker is up.
void test_wheel_is_blocked()
{
    for (int b = 0; b < static_cast<int>(Blocker::Count); ++b)
    {
        const Blocker blocker = static_cast<Blocker>(b);
        const char *name = blockerName(blocker);

        // listBox: after the wheel, the first visible row names the scroll.
        {
            Fixture f;
            f.background = drawWheelScene;
            if (!f.raise(blocker))
            {
                fail("setup: blocker did not open", name, "listBox");
                continue;
            }
            f.wheel(100.0f, 50.0f, -1.0f);
            f.lower();
            f.click(100.0f, 18.0f);
            const bool scrolled = f.scene.list != 0;
            if (blocker == Blocker::None && !scrolled)
                fail("control: wheel missed", name, "listBox");
            if (blocker != Blocker::None && scrolled)
                fail("wheel leak", name, "listBox");
        }
        // codeEditor keeps its scroll in the state.
        {
            Fixture f;
            f.background = drawWheelScene;
            ig::String text;
            for (int i = 0; i < 60; ++i)
                text += "line\n";
            f.scene.code.setText(text);
            f.raise(blocker);
            f.wheel(100.0f, 170.0f, -1.0f);
            const bool scrolled = f.scene.code.scrollLine != 0;
            if (blocker == Blocker::None && !scrolled)
                fail("control: wheel missed", name, "codeEditor");
            if (blocker != Blocker::None && scrolled)
                fail("wheel leak", name, "codeEditor");
        }
        // beginChild: after the wheel, the row under the child's top edge.
        {
            Fixture f;
            f.background = drawWheelScene;
            f.raise(blocker);
            f.wheel(100.0f, 300.0f, -3.0f);
            f.lower();
            // y=266 is inside row 0 unscrolled and inside row 3 after the scroll.
            f.click(30.0f, 266.0f);
            const bool scrolled = f.scene.childClicked > 0;
            if (f.scene.childClicked < 0)
                fail("setup: child row not hit", name, "beginChild");
            else if (blocker == Blocker::None && !scrolled)
                fail("control: wheel missed", name, "beginChild");
            else if (blocker != Blocker::None && scrolled)
                fail("wheel leak", name, "beginChild");
        }
    }
}

// 8. A blocker the application stops submitting (the usual
//    `if (show) messageBox(...)`) must stop blocking: it is no longer on
//    screen, so nothing is there for the user to dismiss.
void test_unsubmitted_blocker_releases_input()
{
    for (int b = 1; b < static_cast<int>(Blocker::Count); ++b)
    {
        const Blocker blocker = static_cast<Blocker>(b);
        Fixture f;
        if (!f.raise(blocker))
        {
            fail("setup: blocker did not open", blockerName(blocker), "button");
            continue;
        }
        f.scene.blockerOn = false;
        f.frame();
        f.frame();
        const Case &c = kCases[WButton];
        const int clicks = f.scene.clicks[WButton];
        f.click(c.screen.x + c.screen.width * 0.5f, c.screen.y + c.screen.height * 0.5f);
        if (f.scene.clicks[WButton] == clicks)
            fail("input stuck after blocker gone", blockerName(blocker), c.name);
    }
}

// 9. A press outside an open popup, in another window, closes the popup (and
//    is consumed by that): the next click on the background works.
void test_click_elsewhere_closes_popup()
{
    for (int b = static_cast<int>(Blocker::Menu); b < static_cast<int>(Blocker::Count); ++b)
    {
        const Blocker blocker = static_cast<Blocker>(b);
        Fixture f;
        if (!f.raise(blocker))
        {
            fail("setup: blocker did not open", blockerName(blocker), "button");
            continue;
        }
        const Case &c = kCases[WButton];
        const float x = c.screen.x + c.screen.width * 0.5f;
        const float y = c.screen.y + c.screen.height * 0.5f;
        f.click(x, y);
        if (f.scene.clicks[WButton] != 0)
            fail("closing click also clicked", blockerName(blocker), c.name);
        f.click(x, y);
        if (f.scene.clicks[WButton] != 1)
            fail("popup still open after click elsewhere", blockerName(blocker), c.name);
    }
}

// 10. Widgets inside the modal keep working: click, type, Tab between fields.
void test_modal_content_still_works()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    bool open = true;
    bool mbOpen = false;
    ig::String first;
    ig::String second;
    ig::String behind;
    int clicked = 0;
    auto build = [&](ig::Context &c)
    {
        c.beginMainWindow("main");
        c.inputText("behind", behind, ig::Rect(10.0f, 10.0f, 200.0f, 28.0f));
        c.endWindow();
        if (c.beginDialog("Settings", open, ig::Vec2(380.0f, 280.0f)))
        {
            if (c.button("ok", ig::Rect(0.0f, 0.0f, 120.0f, 28.0f)))
                ++clicked;
            c.inputText("first", first, ig::Rect(0.0f, 40.0f, 200.0f, 28.0f));
            c.inputText("second", second, ig::Rect(0.0f, 80.0f, 200.0f, 28.0f));
            c.endDialog();
        }
        c.messageBox("Note", "stacked", mbOpen, true);
    };
    harness.frame(build);
    harness.frame(build);
    // Dialog 380x280 centred in 1280x800: (450, 260); content under the
    // title bar starts at y = 288 (see test_interaction_harness.cpp).
    harness.click(510.0f, 302.0f, build);
    if (clicked != 1)
        fail("modal content: button inside dialog", "beginDialog", "button");
    harness.click(550.0f, 342.0f, build);
    context.pushEvent(ig::Event::textInput("a"));
    harness.frame(build);
    context.pushEvent(ig::Event::keyDown(ig::KeyCode::Tab));
    harness.frame(build);
    context.pushEvent(ig::Event::textInput("b"));
    harness.frame(build);
    if (first != "a" || second != "b" || !behind.empty())
        fail("modal content: typing/Tab inside dialog", "beginDialog", "inputText");

    // A message box raised over the dialog takes over; closing it gives
    // the dialog its input back.
    mbOpen = true;
    harness.frame(build);
    harness.frame(build);
    harness.click(510.0f, 302.0f, build);
    if (clicked != 1)
        fail("stacked messageBox: dialog button reacted", "messageBox", "button");
    mbOpen = false;
    harness.frame(build);
    harness.click(510.0f, 302.0f, build);
    if (clicked != 2)
        fail("dialog input not back after messageBox", "beginDialog", "button");
}

// 11. A window the application stopped submitting is gone from the screen,
//     so it must not keep swallowing clicks aimed at what is under it.
void test_unsubmitted_window_does_not_catch_pointer()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    bool showTool = true;
    int clicks = 0;
    auto build = [&](ig::Context &c)
    {
        c.beginMainWindow("main");
        if (c.button("under", ig::Rect(400.0f, 300.0f, 200.0f, 40.0f)))
            ++clicks;
        c.endWindow();
        if (showTool && c.beginWindow("Tool", ig::Rect(350.0f, 250.0f, 300.0f, 200.0f)))
            c.endWindow();
    };
    harness.frame(build);
    harness.click(500.0f, 320.0f, build);
    if (clicks != 0)
        fail("click went through a live window", "window", "button");
    showTool = false;
    harness.frame(build);
    harness.click(500.0f, 320.0f, build);
    if (clicks != 1)
        fail("unsubmitted window still catches clicks", "window", "button");
}

// 12. Clicking the background must not raise it over a setModalWindow()
//     window: the modal would end up hidden behind what it blocks.
void test_click_does_not_raise_window_over_modal()
{
    ig::TestBackend backend;
    ig::Context context(backend);
    ig::Harness harness(context);
    int toolClicks = 0;
    auto build = [&](ig::Context &c)
    {
        if (c.beginWindow("Back", ig::Rect(100.0f, 100.0f, 600.0f, 400.0f)))
            c.endWindow();
        if (c.beginWindow("Tool", ig::Rect(300.0f, 200.0f, 300.0f, 200.0f)))
        {
            if (c.button("tool", ig::Rect(0.0f, 0.0f, 120.0f, 30.0f)))
                ++toolClicks;
            c.endWindow();
        }
        c.setModalWindow("Tool", true);
    };
    harness.frame(build);
    harness.frame(build);
    // Press on the background window, well away from the tool window.
    harness.click(150.0f, 450.0f, build);
    // The tool's button (content origin = window + title bar + padding).
    const float pad = context.theme().windowPadding;
    const float title = context.theme().titleBarHeight;
    harness.click(300.0f + pad + 60.0f, 200.0f + title + pad + 15.0f, build);
    if (toolClicks != 1)
        fail("background click hid the modal window", "setModalWindow", "button");
}

} // namespace

int main()
{
    test_controls_reach_their_widget();
    test_pointer_is_blocked();
    test_prefocused_keyboard_is_blocked();
    test_prefocused_enter_is_blocked();
    test_tab_does_not_reach_background();
    test_prefocused_navigation_keys_are_blocked();
    test_wheel_is_blocked();
    test_unsubmitted_blocker_releases_input();
    test_click_elsewhere_closes_popup();
    test_modal_content_still_works();
    test_unsubmitted_window_does_not_catch_pointer();
    test_click_does_not_raise_window_over_modal();
    if (gFailures != 0)
    {
        printf("test_modal_input: %d failure(s)\n", gFailures);
        return 1;
    }
    printf("test_modal_input: all tests passed\n");
    return 0;
}
