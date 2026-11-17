// Mouse and keyboard through the whole of the zen adapter: real events, made by the XTest extension of an
// X server, reach a window of zen_plataform running ig::zen::App (the platform delivers them: nothing here
// is put into the IO by hand, which is what the SideTabBar test does and what hides a click lost between
// two frames). Each case counts how many frames pass between the injection and its effect, what is lost or
// doubled, and how long the frames take while the input comes. A case that has a right answer (a tap is a
// click, a letter typed is one letter, in order) fails when it is not; the delays and times are printed.
// Needs an X server with the XTEST extension (the test exits 77, skipped, without one), and it injects real
// pointer and keyboard events into the server it is on: it does so only on a throwaway Xvfb that
// tools/xvfb-test.sh started for it (ctest runs it that way), and refuses, before it connects to anything,
// anywhere else (ThrowawayDisplay.hpp). No standard library: ct and C only.

#include "ThrowawayDisplay.hpp"

#include <igui_zen/ZenApp.hpp>

#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/InputWidgets.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/ScrollWidgets.hpp>
#include <igui/widgets/TextInputWidgets.hpp>

#include <platform.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

// Xlib defines these as macros; the widgets have names of their own for some
#undef None
#undef Status
#undef Bool
#undef Always

#include <stdio.h>
#include <string.h>
#include <unistd.h>

using namespace ig::retained;

namespace
{

int gFailures = 0;
void check(bool ok, const char* what)
{
    if (!ok)
    {
        ++gFailures;
        printf("  FAIL %s\n", what);
    }
}

// The wall clock of a frame, in milliseconds.
double nowMs()
{
    return time_seconds() * 1000.0;
}

// What the cases measure: a list of numbers with its median and its worst.
class Samples
{
public:
    void add(double v)
    {
        v_.push_back(v);
    }
    double median() const
    {
        if (v_.empty()) return 0.0;
        ct::Vector<double> s = v_;
        for (size_t i = 1; i < s.size(); ++i)
            for (size_t j = i; j > 0 && s[j - 1] > s[j]; --j)
            {
                const double t = s[j];
                s[j] = s[j - 1];
                s[j - 1] = t;
            }
        return s[s.size() / 2];
    }
    double worst() const
    {
        double w = 0.0;
        for (size_t i = 0; i < v_.size(); ++i)
            if (v_[i] > w) w = v_[i];
        return w;
    }
    size_t count() const
    {
        return v_.size();
    }

private:
    ct::Vector<double> v_;
};

// The events, made by XTest on a connection of their own: the server hands them to the window as it does
// those of a pointer and a keyboard.
class Injector
{
public:
    bool open(PlatformWindow* window)
    {
        dpy_ = XOpenDisplay(nullptr);
        if (!dpy_) return false;
        int event = 0, error = 0, major = 0, minor = 0;
        if (!XTestQueryExtension(dpy_, &event, &error, &major, &minor)) return false;
        win_ = static_cast<Window>(reinterpret_cast<uintptr_t>(window_native_handle(window, NATIVE_WINDOW)));
        if (!win_) return false;
        XSetInputFocus(dpy_, win_, RevertToParent, CurrentTime);
        XSync(dpy_, False);
        return true;
    }
    ~Injector()
    {
        if (dpy_) XCloseDisplay(dpy_);
    }
    // the window's place on the screen: there is no window manager, so it is where it was asked to be
    void locate()
    {
        Window child;
        XTranslateCoordinates(dpy_, win_, DefaultRootWindow(dpy_), 0, 0, &ox_, &oy_, &child);
    }
    void move(int x, int y)
    {
        XTestFakeMotionEvent(dpy_, DefaultScreen(dpy_), ox_ + x, oy_ + y, 0);
    }
    void button(unsigned b, bool down)
    {
        XTestFakeButtonEvent(dpy_, b, down ? True : False, 0);
    }
    void key(KeySym sym, bool down)
    {
        XTestFakeKeyEvent(dpy_, keycodeOf(sym), down ? True : False, 0);
    }
    // a key that exists on no key of the layout (á, ã, ç): the spare codes are given the symbols
    unsigned keycodeOf(KeySym sym)
    {
        const KeyCode code = XKeysymToKeycode(dpy_, sym);
        if (code) return code;
        for (int i = 0; i < spare_; ++i)
            if (spareSyms_[i] == sym) return static_cast<unsigned>(spareCodes_[i]);
        // the highest codes of the server's map are free in a plain X server
        int minKey = 0, maxKey = 0;
        XDisplayKeycodes(dpy_, &minKey, &maxKey);
        const int c = maxKey - 1 - spare_;
        KeySym syms[2] = { sym, sym };
        XChangeKeyboardMapping(dpy_, c, 2, syms, 1);
        XSync(dpy_, False);
        spareSyms_[spare_] = sym;
        spareCodes_[spare_] = c;
        ++spare_;
        return static_cast<unsigned>(c);
    }
    // Everything sent has reached the server, and a moment for it to reach the window's connection.
    void flush()
    {
        XSync(dpy_, False);
        usleep(3000);
    }
    int originX() const
    {
        return ox_;
    }

private:
    Display* dpy_ = nullptr;
    Window win_ = 0;
    int ox_ = 0;
    int oy_ = 0;
    int spare_ = 0;
    KeySym spareSyms_[16];
    int spareCodes_[16];
};

// The window of the cases: a button, a slider, a text field, a list and two panels with a splitter.
struct Scene
{
    ig::zen::App zen;
    Button* button = nullptr;
    Slider* slider = nullptr;
    TextInput* input = nullptr;
    ListBox* list = nullptr;
    Splitter* splitter = nullptr;
    int frame = 0;
    int clicks = 0;
    int clickFrame = -1;
    int sliderChanges = 0;
    int selectionChanges = 0;
    ct::Vector<double> frameMs; // the time of every frame since the last time it was cleared

