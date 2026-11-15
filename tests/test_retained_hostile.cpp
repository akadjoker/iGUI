// Numbers that are not numbers (NaN), infinite, negative, zero and enormous in the float setters of the retained
// widgets: a widget made with each, put through a layout and a paint (the real frame loop, tests/RetainedHarness.hpp),
// must not crash, turn a loop into an endless one or write a vertex that is not finite, and a frame of a 480x320
// window must stay under a sane number of vertices (a loop that draws one rectangle for each step of a float that
// no longer advances would take the memory of the machine, so a regression has to fail fast, not grow). Meant for a
// sanitizer build too (the conversions of a float to an int that does not fit are what UndefinedBehaviorSanitizer
// finds, with -fsanitize=float-cast-overflow too). One check for each setter, and a few scenes for the loops that
// step through time, pixels or grid lines. No standard library: ct and C only.
//
//   igui_retained_hostile_tests [--list] [--only=TEXT]
//
// --list prints the names of the cases, --only=TEXT runs the cases whose name has TEXT in it (one run of the
// program for each case is how to find which one hangs).

#include "RetainedHarness.hpp"

#include <igui/widgets/AppWidgets.hpp>
#include <igui/widgets/AssetBrowser.hpp>
#include <igui/widgets/AudioWidgets.hpp>
#include <igui/widgets/AutomotiveWidgets.hpp>
#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/ChartWidget.hpp>
#include <igui/widgets/ChartWidgets.hpp>
#include <igui/widgets/DataWidgets.hpp>
#include <igui/widgets/DockPanel.hpp>
#include <igui/widgets/InputWidgets.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/NodeEditor.hpp>
#include <igui/widgets/ScrollWidgets.hpp>
#include <igui/widgets/ThumbnailGrid.hpp>
#include <igui/widgets/Timeline.hpp>
#include <igui/widgets/ToolBar.hpp>
#include <igui/widgets/TreePropertyColorWidgets.hpp>
#include <igui/widgets/ViewWidgets.hpp>

using namespace rh;

namespace
{

// A 480x320 window never needs more than this for a frame of one widget (an editor window of 1600x900 is
// about forty thousand); a frame over it is a loop that did not stop where it should.
constexpr size_t kMaxVertices = 200000;

// Whether every rectangle of the tree and every vertex of the frame is finite.
bool finite(float v)
{
    return v == v && v - v == 0.0f;
}

bool treeFinite(const Widget* w)
{
    const Rect& r = w->rect();
    if (!finite(r.x) || !finite(r.y) || !finite(r.w) || !finite(r.h)) return false;
    for (const Widget* c: w->children())
        if (!treeFinite(c)) return false;
    return true;
}

bool vertexFinite()
{
    const DrawList& dl = GetDrawList();
    for (size_t i = 0; i < dl.vertices().size(); ++i)
        if (!finite(dl.vertices()[i].x) || !finite(dl.vertices()[i].y)) return false;
    return true;
}

// A window with a vertical box, and the frames of a case checked as they are drawn: the first thing that is wrong
// is printed (with what had just been done) and the case stops there.
struct Scene
{
    explicit Scene(const char* caseName)
            : h(480.0f, 320.0f),
              name(caseName)
    {
        box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
    }

    // The tree is laid out again and `frames` frames are drawn; false when one of them is not right.
    bool settle(const char* what, int frames = 2)
    {
        if (!ok) return false;
        h.app.requestLayout();
        for (int i = 0; i < frames; ++i)
        {
            h.frame();
            const size_t n = GetDrawList().vertices().size();
            if (n > kMaxVertices) return fail(what, "vertices over the limit", static_cast<double>(n));
            if (!treeFinite(h.root())) return fail(what, "a rectangle is not finite", 0.0);
            if (!vertexFinite()) return fail(what, "a vertex is not finite", 0.0);
        }
        return true;
    }

    bool fail(const char* what, const char* why, double number)
    {
        ok = false;
        printf("    %s: %s after %s (%g)\n", name, why, what, number);
        return false;
    }