    bool build()
    {
        ig::zen::App::Config config;
        config.title = "zen input";
        config.width = 800;
        config.height = 600;
        config.preferOpenGL = false; // the software rasteriser: no GL, and nothing waits for a vertical sync
        config.vsync = false;
        config.showStats = false;
        if (!zen.create(config)) return false;
        WidgetApp& app = zen.widgets();
        Widget* root = app.addStage("input");
        BoxLayout* box = root->createChild<BoxLayout>(LayoutDir::Vertical);
        box->setPadding(10.0f);
        button = box->createChild<Button>("Ok");
        slider = box->createChild<Slider>(0.0f, 1000.0f, 0.0f);
        input = box->createChild<TextInput>("");
        list = box->createChild<ListBox>();
        for (int i = 0; i < 300; ++i) list->addItem(String("Item ") + String::number(i));
        list->setStretch(1.0f);
        splitter = box->createChild<Splitter>(LayoutDir::Horizontal);
        splitter->createChild<Panel>();
        splitter->createChild<Panel>();
        splitter->setMinSize(40.0f);
        splitter->setStretch(1.0f);
        button->clicked.connect(
                [this]
                {
                    ++clicks;
                    clickFrame = frame;
                });
        slider->onValueChanged.connect([this](float) { ++sliderChanges; });
        list->selectionChanged.connect([this](int) { ++selectionChanges; });
        app.setStage("input", TransitionType::None);
        return true;
    }
    // One frame, timed.
    void step()
    {
        const double t0 = nowMs();
        zen.pump();
        frameMs.push_back(nowMs() - t0);
        ++frame;
    }
    void steps(int n)
    {
        for (int i = 0; i < n; ++i) step();
    }
    Rect centre(const Widget* w, float& x, float& y) const
    {
        const Rect r = w->absoluteRect();
        x = r.x + r.w * 0.5f;
        y = r.y + r.h * 0.5f;
        return r;
    }
};

void report(const char* what, const Samples& delay, int lost, int doubled, const ct::Vector<double>& frameMs)
{
    Samples ms;
    for (size_t i = 0; i < frameMs.size(); ++i) ms.add(frameMs[i]);
    printf("  %-44s delay: median %.0f worst %.0f frames; lost %d, doubled %d; frame %.2f ms median, %.2f worst\n", what,
            delay.median(), delay.worst(), lost, doubled, ms.median(), ms.worst());
}

// A press and a release one after the other (a tap on a touch pad: the two reach the window together) is a
// click; so is a press, a frame, and a release.
void testClicks(Scene& s, Injector& x)
{
    float cx = 0, cy = 0;
    s.centre(s.button, cx, cy);
    x.move(static_cast<int>(cx), static_cast<int>(cy));
    x.flush();
    s.steps(2);
    check(s.button->isHovered(), "the pointer over the button hovers it in the frame that reads the move");
    Samples delay;
    int lost = 0, doubled = 0;
    // taps: press and release one after the other, 20 times
    s.frameMs.clear();
    for (int i = 0; i < 20; ++i)
    {
        const int before = s.clicks;
        const int injected = s.frame;
        x.button(1, true);
        x.button(1, false);
        x.flush();
        int waited = 0;
        while (s.clicks == before && waited < 4)
        {
            s.step();
            ++waited;
        }
        if (s.clicks == before) ++lost;
        else
        {
            delay.add(static_cast<double>(s.clickFrame - injected));
            if (s.clicks - before > 1) ++doubled;
        }
        s.steps(2);
    }
    report("button: a tap (press and release together)", delay, lost, doubled, s.frameMs);
    check(lost == 0, "a press and a release that reach the window together are a click");
    check(doubled == 0, "and are one click");
    check(delay.worst() == 0.0, "in the frame that reads them (a release held back for a frame is a click a frame late)");
    // a click with a frame between the press and the release
    Samples slow;
    int slowLost = 0;
    s.frameMs.clear();
    for (int i = 0; i < 20; ++i)
    {
        const int before = s.clicks;
        const int injected = s.frame;
        x.button(1, true);
        x.flush();
        s.step();
        x.button(1, false);
        x.flush();
        s.step();
        if (s.clicks == before) ++slowLost;
        else
            slow.add(static_cast<double>(s.clickFrame - injected));
        s.steps(1);
    }
    report("button: press, a frame, release", slow, slowLost, 0, s.frameMs);
    check(slowLost == 0, "a click with a frame between press and release is a click");
}

// The pointer dragging the slider's knob at `rate` events a second for half a second, 30 frames: the value
// follows the last event of every frame, and ends where the pointer does.
void testSliderDrag(Scene& s, Injector& x, int rate)
{
    float cx = 0, cy = 0;
    const Rect r = s.centre(s.slider, cx, cy);
    const float left = r.x + 10.0f, right = r.x + r.w - 10.0f;
    x.move(static_cast<int>(left), static_cast<int>(cy));
    x.flush();
    s.steps(2);
    x.button(1, true);
    x.flush();
    s.steps(2);
    const int events = rate / 2;
    const int frames = 30;
    s.frameMs.clear();
    Samples delay;
    float last = s.slider->value();
    int backwards = 0;
    int sent = 0;
    for (int f = 0; f < frames; ++f)
    {
        const int upTo = events * (f + 1) / frames;
        float lastX = left;
        for (; sent < upTo; ++sent)
        {
            lastX = left + (right - left) * static_cast<float>(sent + 1) / static_cast<float>(events);
            x.move(static_cast<int>(lastX), static_cast<int>(cy));
        }
        x.flush();
        s.step();
        // where the slider should be after the frame: the position of the last event, as a value
        const float want = 1000.0f * (lastX - r.x) / r.w;
        const float got = s.slider->value();
        if (got < last - 0.5f) ++backwards;
        last = got;
        // how many frames behind: the distance in frames of 1/30 of the whole drag
        const float behind = (want - got) / (1000.0f / static_cast<float>(frames)) * 1.0f;
        delay.add(behind > 0.0f ? behind : 0.0f);
    }
    x.button(1, false);
    x.flush();
    s.steps(3);
    char name[80];
    snprintf(name, sizeof name, "slider drag, %d events a second", rate);
    report(name, delay, 0, backwards, s.frameMs);
    const float endWant = 1000.0f * ((left + (right - left)) - r.x) / r.w;
    check(s.slider->value() > endWant - 30.0f && s.slider->value() < endWant + 30.0f, "the slider ends where the pointer let go");
    if (s.slider->value() < endWant - 30.0f || s.slider->value() > endWant + 30.0f)
        printf("    value %.1f, wanted about %.1f\n", s.slider->value(), endWant);
    check(backwards == 0, "and never goes back during the drag");
}

// The next codepoint of a UTF-8 text, and where it ends.
unsigned nextCodepoint(const char* t, int& n)
{
    const unsigned char c = static_cast<unsigned char>(t[0]);
    if (c < 0x80)
    {
        n = 1;
        return c;
    }
    if (c >= 0xC0 && c < 0xE0)
    {
        n = 2;
        return ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(t[1]) & 0x3Fu);
    }
    n = 1;
    return c;
}

// Types a text on the injector: a key pressed and released for each letter, Shift held for a capital.
// Latin-1 letters (á ã ç é) have the keysym of their codepoint.
void typeText(Injector& x, const char* text, int perFrame, Scene* scene)
{
    int inFrame = 0;
    for (int i = 0; text[i];)
    {
        int n = 1;
        const unsigned cp = nextCodepoint(text + i, n);
        i += n;
        const bool shift = cp >= 'A' && cp <= 'Z';
        if (shift) x.key(XK_Shift_L, true);
        x.key(static_cast<KeySym>(cp), true);
        x.key(static_cast<KeySym>(cp), false);
        if (shift) x.key(XK_Shift_L, false);
        if (scene && ++inFrame == perFrame)
        {
            x.flush();
            scene->step();
            inFrame = 0;
        }
    }
    x.flush();
}

// Typing into the text field: every letter, in order, none doubled or lost, with the letters Portuguese needs.
void testTyping(Scene& s, Injector& x)
{
    float cx = 0, cy = 0;
    s.centre(s.input, cx, cy);
    x.move(static_cast<int>(cx), static_cast<int>(cy));
    x.flush();
    s.steps(1);
    x.button(1, true);
    x.button(1, false);
    x.flush();
    s.steps(2);
    const char* text = "Conceição de João, é útil à vida. ã ç á";
    const struct
    {
        int perFrame;
        const char* what;
    } cases[] = { { 1000, "typing: the whole text in one frame" }, { 3, "typing: three letters a frame" },
        { 1, "typing: one letter a frame" } };
    for (const auto& c: cases)
    {
        s.input->setText("");
        s.frameMs.clear();
        const int first = s.frame;
        typeText(x, text, c.perFrame, &s);
        s.steps(3);
        const bool same = s.input->text() == String(text);
        char name[96];
        snprintf(name, sizeof name, "%s", c.what);
        printf("  %-44s %d frames; %s\n", name, s.frame - first, same ? "the text is exact" : "the text is NOT exact");
        check(same, c.what);
        if (!same) printf("    got \"%s\"\n    want \"%s\"\n", s.input->text().c_str(), text);
    }
}