    Harness h;
    BoxLayout* box = nullptr;
    const char* name;
    bool ok = true;
};

// ── A setter given each hostile value ────────────────────────────────────────────────────────────────────────────

struct HostileCase
{
    const char* name;
    void (*apply)(BoxLayout* box, Widget** made, float v);
};

// A widget of `Type` made once (the first value) and then given every value; `call` is what is done with v.
#define H(Name, Type, call) \
    { Name, [](BoxLayout* box, Widget** made, float v) { Type* w = *made ? static_cast<Type*>(*made) : box->createChild<Type>(); *made = w; call; } }

const HostileCase kHostile[] = {
    H("Slider::setValue", Slider, w->setValue(v)),
    H("Slider::setRange(0, v)", Slider, w->setRange(0.0f, v)),
    H("ProgressBar::setValue", ProgressBar, w->setValue(v)),
    H("ProgressBar::setRange(0, v)", ProgressBar, w->setRange(0.0f, v)),
    H("SpinBox::setValue", SpinBox, w->setValue(v)),
    H("SpinBox::setStep", SpinBox, w->setStep(v)),
    H("SpinBox::setRange(v, v)", SpinBox, w->setRange(v, v)),
    H("SpinBox::setDragSensitivity", SpinBox, w->setDragSensitivity(v)),
    H("ScrollBar::setValue", ScrollBar, w->setValue(v)),
    H("ScrollBar::setContentSize", ScrollBar, w->setContentSize(v)),
    H("ScrollBar::setViewSize", ScrollBar, w->setViewSize(v)),
    H("ScrollView::setScrollX", ScrollView, w->setScrollX(v)),
    H("ScrollView::setScrollY", ScrollView, w->setScrollY(v)),
    H("ThumbnailGrid::setThumbSize", ThumbnailGrid, w->setThumbSize(v, v)),
    H("ThumbnailGrid::setSpacing", ThumbnailGrid, w->setSpacing(v)),
    H("ThumbnailGrid::setScrollY", ThumbnailGrid, w->setScrollY(v)),
    H("AssetBrowser::setThumbSize", AssetBrowser, w->setThumbSize(v)),
    H("Splitter::setRatio", Splitter, w->setRatio(v)),
    H("Splitter::setMinRatio", Splitter, w->setMinRatio(v)),
    H("Splitter::setMaxRatio", Splitter, w->setMaxRatio(v)),
    H("Splitter::setHandleSize", Splitter, w->setHandleSize(v)),
    H("Splitter::setMinSize", Splitter, w->setMinSize(v)),
    H("DockPanel::setTabBarHeight", DockPanel, w->setTabBarHeight(v)),
    H("DockPanel::setHandleSize", DockPanel, w->setHandleSize(v)),
    H("ToolBar::setIconPadding", ToolBar, w->setIconPadding(v)),
    H("ToolBar::setSpacing", ToolBar, w->setSpacing(v)),
    H("Timeline::setHeaderWidth", Timeline, w->setHeaderWidth(v)),
    H("Timeline::setTrackHeight", Timeline, w->setTrackHeight(v)),
    H("Timeline::setEdgePadding", Timeline, w->setEdgePadding(v)),
    H("Timeline::setVerticalScroll", Timeline, w->setVerticalScroll(v)),
    H("Timeline::setTimeRange(v, v)", Timeline, w->setTimeRange(v, v)),
    H("Timeline::setTimeRange(0, v)", Timeline, w->setTimeRange(0.0f, v)),
    H("Timeline::setFrameRate", Timeline, w->setFrameRate(v)),
    H("Knob::setRange(v, v)", Knob, w->setRange(v, v)),
    H("Knob::setSensitivity", Knob, w->setSensitivity(v)),
    H("Fader::setRange(v, v)", Fader, w->setRange(v, v)),
    H("VUMeter::setPeakDecayRate", VUMeter, w->setPeakDecayRate(v)),
    H("SpectrumAnalyzer::setSmoothing", SpectrumAnalyzer, w->setSmoothing(v)),
    H("SpectrumAnalyzer::setSampleRate", SpectrumAnalyzer, w->setSampleRate(v)),
    H("WaveformView::setPlayhead", WaveformView, w->setPlayhead(v)),
    H("WaveformView::setViewRange(v, v)", WaveformView, w->setViewRange(v, v)),
    H("WaveformView::setLoopRegion(0, v)", WaveformView, w->setLoopRegion(0.0f, v)),
    H("PianoRoll::setTimeRange(v, v)", PianoRoll, w->setTimeRange(v, v)),
    H("PianoRoll::setTimeRange(0, v)", PianoRoll, w->setTimeRange(0.0f, v)),
    H("PianoRoll::setPlayhead", PianoRoll, w->setPlayhead(v)),
    H("PianoRoll::setKeyWidth", PianoRoll, w->setKeyWidth(v)),
    H("PianoRoll::setRowHeight", PianoRoll, w->setRowHeight(v)),
    H("RadialGauge::setValue", RadialGauge, w->setValue(v)),
    H("RadialGauge::setNeedleLength", RadialGauge, w->setNeedleLength(v)),
    H("PowerBar::setValue", PowerBar, w->setValue(v)),
    H("PowerBar::setRange(v, v)", PowerBar, w->setRange(v, v)),
    H("BatteryGauge::setPercent", BatteryGauge, w->setPercent(v)),
    H("DigitalSpeed::setValue", DigitalSpeed, w->setValue(v)),
    H("VideoCanvas::setZoom", VideoCanvas, w->setZoom(v)),
    H("VideoCanvas::setPixelAspect", VideoCanvas, w->setPixelAspect(v)),
    H("VideoCanvas::setMargin", VideoCanvas, w->setMargin(v)),
    H("VideoCanvas::setViewTransform", VideoCanvas, w->setViewTransform(v, v, v)),
    H("ImageView::setOffset", ImageView, w->setOffset(v, v)),
    H("PageView::setTransitionDuration", PageView, w->setTransitionDuration(v)),
    H("GridLayout::setSpacing", GridLayout, w->setSpacing(v, v)),
    H("GridLayout::setPadding", GridLayout, w->setPadding(v, v, v, v)),
    H("FormLayout::setLabelWidth", FormLayout, w->setLabelWidth(v)),
    H("FlowLayout::setSpacing", FlowLayout, w->setSpacing(v, v)),
    H("Collapsible::setContentSpacing", Collapsible, w->setContentSpacing(v)),
    H("StatusBar::setSpacing", StatusBar, w->setSpacing(v)),
    H("StackLayout::setAnimDuration", StackLayout, w->setAnimDuration(v)),
    H("Carousel::setAutoPlayInterval", Carousel, w->setAutoPlayInterval(v)),
    H("Carousel::setAnimDuration", Carousel, w->setAnimDuration(v)),
    H("SlidePanel::setWidth", SlidePanel, w->setWidth(v)),
    H("Widget::setPosition", Panel, w->setPosition(v, v)),
    H("Widget::setTooltipDelay", Panel, w->setTooltipDelay(v)),
    H("TreeGrid::setIndentWidth", TreeGrid, w->setIndentWidth(v)),
    H("TreeGrid::setColumnWidth", TreeGrid, w->setColumnWidth(0, v)),
    H("DataGrid::setColumnWidth", DataGrid, w->setColumnWidth(0, v)),
    H("Chart::setDonutRatio", Chart, w->setDonutRatio(v)),
    H("NodeEditor::setGridSnap", NodeEditor, w->setGridSnap(v)),
};

// The values: not a number, the infinities, negative, zero, the very small and the very big.
const float kValues[] = { 0.0f / 0.0f, 1.0f / 0.0f, -1.0f / 0.0f, -1.0f, 0.0f, 1.0e-30f, 1.0e30f, -1.0e30f };

void runHostile(const HostileCase& c)
{
    Scene s(c.name);
    Widget* made = nullptr;
    for (size_t i = 0; i < sizeof kValues / sizeof kValues[0]; ++i)
    {
        c.apply(s.box, &made, kValues[i]);
        char what[48];
        snprintf(what, sizeof what, "the value %g", static_cast<double>(kValues[i]));
        if (!s.settle(what)) break;
    }
    String text = String(c.name) + ": hostile values leave the geometry finite and the frame small";
    check(s.ok, text.c_str());
}

// ── Scenes: loops that step through time, pixels or grid lines ───────────────────────────────────────────────────

struct SceneCase
{
    const char* name;
    void (*run)(Scene& s);
};

// Starts and widths (in beats or seconds) of a visible time range around the places where a float stops adding
// one: 2^24 (a whole number cannot go up by one any more), 2^31 and the enormous ones; the widths are a tiny range,
// a few ones and a million.
const float kStarts[] = { 0.0f, 16777200.0f, 16777216.0f, 33554432.0f, 2147483648.0f, 1.0e15f, 1.0e30f, -16777216.0f, -2147483648.0f, -1.0e30f };
const float kWidths[] = { 1.0f, 16.0f, 1.0e6f, 0.0f, 1.0e30f };

void sceneTimeRanges(Scene& s, PianoRoll* roll, Timeline* line)
{
    char what[96];
    for (float start: kStarts)
        for (float width: kWidths)
        {
            snprintf(what, sizeof what, "the range %g + %g", static_cast<double>(start), static_cast<double>(width));
            if (roll) roll->setTimeRange(start, start + width);
            if (line) line->setTimeRange(start, start + width);
            if (!s.settle(what, 1)) return;
        }
    // and the same with the ends the same, infinite or not a number
    const float odd[] = { 1.0f / 0.0f, -1.0f / 0.0f, 0.0f / 0.0f, 3.0e38f, -3.0e38f };
    for (float a: odd)
        for (float b: odd)
        {
            snprintf(what, sizeof what, "the range %g to %g", static_cast<double>(a), static_cast<double>(b));
            if (roll) roll->setTimeRange(a, b);
            if (line) line->setTimeRange(a, b);
            if (!s.settle(what, 1)) return;
        }
}

PianoRoll* makeRoll(Scene& s)
{
    PianoRoll* roll = s.box->createChild<PianoRoll>();
    roll->setStretch(1.0f);
    roll->addNote(60, 0.0f, 1.0f);
    roll->addNote(64, 16777216.0f, 4.0f);
    roll->addNote(67, 2147483648.0f, 1.0e6f);
    roll->addNote(62, -5.0f, 1.0e30f);
    return roll;
}

const SceneCase kScenes[] = {
    { "PianoRoll::setBeatsPerBar(0, -1, INT_MIN)",
            [](Scene& s)
            {
                PianoRoll* roll = makeRoll(s);
                roll->setTimeRange(0.0f, 16.0f);
                const int values[] = { 0, -1, -2147483647 - 1, 1, 2147483647, 4 };
                for (int bpb: values)
                {
                    char what[48];
                    snprintf(what, sizeof what, "%d beats to a bar", bpb);
                    roll->setBeatsPerBar(bpb);
                    if (!s.settle(what)) return;
                }
            } },
    { "PianoRoll: time ranges near 2^24, 2^31 and enormous",
            [](Scene& s)
            {
                PianoRoll* roll = makeRoll(s);
                sceneTimeRanges(s, roll, nullptr);
            } },
    { "PianoRoll: a million beats in view, whatever the beats to a bar",
            [](Scene& s)
            {
                PianoRoll* roll = makeRoll(s);
                roll->setTimeRange(0.0f, 1.0e6f);
                const int values[] = { 1, 2, 3, 4, 7, 16, 1000 };
                for (int bpb: values)
                {
                    char what[48];
                    snprintf(what, sizeof what, "%d beats to a bar", bpb);
                    roll->setBeatsPerBar(bpb);
                    if (!s.settle(what)) return;
                }
            } },
    { "PianoRoll: a note with a start or a length that is not a number",
            [](Scene& s)
            {
                PianoRoll* roll = makeRoll(s);
                const float odd[] = { 0.0f / 0.0f, 1.0f / 0.0f, -1.0f / 0.0f, 1.0e30f, -1.0e30f };
                for (float v: odd)
                {
                    roll->addNote(60, v, 1.0f);
                    roll->addNote(61, 1.0f, v);
                }
                if (!s.settle("the notes")) return;
            } },
};

void runScene(const SceneCase& c)
{
    Scene s(c.name);
    c.run(s);
    String text = String(c.name) + ": the frames stay finite and small";
    check(s.ok, text.c_str());
}

} // namespace

int main(int argc, char** argv)
{
    const char* only = nullptr;
    bool list = false;
    for (int i = 1; i < argc; ++i)
    {
        if (!strncmp(argv[i], "--only=", 7)) only = argv[i] + 7;
        else if (!strcmp(argv[i], "--list")) list = true;
    }
    for (const HostileCase& c: kHostile)
    {
        if (list) printf("%s\n", c.name);
        else if (!only || strstr(c.name, only)) runHostile(c);
    }
    for (const SceneCase& c: kScenes)
    {
        if (list) printf("%s\n", c.name);
        else if (!only || strstr(c.name, only)) runScene(c);
    }
    if (list) return 0;
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