// Key auto-repeat in the list: a key held down is a press every few milliseconds; each one moves the selection.
void testKeyRepeat(Scene& s, Injector& x)
{
    float cx = 0, cy = 0;
    s.centre(s.list, cx, cy);
    x.move(static_cast<int>(cx), static_cast<int>(cy) - 20);
    x.flush();
    s.steps(1);
    x.button(1, true);
    x.button(1, false);
    x.flush();
    s.steps(2);
    const int start = s.list->selectedIndex();
    check(start >= 0, "a click selects an item of the list");
    s.frameMs.clear();
    // a key held down: the server sends a release and a press for each repeat, with no time between them
    const int presses = 40;
    x.key(XK_Down, true);
    for (int i = 1; i < presses; ++i)
    {
        x.key(XK_Down, false);
        x.key(XK_Down, true);
    }
    x.key(XK_Down, false);
    x.flush();
    s.steps(4);
    const int moved = s.list->selectedIndex() - start;
    printf("  %-44s %d presses moved the selection %d rows; frame %.2f ms worst\n", "list: Down held down", presses, moved,
            [&] { Samples m; for (size_t i = 0; i < s.frameMs.size(); ++i) m.add(s.frameMs[i]); return m.worst(); }());
    check(moved == presses, "every repeat of a held key moves the selection one row");
    // the same spread over frames
    const int before = s.list->selectedIndex();
    for (int f = 0; f < 10; ++f)
    {
        for (int i = 0; i < 3; ++i)
        {
            x.key(XK_Down, true);
            x.key(XK_Down, false);
        }
        x.flush();
        s.step();
    }
    x.flush();
    s.steps(2);
    check(s.list->selectedIndex() - before == 30, "and so do thirty of them spread over ten frames");
}

// The wheel over the list: every notch counts.
void testWheel(Scene& s, Injector& x)
{
    float cx = 0, cy = 0;
    s.centre(s.list, cx, cy);
    x.move(static_cast<int>(cx), static_cast<int>(cy));
    x.flush();
    s.steps(2);
    const Rect top = s.list->absoluteRect();
    // which item is at the top of the list: the one a click on its first row selects
    auto topItem = [&]() -> int
    {
        x.move(static_cast<int>(top.x + 20.0f), static_cast<int>(top.y + 6.0f));
        x.button(1, true);
        x.button(1, false);
        x.flush();
        s.steps(2);
        return s.list->selectedIndex();
    };
    const int first = topItem();
    const int notches = 12;
    for (int i = 0; i < notches; ++i)
    {
        x.button(5, true);
        x.button(5, false);
    }
    x.flush();
    s.steps(3);
    const int second = topItem();
    // the first click may have scrolled the list to show the selection: the notches come after it
    printf("  %-44s %d notches moved the top item from %d to %d\n", "list: wheel", notches, first, second);
    check(second > first, "the wheel scrolls the list");
    for (int i = 0; i < notches; ++i)
    {
        x.button(5, true);
        x.button(5, false);
        x.flush();
        s.step();
    }
    s.steps(2);
    const int third = topItem();
    check(third - second == second - first || third - second >= second - first - 1,
            "notches spread over frames scroll as much as the same notches in one frame");
    if (third - second != second - first) printf("    %d then %d rows\n", second - first, third - second);
}

// The splitter's handle dragged at a thousand events a second.
void testSplitter(Scene& s, Injector& x)
{
    const Rect r = s.splitter->absoluteRect();
    const float before = s.splitter->ratio();
    const int y = static_cast<int>(r.y + r.h * 0.5f);
    const int start = static_cast<int>(r.x + r.w * before);
    x.move(start, y);
    x.flush();
    s.steps(2);
    x.button(1, true);
    x.flush();
    s.steps(1);
    const int events = 500;
    const int frames = 30;
    int sent = 0;
    s.frameMs.clear();
    Samples delay;
    for (int f = 0; f < frames; ++f)
    {
        const int upTo = events * (f + 1) / frames;
        int lastX = start;
        for (; sent < upTo; ++sent)
        {
            lastX = start + 100 * (sent + 1) / events;
            x.move(lastX, y);
        }
        x.flush();
        s.step();
        const float want = (static_cast<float>(lastX) - r.x) / r.w;
        const float behind = (want - s.splitter->ratio()) * r.w;
        delay.add(behind > 0.0f ? behind : 0.0f);
    }
    x.button(1, false);
    x.flush();
    s.steps(2);
    const float moved = (s.splitter->ratio() - before) * r.w;
    printf("  %-44s moved %.0f of 100 pixels; behind the pointer %.0f px median, %.0f worst\n", "splitter drag", moved, delay.median(),
            delay.worst());
    check(moved > 95.0f && moved < 105.0f, "the splitter ends where the pointer let go");
}

// A press, a drag and a release that all fall between two frames: the press is where the pointer was when it
// was pressed, so the text the drag crossed is selected (the press taken at the end of the drag selects none).
// And a letter, a key and another letter of one frame act in the order they were typed.
void testOneFrameOfInput(Scene& s, Injector& x)
{
    float cx = 0, cy = 0;
    const Rect r = s.centre(s.input, cx, cy);
    s.input->setText("0123456789 0123456789");
    x.move(static_cast<int>(r.x + 40.0f), static_cast<int>(cy));
    x.flush();
    s.steps(2);
    x.button(1, true);
    x.button(1, false);
    x.flush();
    s.steps(2);
    // press near the start of the text, drag to the middle, release: one frame
    x.move(static_cast<int>(r.x + 14.0f), static_cast<int>(cy));
    x.button(1, true);
    x.move(static_cast<int>(r.x + 60.0f), static_cast<int>(cy));
    x.move(static_cast<int>(r.x + 90.0f), static_cast<int>(cy));
    x.button(1, false);
    x.flush();
    s.step();
    check(s.input->hasSelection() && s.input->selectedText().size() >= 3,
            "a press and a drag in one frame select the text the drag crossed");
    // the letters and the key of one frame
    s.input->setText("");
    x.key(XK_a, true);
    x.key(XK_a, false);
    x.key(XK_Left, true);
    x.key(XK_Left, false);
    x.key(XK_b, true);
    x.key(XK_b, false);
    x.flush();
    s.step();
    s.steps(1);
    check(s.input->text() == "ba", "a, Left and b typed within one frame make ba");
    if (s.input->text() != "ba") printf("    got \"%s\"\n", s.input->text().c_str());
    // Ctrl+A with the Ctrl pressed and released inside the frame: the key carries the Ctrl it was pressed with
    s.input->setText("selecione tudo");
    s.input->clearSelection();
    x.key(XK_Control_L, true);
    x.key(XK_a, true);
    x.key(XK_a, false);
    x.key(XK_Control_L, false);
    x.flush();
    s.step();
    s.steps(1);
    check(s.input->hasSelection() && s.input->selectedText() == "selecione tudo",
            "Ctrl+A with the Ctrl pressed and released within one frame selects all");
    // and a Shift+Left, the same way, extends the selection by one letter
    s.input->clearSelection();
    x.key(XK_Shift_L, true);
    x.key(XK_Left, true);
    x.key(XK_Left, false);
    x.key(XK_Shift_L, false);
    x.flush();
    s.step();
    s.steps(1);
    check(s.input->hasSelection(), "Shift+Left with the Shift pressed and released within one frame selects a letter");
}

} // namespace

int main()
{
    // first of all: no window and no event on a display that is not a throwaway one
    if (!igui_test::onThrowawayDisplay("test_zen_input")) return 77;
    Scene scene;
    if (!scene.build())
    {
        printf("test_zen_input: no window (no X server?): skipped\n");
        return 77;
    }
    Injector x;
    if (!x.open(scene.zen.window()))
    {
        printf("test_zen_input: no XTEST extension: skipped\n");
        return 77;
    }
    scene.steps(5);
    x.locate();
    printf("test_zen_input: software rasteriser, 800x600 window at %d\n", x.originX());
    testClicks(scene, x);
    testSliderDrag(scene, x, 125);
    testSliderDrag(scene, x, 1000);
    testSliderDrag(scene, x, 8000);
    testTyping(scene, x);
    testOneFrameOfInput(scene, x);
    testKeyRepeat(scene, x);
    testWheel(scene, x);
    testSplitter(scene, x);
    if (gFailures) printf("%d checks failed\n", gFailures);
    else
        printf("ok\n");
    return gFailures ? 1 : 0;
}
