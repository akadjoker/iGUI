// Round 5 (docs/ROUND5.md): the piano roll's note length with the mouse and its velocity lane, hostile inputs for that
// new code, the tool bar in the serializer, the scroll bar thickness from the theme. Headless: the real frame loop
// (tests/RetainedHarness.hpp), the synthetic input of WidgetApp (feedMouseMove/Button/Wheel/Key/Text) where a mouse is
// involved, pixels of the software rasteriser where a picture is the point. No standard library: ct and C only.

#include "RasterScreen.hpp"
#include "RetainedHarness.hpp"

#include <unistd.h>

#include <igui/widgets/AudioWidgets.hpp>
#include <igui/widgets/ConsoleWidget.hpp>
#include <igui/widgets/DataWidgets.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/ScrollWidgets.hpp>
#include <igui/widgets/TextInputWidgets.hpp>
#include <igui/widgets/Theme.hpp>
#include <igui/widgets/ThumbnailGrid.hpp>
#include <igui/widgets/Timeline.hpp>
#include <igui/widgets/ToolBar.hpp>
#include <igui/widgets/TreePropertyColorWidgets.hpp>
#include <igui/widgets/WidgetSerializer.hpp>

using namespace rh;

// ---- the ceiling on what a frame may ask of the heap (the hostile cases, below) ----
//
// The heap requests of a frame are counted through the wrapped malloc, calloc and realloc (ct allocates with them; the
// link options of the target say so) and a frame of a hostile case that asks for more than kMaxFrameBytes stops the
// program there, naming the case: a loop that does not stop or a size that is not bounded fails here, small and fast,
// and never by taking the memory of the machine. Counting is on only while a hostile frame is being made.

namespace
{

constexpr unsigned long long kMaxFrameBytes = 64ull << 20;
unsigned long long gAskedBytes = 0;
bool gCounting = false;
const char* gCurrentCase = "(none)";

[[noreturn]] void frameAskedTooMuch(unsigned long long bytes)
{
    gCounting = false;
    char text[256];
    const int n = snprintf(text, sizeof text, "\n    FAIL %s: one frame asked the heap for more than %llu MiB (%llu so far): a loop that does not stop\n",
            gCurrentCase, kMaxFrameBytes >> 20, bytes >> 20);
    if (n > 0)
    {
        const ssize_t written = write(2, text, static_cast<size_t>(n < static_cast<int>(sizeof text) ? n : static_cast<int>(sizeof text) - 1));
        (void) written;
    }
    _exit(3);
}

void asked(size_t bytes)
{
    if (!gCounting) return;
    gAskedBytes += bytes;
    if (gAskedBytes > kMaxFrameBytes) frameAskedTooMuch(gAskedBytes);
}

} // namespace

extern "C" void* __real_malloc(size_t);
extern "C" void* __real_calloc(size_t, size_t);
extern "C" void* __real_realloc(void*, size_t);
extern "C" void* __wrap_malloc(size_t n)
{
    asked(n);
    return __real_malloc(n);
}
extern "C" void* __wrap_calloc(size_t a, size_t b)
{
    asked(a * b);
    return __real_calloc(a, b);
}
extern "C" void* __wrap_realloc(void* p, size_t n)
{
    asked(n);
    return __real_realloc(p, n);
}

namespace
{

// ---- a piano roll in a window, and a mouse on it ----

// The roll of these tests: 400x200 at (20, 20) in a 480x260 window, 12 rows of 12 pixels (pitches 48 to 59, the top row
// is 59), the keys 36 pixels wide so the grid starts at x = 56 and 8 beats take 364 pixels, 45.5 a beat.
struct RollRig
{
    explicit RollRig(float width = 480.0f, float height = 260.0f)
            : screen(width, height),
              h(screen.h),
              app(WidgetApp::instance())
    {
        roll = h.root()->createChild<PianoRoll>();
        roll->setRect(Rect(20.0f, 20.0f, 400.0f, 200.0f));
        roll->setPitchRange(48, 60);
        roll->setTimeRange(0.0f, 8.0f);
        h.frame();
    }
    // pixels of a beat and of a pitch: the middle of its row
    static float xBeat(float beat)
    {
        return 56.0f + beat * 45.5f;
    }
    static float yPitch(int pitch)
    {
        return 20.0f + static_cast<float>(59 - pitch) * 12.0f + 6.0f;
    }
    void move(float x, float y)
    {
        app.feedMouseMove(x, y);
    }
    void down(int button = 0)
    {
        app.feedMouseButton(button, true);
    }
    void up(int button = 0)
    {
        app.feedMouseButton(button, false);
    }
    Screen screen;
    Harness& h;
    WidgetApp& app;
    PianoRoll* roll = nullptr;
};

// What the signals of a roll said.
struct SignalLog
{
    int added = 0, created = 0, moved = 0, resized = 0, deleted = 0, clicked = 0;
    int createdIndex = -1, movedIndex = -1, resizedIndex = -1;
    float lengthAtResized = -1.0f;
    void connect(PianoRoll* roll)
    {
        roll->onNoteAdded.connect([this](int, float, float) { ++added; });
        roll->onNoteCreated.connect([this](int i) { ++created; createdIndex = i; });
        roll->onNoteMoved.connect([this, roll](int i) { ++moved; movedIndex = i; (void) roll; });
        roll->onNoteResized.connect([this, roll](int i)
                {
                    ++resized;
                    resizedIndex = i;
                    if (i >= 0 && i < static_cast<int>(roll->notes().size())) lengthAtResized = roll->notes()[i].length;
                });
        roll->onNoteDeleted.connect([this](int, const PianoNote&) { ++deleted; });
        roll->onNoteClicked.connect([this](int) { ++clicked; });
    }
};

// whether v is a whole number of quarters of a beat
bool onTheGrid(float v)
{
    const float q = v * 4.0f;
    return q == floorf(q);
}

// ---- item 1: the length of a note with the mouse ----

void testPianoRollResizeWithTheMouse()
{
    RollRig rig;
    PianoRoll* roll = rig.roll;
    roll->addNote(55, 2.0f, 1.0f); // x from 147, 44.5 wide: the last pixels are the end to grab
    SignalLog log;
    log.connect(roll);
    const float y = RollRig::yPitch(55);
    const float xEdge = RollRig::xBeat(3.0f) - 2.5f; // 189: inside the last six pixels of the note
    const float xMiddle = RollRig::xBeat(2.5f);

    // the pointer over the end of a note is the horizontal resize cursor, over the rest of it and over nothing the arrow
    rig.move(xMiddle, y);
    check(rig.app.wantedCursor() == CursorType::Arrow, "over the middle of a note the cursor is the arrow");
    rig.move(xEdge, y);
    check(rig.app.wantedCursor() == CursorType::SizeH, "over the last pixels of a note it is the horizontal resize cursor");
    rig.move(RollRig::xBeat(6.0f), y);
    check(rig.app.wantedCursor() == CursorType::Arrow, "and off the note the arrow again");
    rig.move(xEdge, y - 12.0f);
    check(rig.app.wantedCursor() == CursorType::Arrow, "a pointer in the row above the note is not on its end");

    // drag the end two beats to the right: the end goes to beat 4, the note keeps its pitch and its start
    rig.move(xEdge, y);
    rig.down();
    check(log.clicked == 1, "the press on the end of a note says it was clicked, as a press on the note always did");
    rig.move(RollRig::xBeat(4.0f) + 1.0f, y);
    check(roll->notes()[0].length == 2.0f, "the length follows the pointer while the button is down (2 beats: the end is at beat 4)");
    check(rig.app.wantedCursor() == CursorType::SizeH, "and the cursor stays the resize one during the drag");
    check(log.resized == 0 && log.moved == 0 && log.added == 0, "(nothing is said while the button is down)");
    rig.up();
    check(roll->notes().size() == 1 && roll->notes()[0].pitch == 55 && roll->notes()[0].start == 2.0f, "the note is where it was");
    check(roll->notes()[0].length == 2.0f, "and 2 beats long");
    check(log.resized == 1 && log.resizedIndex == 0 && log.lengthAtResized == 2.0f, "onNoteResized says which note, once, with the new length already in notes()");
    check(log.moved == 0 && log.created == 0, "a resize is not a move and not a creation");
    check(log.added == 1, "onNoteAdded is emitted as it always was after a drag of a note");

    // the end goes to the nearest quarter of a beat, whatever the pointer does, and the length is never under a quarter;
    // grabbed at the end (beat 4), the end moves by the distance the pointer does
    const float xGrab = RollRig::xBeat(4.0f) - 2.5f;
    rig.move(xGrab, y);
    rig.down();
    rig.move(xGrab + 4.0f, y); // 0.09 of a beat
    check(roll->notes()[0].length == 2.0f, "a pointer a little over the grab point leaves the end on the nearest line, beat 4");
    rig.move(xGrab + 7.0f, y); // 0.15 of a beat: nearer to 4.25
    check(roll->notes()[0].length == 2.25f, "and past the middle of the step it is the next line (length 2.25)");
    rig.move(xGrab + 45.5f + 4.0f, y);
    check(roll->notes()[0].length == 3.0f, "a beat on and a little over, beat 5 (length 3)");
    rig.move(xGrab + 45.5f + 7.0f, y);
    check(roll->notes()[0].length == 3.25f, "and past the middle of the step, 5.25 (length 3.25)");
    bool allOnTheGrid = true, allAtLeastAQuarter = true;
    for (float x = 50.0f; x < 430.0f; x += 7.0f)
    {
        rig.move(x, y);
        const float len = roll->notes()[0].length;
        if (!onTheGrid(len) || !onTheGrid(roll->notes()[0].start + len)) allOnTheGrid = false;
        if (!(len >= 0.25f)) allAtLeastAQuarter = false;
    }
    check(allOnTheGrid, "the end is on a quarter of a beat at every step of a drag across the roll");
    check(allAtLeastAQuarter, "and the length is never under one quarter");
    rig.move(RollRig::xBeat(0.0f) + 4.0f, y); // far to the left of the start
    check(roll->notes()[0].length == 0.25f, "a drag to the left of the start leaves the minimum, a quarter of a beat");
    check(roll->notes()[0].start == 2.0f, "(the start never moves)");
    rig.move(xGrab + 45.5f + 7.0f, y);
    log = SignalLog();
    rig.up();
    check(log.resized == 1 && roll->notes()[0].length == 3.25f, "(the button goes up on a changed length: one onNoteResized, with the length of the last move)");

    // a press on the end and a release without moving does not change the note, and says nothing about a resize
    const float before = roll->notes()[0].length;
    const float xEnd = RollRig::xBeat(2.0f + before) - 3.0f;
    log = SignalLog();
    rig.move(xEnd, y);
    rig.down();
    rig.up();
    check(roll->notes()[0].length == before && roll->notes()[0].start == 2.0f, "a click on the end of a note leaves it as it is");
    check(log.resized == 0 && log.moved == 0 && log.added == 1 && log.clicked == 1, "and it says it was clicked, added as before, and no resize");
    // a drag that comes back to where it was is not a resize either
    log = SignalLog();
    rig.move(xEnd, y);
    rig.down();
    rig.move(xEnd + 60.0f, y);
    rig.move(xEnd + 1.0f, y);
    rig.up();
    check(roll->notes()[0].length == before && log.resized == 0, "a drag that ends with the length it started with says nothing about a resize");

    // a press a few pixels from the end moves the note, as it always did
    log = SignalLog();
    rig.move(RollRig::xBeat(2.5f) + 1.0f, y);
    rig.down();
    rig.move(RollRig::xBeat(3.5f) + 6.0f, y);
    rig.up();
    check(roll->notes()[0].start == 3.0f && roll->notes()[0].length == before, "a drag from the middle of the note moves it (start 3) and does not change its length");
    check(log.moved == 1 && log.resized == 0, "onNoteMoved, not onNoteResized");

    // a short note keeps most of its width for moving
    roll->clearNotes();
    roll->addNote(50, 1.0f, 0.05f); // drawn at the minimum width, 4 pixels
    const float xShort = RollRig::xBeat(1.0f);
    log = SignalLog();
    rig.move(xShort + 1.0f, RollRig::yPitch(50));
    rig.down();
    rig.move(xShort + 1.0f + 2.0f * 45.5f + 5.0f, RollRig::yPitch(50));
    rig.up();
    check(log.moved == 1 && log.resized == 0 && roll->notes()[0].start == 3.0f, "the left of a very short note moves it");
    check(roll->notes()[0].length == 0.05f, "(and does not touch its length)");
    // a note shorter than the minimum is made to be the minimum by a resize
    log = SignalLog();
    const float xShortEnd = RollRig::xBeat(3.0f) + 3.5f; // the note is 4 pixels wide: the end zone is the last one and a third
    rig.move(xShortEnd, RollRig::yPitch(50));
    rig.down();
    rig.move(xShortEnd + 3.0f, RollRig::yPitch(50));
    rig.up();
    check(log.resized == 1 && roll->notes()[0].length >= 0.25f, "grabbed by its end, a note shorter than a quarter becomes a quarter long");
}

void testPianoRollNewNoteTakesItsLengthFromTheDrag()
{
    RollRig rig;
    PianoRoll* roll = rig.roll;
    SignalLog log;
    log.connect(roll);
    const float y = RollRig::yPitch(55);

    // a click with no drag: today's length, a quarter of a beat
    rig.move(RollRig::xBeat(2.0f) + 3.0f, y);
    rig.down();
    rig.up();
    check(roll->notes().size() == 1 && roll->notes()[0].length == 0.25f && roll->notes()[0].start == 2.0f, "a click on an empty place puts a note a quarter of a beat long");
    check(log.created == 1 && log.createdIndex == 0 && log.resized == 0, "onNoteCreated, and no resize");
    roll->clearNotes();

    // a drag: the length comes from it, the start and the pitch stay where the button went down
    log = SignalLog();
    rig.move(RollRig::xBeat(2.0f) + 3.0f, y);
    rig.down();
    check(roll->notes().size() == 1, "(a note is put at the press)");
    rig.move(RollRig::xBeat(4.0f) + 3.0f, y);
    check(roll->notes().size() == 1 && roll->notes()[0].length == 2.0f, "dragging to beat 4 gives the new note 2 beats");
    check(rig.app.wantedCursor() == CursorType::SizeH, "and the cursor is the resize one: the rest of the drag sets its length");
    rig.move(RollRig::xBeat(6.0f) + 3.0f, y - 24.0f);
    check(roll->notes()[0].pitch == 55 && roll->notes()[0].start == 2.0f, "dragging up and right does not move it: pitch and start stay");
    check(roll->notes()[0].length == 4.0f, "(the length is 4: the end is at beat 6)");
    rig.move(RollRig::xBeat(1.0f), y);
    check(roll->notes()[0].length == 0.25f && roll->notes()[0].start == 2.0f, "dragging to the left of the start leaves a quarter, never a negative length");
    rig.move(RollRig::xBeat(3.0f) + 3.0f, y);
    check(log.created == 0, "(nothing is said while the button is down)");
    rig.up();
    check(roll->notes().size() == 1 && roll->notes()[0].length == 1.0f && roll->notes()[0].start == 2.0f && roll->notes()[0].pitch == 55,
            "the note ends as the drag left it: beat 2, pitch 55, 1 beat");
    check(log.created == 1 && log.createdIndex == 0 && log.resized == 0 && log.moved == 0 && log.added == 1,
            "onNoteCreated once, with the length the drag gave it; no resize, no move");
    check(roll->notes()[0].velocity == 0.8f, "with the default velocity");

    // the pointer past the right edge of the grid: the length is bounded and a number
    roll->clearNotes();
    rig.move(RollRig::xBeat(2.0f) + 3.0f, y);
    rig.down();
    rig.move(470.0f, y);
    check(roll->notes()[0].length > 0.25f && roll->notes()[0].length < 100.0f && onTheGrid(roll->notes()[0].length), "a pointer outside the roll gives a length on the grid and not a wild one");
    rig.up();

    // a second note to see the index is the new note's
    log = SignalLog();
    rig.move(RollRig::xBeat(5.0f) + 3.0f, RollRig::yPitch(52));
    rig.down();
    rig.move(RollRig::xBeat(6.0f) + 3.0f, RollRig::yPitch(52));
    rig.up();
    check(roll->notes().size() == 2 && log.createdIndex == 1 && roll->notes()[1].length == 1.0f, "the second note is index 1, 1 beat long");
}

void testPianoRollNoteIndexStaysRightWhenANoteBeforeItGoes()
{
    // a note removed from the list during a drag (by the application, from a timer, a signal...) must not make the drag
    // go on with another note
    RollRig rig;
    PianoRoll* roll = rig.roll;
    roll->addNote(50, 0.0f, 1.0f);
    roll->addNote(55, 2.0f, 1.0f);
    const float y = RollRig::yPitch(55);
    rig.move(RollRig::xBeat(3.0f) - 2.5f, y);
    rig.down(); // the end of the second note
    roll->removeNote(0);
    check(roll->notes().size() == 1 && roll->notes()[0].pitch == 55, "(the first note is gone, the dragged one is index 0 now)");
    rig.move(RollRig::xBeat(4.0f) + 1.0f, y);
    check(roll->notes()[0].length == 2.0f, "the drag goes on with the same note");
    SignalLog log;
    log.connect(roll);
    rig.up();
    check(log.resized == 1 && log.resizedIndex == 0, "and says index 0");
}

// ---- item 2: the velocity lane ----

// With the default lane (60 pixels) in this roll: it begins at y = 160, the feet of the bars are at y = 218 and a velocity of 1 is
// 54 pixels of bar, so the pointer at y = 218 - 54 v sets velocity v.
float yVelocity(float v)
{
    return 218.0f - 54.0f * v;
}

struct VelocityLog
{
    int changed[16] = {};
    int total = 0;
    int order[16] = {};
    void connect(PianoRoll* roll)
    {
        roll->onNoteVelocityChanged.connect([this](int i)
                {
                    if (i >= 0 && i < 16) ++changed[i];
                    if (total < 16) order[total] = i;
                    ++total;
                });
    }
    void reset()
    {
        for (int& c: changed) c = 0;
        total = 0;
    }
};

bool near(float a, float b, float tolerance)
{
    return fabsf(a - b) <= tolerance;
}

void testPianoRollVelocityLaneSetsVelocities()
{
    RollRig rig;
    PianoRoll* roll = rig.roll;
    check(!roll->showVelocityLane(), "the velocity lane is off by default");
    check(roll->velocityLaneHeight() == 60.0f, "and 60 pixels high by default");
    const float beats[5] = { 1.0f, 2.0f, 3.0f, 4.0f, 6.0f };
    for (int i = 0; i < 5; ++i) roll->addNote(50 + i, beats[i], 0.5f); // velocity 0.8
    roll->setShowVelocityLane(true);
    check(roll->showVelocityLane(), "setShowVelocityLane(true) is kept");
    rig.h.frame();
    VelocityLog log;
    log.connect(roll);
    SignalLog signals;
    signals.connect(roll);

    // a click at half the height of the lane, on the bar of the second note
    rig.move(RollRig::xBeat(2.0f), yVelocity(0.5f));
    rig.down();
    check(roll->notes()[1].velocity == 0.5f, "pressing in the lane at half height sets the velocity of the note whose bar is there to 0.5");
    check(roll->notes()[0].velocity == 0.8f && roll->notes()[2].velocity == 0.8f && roll->notes()[3].velocity == 0.8f && roll->notes()[4].velocity == 0.8f,
            "and no other note's");
    check(log.total == 0, "nothing is emitted while the button is down");
    rig.up();
    check(log.total == 1 && log.changed[1] == 1, "onNoteVelocityChanged says note 1, once, when the button goes up");
    check(roll->notes().size() == 5 && signals.created == 0 && signals.clicked == 0 && signals.added == 0, "a press in the lane puts no note, picks none and emits none of the note signals");

    // the same click again changes nothing: no signal
    log.reset();
    rig.move(RollRig::xBeat(2.0f), yVelocity(0.5f));
    rig.down();
    rig.up();
    check(log.total == 0, "a click that leaves the velocity as it is emits nothing");

    // the top of the lane is 1, the bottom is 0, and outside is the nearest of them
    log.reset();
    rig.move(RollRig::xBeat(3.0f), 161.0f);
    rig.down();
    check(roll->notes()[2].velocity == 1.0f, "at the top of the lane the velocity is 1");
    rig.move(RollRig::xBeat(3.0f), 400.0f); // below the window
    check(roll->notes()[2].velocity == 0.0f, "a pointer far under the lane sets 0");
    rig.move(RollRig::xBeat(3.0f), -400.0f);
    check(roll->notes()[2].velocity == 1.0f, "and far over it 1");
    rig.move(RollRig::xBeat(3.0f), yVelocity(0.25f));
    rig.up();
    check(roll->notes()[2].velocity == 0.25f && log.total == 1 && log.changed[2] == 1, "the gesture ends with the last value, and the note is said once however many times it was set");
    // a gesture that goes back to the velocity the note had says nothing
    log.reset();
    rig.move(RollRig::xBeat(3.0f), yVelocity(0.25f));
    rig.down();
    rig.move(RollRig::xBeat(3.0f), yVelocity(0.9f));
    rig.move(RollRig::xBeat(3.0f), yVelocity(0.25f));
    rig.up();
    check(log.total == 0, "a gesture that ends with the velocity it started with says nothing");

    // a chord: two notes with the same start are both under the pointer
    roll->addNote(57, 2.0f, 1.0f); // index 5, the same start as note 1
    log.reset();
    rig.move(RollRig::xBeat(2.0f), yVelocity(1.0f));
    rig.down();
    rig.up();
    check(roll->notes()[1].velocity == 1.0f && roll->notes()[5].velocity == 1.0f, "notes that start together are both set");
    check(log.total == 2 && log.changed[1] == 1 && log.changed[5] == 1, "and both are said");
    check(log.order[0] == 5 && log.order[1] == 1, "from the highest index to the lowest");

    // the right and the middle buttons: the right one does nothing in the lane (it is not a note), the middle one pans
    signals = SignalLog();
    log.reset();
    const size_t count = roll->notes().size();
    rig.move(RollRig::xBeat(2.0f), yVelocity(0.3f));
    rig.down(1);
    rig.up(1);
    check(roll->notes().size() == count && signals.deleted == 0 && roll->notes()[1].velocity == 1.0f && log.total == 0, "the right button in the lane removes nothing and sets nothing");
    rig.move(RollRig::xBeat(2.0f), 190.0f);
    rig.down(2);
    rig.move(RollRig::xBeat(2.0f) - 91.0f, 190.0f); // two beats to the left: the view moves two beats on
    rig.up(2);
    // the note that started at beat 4 is now where the one at beat 2 was
    rig.move(RollRig::xBeat(2.0f), yVelocity(0.4f));
    rig.down();
    rig.up();
    check(near(roll->notes()[3].velocity, 0.4f, 0.0001f) && roll->notes()[1].velocity == 1.0f, "the middle button pans in the lane, and the bars are where the roll shows them after");
}

void testPianoRollVelocityLanePaintsAcrossBars()
{
    RollRig rig;
    PianoRoll* roll = rig.roll;
    const float beats[6] = { 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 7.0f };
    for (int i = 0; i < 6; ++i) roll->addNote(50 + i, beats[i], 0.5f);
    roll->setShowVelocityLane(true);
    rig.h.frame();
    VelocityLog log;
    log.connect(roll);
    // from beat 1 at 0.2 to beat 6 at 0.9 along a straight line, in many small steps of the mouse
    const float x0 = RollRig::xBeat(1.0f), x1 = RollRig::xBeat(6.0f);
    rig.move(x0, yVelocity(0.2f));
    rig.down();
    float lastThird = 0.0f;
    for (int step = 1; step <= 40; ++step)
    {
        const float t = static_cast<float>(step) / 40.0f;
        rig.move(x0 + (x1 - x0) * t, yVelocity(0.2f + 0.7f * t));
        if (step == 20) lastThird = roll->notes()[2].velocity; // beat 3.5 has been passed: beat 3 is set
    }
    check(near(lastThird, 0.2f + 0.7f * 0.4f, 0.02f), "while the button is down the notes passed already have their velocity (note 2 at beat 3: 0.48)");
    check(log.total == 0, "and nothing is emitted");
    rig.up();
    const float want[5] = { 0.2f, 0.34f, 0.48f, 0.62f, 0.9f };
    bool right = true;
    for (int i = 0; i < 5; ++i)
        if (!near(roll->notes()[i].velocity, want[i], 0.02f)) right = false;
    check(right, "a drag across five bars gives each the height the pointer had at its place (0.2, 0.34, 0.48, 0.62, 0.9)");
    if (!right)
        for (int i = 0; i < 5; ++i) printf("    note %d: %g (wanted %g)\n", i, static_cast<double>(roll->notes()[i].velocity), static_cast<double>(want[i]));
    check(roll->notes()[5].velocity == 0.8f, "a note past the end of the path is left alone");
    check(log.total == 5 && log.changed[0] == 1 && log.changed[1] == 1 && log.changed[2] == 1 && log.changed[3] == 1 && log.changed[4] == 1 && log.changed[5] == 0,
            "each of the five is said once, the sixth not");
    check(log.order[0] == 4 && log.order[4] == 0, "from the highest index down");

    // one big jump of the mouse (two events only) does not skip the bars between: the path is followed
    for (int i = 0; i < 6; ++i) roll->setNoteVelocity(i, 0.8f);
    log.reset();
    rig.move(x0, yVelocity(0.0f));
    rig.down();
    rig.move(x1, yVelocity(1.0f));
    rig.up();
    bool ramp = true;
    for (int i = 0; i < 5; ++i)
    {
        const float expect = (RollRig::xBeat(beats[i]) - x0) / (x1 - x0);
        if (!near(roll->notes()[i].velocity, expect, 0.02f)) ramp = false;
    }
    check(ramp, "one fast move from the first bar to the fifth still sets the three between, on the line the pointer went along");
    check(log.total == 5, "(five notes said)");

    // a drag from outside the roll, over the key column, sets only what is in view
    for (int i = 0; i < 6; ++i) roll->setNoteVelocity(i, 0.8f);
    roll->setTimeRange(2.0f, 6.0f); // bars of beats 2, 3, 4 and 6 at x = 56, 147, 238, 420
    rig.h.frame();
    log.reset();
    rig.move(30.0f, yVelocity(0.5f)); // over the keys: not the lane
    rig.down();
    check(roll->notes()[0].velocity == 0.8f && roll->notes()[1].velocity == 0.8f, "(the press over the key column is nothing)");
    rig.up();
    rig.move(150.0f, yVelocity(0.5f));
    rig.down();
    rig.move(10.0f, yVelocity(0.5f)); // out over the keys, far left of the bars in view and the one off the left edge
    rig.up();
    check(roll->notes()[0].velocity == 0.8f, "a pointer dragged out to the left does not reach the bar of a note scrolled out of view (beat 1)");
    check(roll->notes()[1].velocity == 0.5f && roll->notes()[2].velocity == 0.5f, "the bars in view on the way are set (beats 2 and 3)");
}

// the x of the centre of each run of pixels that is not the background, along one row of a shot
ct::Vector<float> barCentres(const Shot& shot, int y, int x0, int x1)
{
    ct::Vector<float> out;
    int runStart = -1;
    for (int x = x0; x <= x1; ++x)
    {
        bool bar = false;
        if (x < x1)
        {
            const uint32_t v = shot.px[static_cast<size_t>(y) * static_cast<size_t>(shot.w) + static_cast<size_t>(x)];
            bar = (v & 0xffffffu) != 0x14161au; // (20, 22, 26): the lane
        }
        if (bar && runStart < 0) runStart = x;
        else if (!bar && runStart >= 0)
        {
            out.push_back(0.5f * static_cast<float>(runStart + x - 1) + 0.5f);
            runStart = -1;
        }
    }
    return out;
}

// the first row from y0 down where the column at x is not the lane's background
int firstBarRow(const Shot& shot, int x, int y0, int y1)
{
    for (int y = y0; y < y1; ++y)
        if ((shot.px[static_cast<size_t>(y) * static_cast<size_t>(shot.w) + static_cast<size_t>(x)] & 0xffffffu) != 0x14161au) return y;
    return -1;
}

void testPianoRollVelocityLaneDrawsOneBarPerNote()
{
    RollRig rig;
    PianoRoll* roll = rig.roll;
    roll->addNote(55, 2.0f, 2.0f, 1.0f);
    roll->addNote(52, 4.0f, 1.0f, 0.5f);
    rig.h.frame();
    const Shot plain = rig.screen.take();
    roll->setShowVelocityLane(true);
    rig.h.frames(2);
    const Shot lane = rig.screen.take();
    check(differing(lane, plain, 0, 0, 480, 260) > 500, "the lane is drawn when it is on");
    check(differing(lane, plain, 0, 0, 480, 150) < differing(lane, plain, 0, 150, 480, 260), "mostly in the strip along the bottom (and not over the notes)");
    const ct::Vector<float> bars = barCentres(lane, 217, 60, 419);
    check(bars.size() == 2, "one bar for each note at the foot of the lane");
    if (bars.size() == 2)
    {
        check(near(bars[0], RollRig::xBeat(2.0f), 1.0f) && near(bars[1], RollRig::xBeat(4.0f), 1.0f), "at the start of each note (beats 2 and 4)");
        const int tall = firstBarRow(lane, static_cast<int>(bars[0]), 162, 219);
        const int half = firstBarRow(lane, static_cast<int>(bars[1]), 162, 219);
        check(tall >= 163 && tall <= 165, "the bar of a note of velocity 1 reaches the top of the lane");
        check(half >= 190 && half <= 192, "and the bar of a note of velocity 0.5 half way up");
    }
    // the bars follow the velocity
    roll->setNoteVelocity(1, 1.0f);
    rig.h.frames(2);
    const Shot taller = rig.screen.take();
    const ct::Vector<float> again = barCentres(taller, 217, 60, 419);
    check(again.size() == 2 && firstBarRow(taller, static_cast<int>(again[1]), 162, 219) <= 165, "the bar follows the velocity of its note");
    // the lowest row of notes does not draw over the lane
    roll->addNote(48, 3.0f, 2.0f);
    rig.h.frames(2);
    const Shot low = rig.screen.take();
    const uint32_t under = low.px[static_cast<size_t>(162) * static_cast<size_t>(low.w) + static_cast<size_t>(RollRig::xBeat(3.5f))] & 0xffffffu;
    check(under == 0x14161au, "a note in the lowest row, half under the lane, is cut at the lane");
    // off again: the picture of before
    roll->clearNotes();
    roll->addNote(55, 2.0f, 2.0f, 1.0f);
    roll->addNote(52, 4.0f, 1.0f, 0.5f);
    roll->setShowVelocityLane(false);
    rig.h.frames(2);
    check(differing(rig.screen.take(), plain, 0, 0, 480, 260) == 0, "with the lane off again the picture is the one it was before");

    // while a gesture is in progress the bars of the notes it has touched are drawn in the colour of a dragged note
    roll->setShowVelocityLane(true);
    rig.h.frames(2);
    const Shot before = rig.screen.take();
    rig.move(RollRig::xBeat(2.0f), yVelocity(0.2f));
    rig.down();
    rig.h.frames(2);
    const Shot during = rig.screen.take();
    rig.up();
    check(differing(during, before, 60, 162, 420, 219) > 20, "a bar being set is drawn changed");
}

void testPianoRollVelocityLaneFollowsTheView()
{
    RollRig rig;
    PianoRoll* roll = rig.roll;
    const float beats[4] = { 2.0f, 3.0f, 4.0f, 5.0f };
    for (int i = 0; i < 4; ++i) roll->addNote(50 + i, beats[i], 0.5f);
    roll->setShowVelocityLane(true);
    rig.h.frame();

    // scrolled: beats 3 to 7, 91 pixels a beat... no, 4 beats to 364 pixels
    roll->setTimeRange(3.0f, 7.0f);
    rig.h.frames(2);
    // the bar of the note at beat 4 is one beat in: x = 56 + 91
    rig.move(56.0f + 91.0f, yVelocity(0.5f));
    rig.down();
    rig.up();
    check(roll->notes()[2].velocity == 0.5f && roll->notes()[1].velocity == 0.8f && roll->notes()[0].velocity == 0.8f && roll->notes()[3].velocity == 0.8f,
            "after the view is scrolled the bar of the note at beat 4 is one beat in from the left, and that is the one a click there sets");

    // zoomed with the wheel: look at where the bars are drawn, and click on them
    roll->setTimeRange(0.0f, 8.0f);
    for (int i = 0; i < 4; ++i) roll->setNoteVelocity(i, 0.8f);
    rig.move(250.0f, 100.0f);
    rig.app.feedMouseWheel(0.0f, 1.0f); // zoom in around the pointer
    rig.h.frames(2);
    const Shot zoomed = rig.screen.take();
    const ct::Vector<float> bars = barCentres(zoomed, 217, 60, 419);
    check(bars.size() >= 2 && bars.size() <= 4, "after a zoom the bars of the notes in view are where the roll puts them");
    int setByClick = 0;
    for (size_t k = 0; k < bars.size(); ++k)
    {
        rig.move(bars[k], yVelocity(0.1f));
        rig.down();
        rig.up();
    }
    for (int i = 0; i < 4; ++i)
        if (near(roll->notes()[i].velocity, 0.1f, 0.0001f)) ++setByClick;
    check(static_cast<size_t>(setByClick) == bars.size(), "and a click on each drawn bar sets one note: what is drawn is what is hit");
}

void testPianoRollVelocityLaneHeightAndHitAreas()
{
    RollRig rig;
    PianoRoll* roll = rig.roll;
    roll->addNote(50, 2.0f, 1.0f);
    roll->addNote(48, 4.0f, 1.0f); // the lowest row, y 153 to 163: its last pixels are under the lane
    roll->setShowVelocityLane(true);
    rig.h.frame();
    SignalLog signals;
    signals.connect(roll);

    // a press over the lane's top edge (y = 160) is the lane's, not the note's
    rig.move(RollRig::xBeat(4.0f) + 10.0f, 162.0f);
    rig.down();
    rig.up();
    check(signals.clicked == 0 && signals.added == 0, "a press on the part of a note that is under the lane does not pick the note");
    rig.move(RollRig::xBeat(4.0f) + 10.0f, 156.0f);
    rig.down();
    rig.up();
    check(signals.clicked == 1, "and one over the lane does");
    // a press on an empty place in the lane puts no note; the same place without the lane does
    const size_t count = roll->notes().size();
    rig.move(RollRig::xBeat(6.0f), 190.0f);
    rig.down();
    rig.up();
    check(roll->notes().size() == count, "with the lane on a left click in it puts no note");
    roll->setShowVelocityLane(false);
    rig.h.frame();
    rig.move(RollRig::xBeat(6.0f), 190.0f);
    rig.down();
    rig.up();
    check(roll->notes().size() == count + 1, "with the lane off the same click puts one: the lane takes nothing when it is off");
    roll->removeNote(static_cast<int>(roll->notes().size()) - 1);
    roll->setShowVelocityLane(true);

    // the height
    roll->setVelocityLaneHeight(100.0f);
    check(roll->velocityLaneHeight() == 100.0f, "the height is kept");
    roll->setVelocityLaneHeight(NAN);
    check(roll->velocityLaneHeight() == 100.0f, "NaN keeps the old one");
    rig.h.frame();
    // lane from y = 120 now, feet at 218, 94 pixels of velocity
    rig.move(RollRig::xBeat(2.0f), 218.0f - 47.0f);
    rig.down();
    rig.up();
    check(near(roll->notes()[0].velocity, 0.5f, 0.0001f), "with a lane of 100 pixels the middle of it is 0.5");
    roll->setVelocityLaneHeight(1.0e9f);
    check(roll->velocityLaneHeight() == 10000.0f, "an enormous height is clamped");
    roll->setVelocityLaneHeight(-5.0f);
    check(roll->velocityLaneHeight() == 10.0f, "a height under 10 is 10");
    roll->setVelocityLaneHeight(10000.0f);
    rig.h.frame();
    // never more than half of the roll: from y = 120 whatever is asked
    rig.move(RollRig::xBeat(2.0f), 218.0f - 47.0f);
    rig.down();
    rig.up();
    check(near(roll->notes()[0].velocity, 0.5f, 0.0001f), "however high it is asked, the lane is half of the roll at most");
}

// ---- item 4: the tool bar in the serializer ----

// whether two bars have the same flags, sizes and items, in the same order
bool sameToolBar(const ToolBar& a, const ToolBar& b, const char* what)
{
    bool same = true;
    auto differ = [&](bool equal, const char* field)
    {
        if (!equal)
        {
            printf("    %s: %s differs\n", what, field);
            same = false;
        }
    };
    differ(a.coloredIcons() == b.coloredIcons(), "coloredIcons");
    differ(a.bottomBorder() == b.bottomBorder(), "bottomBorder");
    differ(a.isDraggable() == b.isDraggable(), "draggable");
    differ(a.buttonSize() == b.buttonSize(), "buttonSize");
    differ(a.iconPadding() == b.iconPadding(), "iconPadding");
    differ(a.spacing() == b.spacing(), "spacing");
    differ(a.itemCount() == b.itemCount(), "itemCount");
    for (int i = 0; i < a.itemCount() && i < b.itemCount(); ++i)
    {
        const int x = a.itemIdAt(i), y = b.itemIdAt(i);
        differ(a.itemType(x) == b.itemType(y), "item type");
        differ(a.itemCell(x) == b.itemCell(y), "item cell");
        differ(a.itemIcon(x) == b.itemIcon(y), "item icon");
        differ(a.itemTooltip(x) == b.itemTooltip(y), "item tooltip");
        differ(a.isItemEnabled(x) == b.isItemEnabled(y), "item enabled");
        differ(a.isItemVisible(x) == b.isItemVisible(y), "item visible");
        differ(a.isItemChecked(x) == b.isItemChecked(y), "item checked");
        differ(a.isItemDraggable(x) == b.isItemDraggable(y), "item draggable");
    }
    return same;
}

ToolBar* toolBarOf(Widget* loaded)
{
    return loaded && !loaded->children().empty() ? widget_cast<ToolBar>(loaded->children()[0]) : nullptr;
}

void testSerializerKeepsToolBar()
{
    WidgetSerializer::registerBuiltinTypes();
    BoxLayout root(LayoutDir::Vertical);
    ToolBar* bar = root.createChild<ToolBar>();
    bar->setColoredIcons(true);
    bar->setBottomBorder(false);
    bar->setButtonSize(32.0f);
    bar->setIconPadding(6.0f);
    bar->setSpacing(3.0f);
    bar->setDraggable(false);
    const int gear = bar->addButton(IconId::Gear, "Settings");
    bar->addToggle(IconId::Folder, "Folder", true);
    bar->addSeparator();
    const int cell = bar->addButton(3, "Cell three");
    bar->addSpacer();
    const int file = bar->addToggle(IconId::File, "File");
    const int cellToggle = bar->addToggle(7, "", false);
    bar->setItemEnabled(gear, false);
    bar->setItemVisible(file, false);
    bar->setItemDraggable(cell, false);
    bar->setItemChecked(cellToggle, true);

    const json saved = WidgetSerializer::save(&root);
    check(String(saved["children"][0]["type"].str()) == String("ToolBar"), "a tool bar is saved as a ToolBar (it was a plain Widget, with nothing of its own)");
    check(saved["children"][0]["coloredIcons"].as_bool(false), "the saved bar says its icons are coloured");
    check(saved["children"][0].contains("bottomBorder") && !saved["children"][0]["bottomBorder"].as_bool(true), "and that it has no bottom border");
    BoxLayout destination(LayoutDir::Vertical);
    Widget* loaded = WidgetSerializer::load(saved, &destination);
    ToolBar* copy = toolBarOf(loaded);
    check(copy != nullptr, "the bar is loaded as a ToolBar");
    if (!copy) return;
    check(copy->coloredIcons() && !copy->bottomBorder(), "the copy has setColoredIcons(true) and setBottomBorder(false)");
    check(sameToolBar(*bar, *copy, "round trip"), "and the sizes, the flags and every item (type, cell or icon, tooltip, enabled, visible, checked, draggable) come back, in order");
    check(copy->itemCount() == 7 && copy->itemTooltip(copy->itemIdAt(3)) == String("Cell three") && copy->itemCell(copy->itemIdAt(3)) == 3,
            "(seven items; the fourth is the button of cell 3)");
    check(copy->itemIdAt(0) == 1 && copy->itemIdAt(6) == 7, "the ids are 1 to 7 in the order of the bar");

    // saved again, it is the same file
    const json again = WidgetSerializer::save(loaded);
    check(again.dump(0) == saved.dump(0), "a bar saved after it was loaded gives the same file");

    // the defaults are not written, and load as the defaults
    BoxLayout plainRoot(LayoutDir::Vertical);
    ToolBar* plain = plainRoot.createChild<ToolBar>();
    plain->addButton(IconId::Gear, "One");
    const json plainJson = WidgetSerializer::save(&plainRoot);
    const json& pj = plainJson["children"][0];
    check(!pj.contains("coloredIcons") && !pj.contains("bottomBorder") && !pj.contains("draggable"), "a bar with the default flags writes none of them");
    BoxLayout plainDest(LayoutDir::Vertical);
    ToolBar* plainCopy = toolBarOf(WidgetSerializer::load(plainJson, &plainDest));
    check(plainCopy && sameToolBar(*plain, *plainCopy, "defaults") && !plainCopy->coloredIcons() && plainCopy->bottomBorder(), "and loads with them");
}

void testSerializerKeepsTheOrderOfAToolBarAfterADrag()
{
    // the items are saved in the order they are drawn, which a drag of an item changes
    Harness h(480.0f, 120.0f, false);
    ToolBar* bar = h.root()->createChild<ToolBar>();
    bar->setRect(Rect(0.0f, 0.0f, 300.0f, 34.0f));
    const int a = bar->addButton(IconId::Gear, "A");
    const int b = bar->addButton(IconId::Folder, "B");
    bar->addButton(IconId::File, "C");
    h.relayout();
    Rect ra, rb;
    check(bar->itemRect(a, ra) && bar->itemRect(b, rb), "(the items have rectangles)");
    WidgetApp& app = h.app;
    app.feedMouseMove(ra.x + ra.w * 0.5f, ra.y + ra.h * 0.5f);
    app.feedMouseButton(0, true);
    app.feedMouseMove(rb.x + rb.w * 0.5f + 6.0f, rb.y + rb.h * 0.5f);
    app.feedMouseMove(rb.x + rb.w * 0.5f + 12.0f, rb.y + rb.h * 0.5f);
    app.feedMouseButton(0, false);
    check(bar->itemTooltip(bar->itemIdAt(0)) == String("B") && bar->itemTooltip(bar->itemIdAt(1)) == String("A"), "(dragging A over B put B first)");
    WidgetSerializer::registerBuiltinTypes();
    const json saved = WidgetSerializer::save(h.root());
    BoxLayout destination(LayoutDir::Vertical);
    Widget* loaded = WidgetSerializer::load(saved, &destination);
    // the stage root is a plain widget: find the bar in what was loaded
    ToolBar* copy = nullptr;
    if (loaded)
        for (Widget* c: loaded->children())
            if (!copy) copy = widget_cast<ToolBar>(c);
    check(copy && copy->itemCount() == 3 && copy->itemTooltip(copy->itemIdAt(0)) == String("B") && copy->itemTooltip(copy->itemIdAt(1)) == String("A") &&
                    copy->itemTooltip(copy->itemIdAt(2)) == String("C"),
            "the file keeps B, A, C: the order they are drawn in");
}

void testToolBarLoadsWhatAFileCanHold()
{
    // a file made by hand: numbers that are not for a bar, icons that do not exist, items of no known kind
    WidgetSerializer::registerBuiltinTypes();
    const char* text = "{\"type\":\"BoxLayout\",\"children\":[{\"type\":\"ToolBar\",\"buttonSize\":1e39,\"iconPadding\":-1e39,\"spacing\":\"x\","
                       "\"items\":[{\"type\":\"Button\",\"icon\":99999,\"tooltip\":\"far\"},{\"type\":\"Toggle\",\"icon\":-4,\"checked\":true},"
                       "{\"type\":\"Whatever\"},{\"cell\":2},5,{\"type\":\"Separator\",\"tooltip\":\"ignored\",\"checked\":true},{\"type\":\"Spacer\"}]},"
                       "{\"type\":\"ToolBar\",\"items\":7}]}";
    json::Error error;
    const json file = json::parse(String(text), &error);
    check(!error, "(the hand-made file parses)");
    BoxLayout destination(LayoutDir::Vertical);
    Widget* loaded = WidgetSerializer::load(file, &destination);
    ToolBar* bar = toolBarOf(loaded);
    check(bar != nullptr, "the file loads");
    if (!bar) return;
    check(bar->buttonSize() >= 1.0f && bar->buttonSize() <= 1000.0f && bar->iconPadding() >= 0.0f && bar->iconPadding() <= 1000.0f && bar->spacing() >= 0.0f && bar->spacing() <= 1000.0f,
            "sizes that are infinite or not numbers are kept to what a bar can use");
    check(bar->itemCount() == 7, "every entry of the list is an item (the ones of no known kind are buttons)");
    check(bar->itemIcon(bar->itemIdAt(0)) == IconId::None && bar->itemIcon(bar->itemIdAt(1)) == IconId::None, "an icon that does not exist is no icon");
    check(bar->itemType(bar->itemIdAt(1)) == ToolBar::ItemType::Toggle && bar->isItemChecked(bar->itemIdAt(1)), "a toggle comes back checked");
    check(bar->itemCell(bar->itemIdAt(3)) == 2, "an entry with a cell is a button of that cell");
    check(bar->itemType(bar->itemIdAt(5)) == ToolBar::ItemType::Separator && !bar->isItemChecked(bar->itemIdAt(5)), "a separator takes none of the state of a button");
    ToolBar* second = loaded->children().size() > 1 ? widget_cast<ToolBar>(loaded->children()[1]) : nullptr;
    check(second && second->itemCount() == 0, "a list that is not a list is no items");
}

// ---- item 5: the scroll bar thickness comes from the theme ----

// The theme with another thickness for as long as this lives (the whole theme is put back, the way a test of themes does).
struct ThicknessScope
{
    explicit ThicknessScope(float thickness) : saved(Theme::instance())
    {
        Theme::instance().scrollbarWidth = thickness;
    }
    ~ThicknessScope()
    {
        Theme::instance() = saved;
    }
    Theme saved;
};

// A widget that asks for a size.
class Wants : public Widget
{
public:
    IG_WIDGET_CLASS(Wants, Widget);
    Wants(float w, float h) : w_(w), h_(h) {}
    Vec2f sizeHint() const override
    {
        return { w_, h_ };
    }
    float w_, h_;
};

ScrollBar* findBar(Widget* w, ScrollBarOrientation o)
{
    for (Widget* c: w->children())
        if (ScrollBar* bar = widget_cast<ScrollBar>(c))
            if (bar->orientation() == o) return bar;
    return nullptr;
}

// The grids of the grid tests, a window of 640x360 with one grid that stretches.
struct GridRig
{
    GridRig(bool tree_, int rows, int columns, float width = 640.0f, float height = 360.0f)
            : h(width, height, false),
              tree(tree_)
    {
        BoxLayout* box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
        box->setPadding(0.0f);
        if (tree)
        {
            tg = box->createChild<TreeGrid>();
            tg->setStretch(1.0f);
            for (int c = 0; c < columns; ++c) tg->addColumn(String("C") + String::number(c), 100.0f);
            tg->selectionChanged.connect([this](TreeGrid::Node* n) { selected = n && n->cells.size() > 0 ? atoi(n->cells[0].c_str()) : -1; });
            tg->setStretchLastColumn(true);
        }
        else
        {
            dg = box->createChild<DataGrid>();
            dg->setStretch(1.0f);
            for (int c = 0; c < columns; ++c) dg->addColumn(String("C") + String::number(c), 100.0f);
            dg->selectionChanged.connect([this](int row) { selected = row; });
            dg->setStretchLastColumn(true);
        }
        for (int r = 0; r < rows; ++r)
        {
            ct::Vector<String> cells;
            for (int c = 0; c < columns; ++c) cells.push_back(String::number(r));
            if (tree) tg->addRoot(cells);
            else
                dg->addRow(cells);
        }
        relayout();
    }
    void relayout()
    {
        if (tree) tg->rebuild();
        h.relayout();
        h.frames(2);
    }
    Rect rect() const
    {
        return tree ? tg->absoluteRect() : dg->absoluteRect();
    }
    float columnsWidth(int columns) const
    {
        float sum = 0.0f;
        for (int c = 0; c < columns; ++c) sum += tree ? tg->columnWidth(c) : dg->columnWidth(c);
        return sum;
    }
    Harness h;
    DataGrid* dg = nullptr;
    TreeGrid* tg = nullptr;
    bool tree;
    int selected = -1;
};

void testScrollBarThicknessIsTheThemes()
{
    // by default, in every preset, 8: the look the library always had
    check(ScrollBar::thickness() == 8.0f, "ScrollBar::thickness() is 8 by default");
    check(ScrollBar::kBarThickness == 8.0f, "and the old constant is still 8");
    bool allEight = Theme().scrollbarWidth == 8.0f;
    for (int i = 0; i < Theme::kPresetCount; ++i)
    {
        const Theme preset = Theme::byName(Theme::presetName(i));
        if (preset.scrollbarWidth != 8.0f)
        {
            printf("    the preset %s has %g\n", Theme::presetName(i), static_cast<double>(preset.scrollbarWidth));
            allEight = false;
        }
    }
    check(allEight, "scrollbarWidth is 8 in the default theme and in all five presets");

    const ThicknessScope scope(14.0f);
    check(ScrollBar::thickness() == 14.0f, "with the theme at 14, ScrollBar::thickness() is 14");
    check(ScrollBar::kBarThickness == 8.0f, "and the constant stays 8, for the code that reads it");
    ScrollBar vertical(ScrollBarOrientation::Vertical), horizontal(ScrollBarOrientation::Horizontal);
    check(vertical.sizeHint().x == 14.0f && horizontal.sizeHint().y == 14.0f, "a ScrollBar asks for the theme's thickness");

    // a ScrollView with content past both edges: both bars 14 thick, the content ends where the vertical one begins and
    // the horizontal one is under it
    {
        Harness h(700.0f, 500.0f, false);
        Widget* holder = h.root()->createChild<Widget>();
        ScrollView* view = holder->createChild<ScrollView>();
        view->setRect({ 10.0f, 10.0f, 200.0f, 150.0f });
        Wants* content = view->setContent<Wants>(500.0f, 600.0f);
        h.relayout();
        ScrollBar* vbar = findBar(view, ScrollBarOrientation::Vertical);
        ScrollBar* hbar = findBar(view, ScrollBarOrientation::Horizontal);
        check(vbar && vbar->isVisible() && vbar->rect().w == 14.0f, "ScrollView: the vertical bar is 14 thick");
        check(hbar && hbar->isVisible() && hbar->rect().h == 14.0f, "ScrollView: the horizontal bar is 14 thick");
        if (vbar && hbar)
        {
            check(vbar->rect().x + vbar->rect().w == 200.0f && hbar->rect().y + hbar->rect().h == 150.0f, "ScrollView: they are at the right and bottom edges");
            check(vbar->rect().y + vbar->rect().h <= hbar->rect().y, "ScrollView: the vertical bar ends where the horizontal one begins");
            check(vbar->rect().x - 2.0f == 200.0f - 16.0f, "ScrollView: the view the content sees is 16 less than the window (the bar and the 2 pixels gap)");
            check(content->rect().h == 600.0f && content->rect().w >= 500.0f, "ScrollView: the content keeps what it asked for");
        }
        // the theme changes with the view on screen: the next layout takes it
        Theme::instance().scrollbarWidth = 20.0f;
        h.relayout();
        check(vbar && vbar->rect().w == 20.0f && hbar && hbar->rect().h == 20.0f, "ScrollView: a new thickness is taken at the next layout");
        Theme::instance().scrollbarWidth = 14.0f;
    }

    // ListBox
    {
        Harness h(700.0f, 500.0f, false);
        Widget* holder = h.root()->createChild<Widget>();
        ListBox* list = holder->createChild<ListBox>();
        list->setRect({ 10.0f, 10.0f, 240.0f, 160.0f });
        for (int i = 0; i < 40; ++i) list->addItem(String("item ") + String::number(i));
        h.relayout();
        ScrollBar* bar = findBar(list, ScrollBarOrientation::Vertical);
        check(bar && bar->isVisible() && bar->rect().w == 14.0f, "ListBox: the bar is 14 thick");
        Rect item;
        check(list->itemRect(0, item), "(ListBox: item 0 has a rectangle)");
        if (bar) check(item.x + item.w <= bar->absoluteRect().x && bar->absoluteRect().x + bar->absoluteRect().w == 10.0f + 240.0f, "ListBox: the items end where the bar begins, and the bar is at the right edge");
    }

    // ListWidget
    {
        Harness h(700.0f, 500.0f, false);
        Widget* holder = h.root()->createChild<Widget>();
        ListWidget* list = holder->createChild<ListWidget>();
        list->setRect({ 10.0f, 10.0f, 240.0f, 160.0f });
        for (int i = 0; i < 20; ++i) list->addRow<Label>(String("row ") + String::number(i));
        h.relayout();
        ScrollBar* bar = findBar(list, ScrollBarOrientation::Vertical);
        check(bar && bar->isVisible() && bar->rect().w == 14.0f, "ListWidget: the bar is 14 thick");
        Widget* row = list->children().empty() ? nullptr : list->children()[0];
        for (Widget* c: list->children())
            if (!widget_cast<ScrollBar>(c)) { row = c; break; }
        if (bar && row) check(row->rect().x + row->rect().w <= bar->rect().x, "ListWidget: the rows end where the bar begins");
    }

    // DataGrid and TreeGrid
    for (int tree = 0; tree < 2; ++tree)
    {
        const char* name = tree ? "TreeGrid" : "DataGrid";
        GridRig g(tree != 0, 100, 3);
        const Rect r = g.rect();
        const float sum = g.columnsWidth(3);
        check(fabsf(sum - (r.w - 14.0f)) < 0.01f, (String(name) + ": with a vertical bar the columns end where it begins, 14 before the edge").c_str());
        if (fabsf(sum - (r.w - 14.0f)) >= 0.01f) printf("    %g of %g\n", static_cast<double>(sum), static_cast<double>(r.w));
        g.selected = -1;
        g.h.click(r.x + r.w - 12.0f, r.y + 60.0f); // 12 from the edge: the bar if it is 14 thick, a cell if it is 8
        check(g.selected == -1, (String(name) + ": a press 12 pixels from the edge is on the bar, not on a cell").c_str());
        g.h.click(r.x + r.w - 20.0f, r.y + 60.0f);
        check(g.selected != -1, (String(name) + ": and one 20 pixels from it is on a cell").c_str());
        // the theme changes: the grid lays itself out again with the new thickness, without being resized
        Theme::instance().scrollbarWidth = 20.0f;
        g.h.frames(2);
        check(fabsf(g.columnsWidth(3) - (r.w - 20.0f)) < 0.01f, (String(name) + ": a new thickness moves the end of the columns without the grid being resized").c_str());
        Theme::instance().scrollbarWidth = 14.0f;
        g.h.frames(2);
    }

    // ThumbnailGrid
    {
        Harness h(700.0f, 500.0f, false);
        Widget* holder = h.root()->createChild<Widget>();
        ThumbnailGrid* grid = holder->createChild<ThumbnailGrid>();
        grid->setRect({ 10.0f, 10.0f, 300.0f, 200.0f });
        grid->setThumbSize(60.0f, 60.0f);
        for (int i = 0; i < 60; ++i) grid->addItem(String("t") + String::number(i));
        h.relayout();
        ScrollBar* bar = findBar(grid, ScrollBarOrientation::Vertical);
        check(bar && bar->isVisible() && bar->rect().w == 14.0f, "ThumbnailGrid: the bar is 14 thick");
        if (bar)
        {
            check(bar->absoluteRect().x + bar->absoluteRect().w == 10.0f + 300.0f, "ThumbnailGrid: the bar is at the right edge");
            int hit = -2;
            for (float y = 12.0f; y < 208.0f; y += 6.0f)
                if (grid->itemAt(bar->absoluteRect().x + 1.0f, y) >= 0) hit = grid->itemAt(bar->absoluteRect().x + 1.0f, y);
            check(hit == -2, "ThumbnailGrid: no thumbnail is under the bar (they end where it begins)");
        }
    }

    // Timeline
    {
        Harness h(700.0f, 500.0f, false);
        Widget* holder = h.root()->createChild<Widget>();
        Timeline* line = holder->createChild<Timeline>();
        line->setRect({ 10.0f, 10.0f, 600.0f, 200.0f });
        for (int i = 0; i < 20; ++i) line->addTrack(String("track ") + String::number(i), TrackKind::Video);
        h.relayout();
        ScrollBar* bar = findBar(line, ScrollBarOrientation::Vertical);
        check(bar && bar->isVisible() && bar->rect().w == 14.0f, "Timeline: the bar is 14 thick");
        if (bar) check(bar->rect().x == 600.0f - 14.0f - 2.0f, "Timeline: and 2 pixels from the right edge, as it was with 8");
    }

    // The bars that are drawn by their widget and not made of a ScrollBar: what changes when the thickness goes from 8
    // to 14 is in the last 14 pixels at the right of the widget, and only there
    struct DrawnBar
    {
        const char* name;
        Widget* widget;
    };
    {
        Screen screen(500.0f, 300.0f);
        Widget* holder = screen.h.root()->createChild<Widget>();
        TextEdit* edit = holder->createChild<TextEdit>();
        edit->setRect({ 10.0f, 10.0f, 220.0f, 120.0f });
        String text;
        for (int i = 0; i < 60; ++i) text = text + "line " + String::number(i) + "\n";
        edit->setText(text);
        ConsoleWidget* console = holder->createChild<ConsoleWidget>();
        console->setRect({ 250.0f, 10.0f, 240.0f, 120.0f });
        for (int i = 0; i < 60; ++i) console->log(LogLevel::Info, String("message ") + String::number(i));
        TreeView* tree = holder->createChild<TreeView>();
        tree->setRect({ 10.0f, 150.0f, 220.0f, 120.0f });
        for (int i = 0; i < 40; ++i) tree->addRoot(String("node ") + String::number(i));
        screen.h.relayout();
        screen.h.frames(2);
        // (a new thickness is taken at the next layout, as for every other bar: round 6 made these three read it there)
        Theme::instance().scrollbarWidth = 8.0f;
        screen.h.relayout();
        screen.h.frames(2);
        const Shot eight = screen.take();
        Theme::instance().scrollbarWidth = 14.0f;
        screen.h.relayout();
        screen.h.frames(2);
        const Shot fourteen = screen.take();
        const DrawnBar bars[] = { { "TextEdit", edit }, { "ConsoleWidget", console }, { "TreeView", tree } };
        for (const DrawnBar& b: bars)
        {
            const Rect r = b.widget->absoluteRect();
            int left = 1 << 30;
            for (int x = static_cast<int>(r.x) + 1; x < static_cast<int>(r.x + r.w) - 1; ++x)
                if (differing(eight, fourteen, x, static_cast<int>(r.y) + 2, x + 1, static_cast<int>(r.y + r.h) - 2) > 0)
                {
                    left = x;
                    break;
                }
            const int want = static_cast<int>(r.x + r.w) - 14;
            const bool ok = left >= want - 1 && left <= want + 1;
            check(ok, (String(b.name) + ": the bar it draws starts 14 pixels from its right edge with the theme at 14").c_str());
            if (!ok) printf("    first column that changes: %d, wanted %d\n", left, want);
        }
    }
}

// ---- item 3: hostile inputs for the new code ----

// A frame of a 480x260 window never needs more than this (a roll with three thousand notes and their velocity bars is about
// a hundred thousand vertices); a frame over it is a loop that did not stop where it should.
constexpr size_t kMaxVertices = 200000;

bool finiteNumber(float v)
{
    return v == v && v - v == 0.0f;
}

bool treeFinite(const Widget* w)
{
    const Rect& r = w->rect();
    if (!finiteNumber(r.x) || !finiteNumber(r.y) || !finiteNumber(r.w) || !finiteNumber(r.h)) return false;
    for (const Widget* c: w->children())
        if (!treeFinite(c)) return false;
    return true;
}

bool verticesFinite()
{
    const DrawList& dl = GetDrawList();
    for (size_t i = 0; i < dl.vertices().size(); ++i)
        if (!finiteNumber(dl.vertices()[i].x) || !finiteNumber(dl.vertices()[i].y)) return false;
    return true;
}

// What a case does to a roll, frame by frame: every frame is made with the heap counted and then looked at (the vertices
// are under the bound and finite, so are the rectangles of the tree). The first thing wrong is printed with what had just
// been done, and counted as one failure; the case goes on with the checks that cannot be spoiled by it.
struct Watch
{
    explicit Watch(Harness& harness, const char* caseName) : h(harness), name(caseName)
    {
        gCurrentCase = caseName;
    }
    bool frame(const char* what)
    {
        gAskedBytes = 0;
        gCounting = true;
        h.frame();
        gCounting = false;
        const size_t n = GetDrawList().vertices().size();
        if (n > kMaxVertices) return fail(what, "more vertices than the bound", static_cast<double>(n));
        if (!treeFinite(h.root())) return fail(what, "a rectangle is not finite", 0.0);
        if (!verticesFinite()) return fail(what, "a vertex is not finite", 0.0);
        if (gAskedBytes > mostAsked) mostAsked = gAskedBytes;
        return true;
    }
    // an event of the synthetic input, counted like a frame, then a frame
    void move(float x, float y, const char* what)
    {
        gAskedBytes = 0;
        gCounting = true;
        WidgetApp::instance().feedMouseMove(x, y);
        gCounting = false;
        if (gAskedBytes > mostAsked) mostAsked = gAskedBytes;
        if (drawAfterEvents) frame(what);
    }
    void button(int b, bool down, const char* what)
    {
        gAskedBytes = 0;
        gCounting = true;
        WidgetApp::instance().feedMouseButton(b, down);
        gCounting = false;
        if (gAskedBytes > mostAsked) mostAsked = gAskedBytes;
        if (drawAfterEvents) frame(what);
    }
    bool fail(const char* what, const char* why, double number)
    {
        if (ok) printf("    %s: %s after %s (%g)\n", name, why, what, number);
        ok = false;
        return false;
    }
    Harness& h;
    const char* name;
    bool ok = true;
    bool drawAfterEvents = true; // false: the events alone are counted (for a roll too full to draw)
    unsigned long long mostAsked = 0;
};

const float kOdd[] = { 0.0f / 0.0f, 1.0f / 0.0f, -1.0f / 0.0f, 1.0e30f, -1.0e30f, 3.0e38f, -3.0e38f, 0.0f, -1.0f, 1.0e-30f, 16777216.0f,
    2147483648.0f, 1.0e9f, -1.0e9f, 0.5f, 100000.0f };

// every note of a roll is a number in the ranges the roll keeps them to
bool notesSane(const PianoRoll* roll)
{
    for (size_t i = 0; i < roll->notes().size(); ++i)
    {
        const PianoNote& n = roll->notes()[i];
        if (!finiteNumber(n.start) || n.start < -1.0e9f || n.start > 1.0e9f) return false;
        if (!finiteNumber(n.length) || !(n.length >= 0.01f) || n.length > 1.0e9f) return false;
        if (!(n.velocity >= 0.0f && n.velocity <= 1.0f)) return false;
    }
    return true;
}

void testHostileRollSettersAndNotes()
{
    RollRig rig;
    PianoRoll* roll = rig.roll;
    Watch watch(rig.h, "the roll's setters and notes");
    roll->addNote(55, 2.0f, 2.0f);
    roll->addNote(57, 3.0f, 1.0f);
    roll->setShowVelocityLane(true);
    char what[96];
    for (float v: kOdd)
    {
        snprintf(what, sizeof what, "the value %g", static_cast<double>(v));
        roll->setVelocityLaneHeight(v);
        watch.frame(what);
        roll->setNoteVelocity(0, v);
        roll->setNoteVelocity(1, v);
        roll->addNote(60, v, 1.0f, v);
        roll->addNote(61, 1.0f, v, v);
        roll->addNote(62, v, v, v);
        roll->setShowVelocityLane(true);
        watch.frame(what);
        roll->setTimeRange(v, v);
        watch.frame(what);
        roll->setTimeRange(0.0f, v);
        watch.frame(what);
        roll->setKeyWidth(v);
        watch.frame(what);
        roll->setRowHeight(v);
        watch.frame(what);
        roll->setPlayhead(v);
        roll->setTimeRange(0.0f, 8.0f);
        roll->setKeyWidth(36.0f);
        roll->setRowHeight(12.0f);
        watch.frame(what);
    }
    check(watch.ok, "the lane's height, velocities, notes, ranges and sizes that are not numbers leave the frames small and finite");
    check(notesSane(roll), "and the notes are numbers in their ranges (a velocity 0 to 1, a start and a length that are finite)");
    check(roll->velocityLaneHeight() >= 10.0f && roll->velocityLaneHeight() <= 10000.0f, "and the lane's height is one the roll keeps to");
    // the lane at every height that is asked, in this view and in a hostile one
    const float heights[] = { 10.0f, 11.0f, 59.0f, 60.0f, 99.0f, 100.0f, 101.0f, 5000.0f, 10000.0f };
    for (float hgt: heights)
    {
        roll->setVelocityLaneHeight(hgt);
        snprintf(what, sizeof what, "a lane of %g", static_cast<double>(hgt));
        watch.frame(what);
    }
    check(watch.ok, "a lane at every height from 10 to 10000 draws a bounded, finite frame");
    check(watch.mostAsked < 16ull << 20, "and a frame asks the heap for little (under 16 MiB)");
}

// A gesture of each kind with the pointer sent to places that are numbers of no use: not a number, infinite, enormous.
void testHostilePointerInGestures()
{
    RollRig rig;
    PianoRoll* roll = rig.roll;
    Watch watch(rig.h, "the pointer in gestures");
    roll->setShowVelocityLane(true);
    char what[96];
    // where to press for each gesture: the end of a note, the middle of one, an empty place, the lane, and (the middle button) the grid
    struct Gesture
    {
        const char* name;
        float x, y;
        int button;
    };
    const Gesture gestures[] = {
        { "resize", RollRig::xBeat(3.0f) - 2.5f, RollRig::yPitch(55), 0 },
        { "move", RollRig::xBeat(2.5f), RollRig::yPitch(55), 0 },
        { "create", RollRig::xBeat(5.0f) + 3.0f, RollRig::yPitch(52), 0 },
        { "lane", RollRig::xBeat(2.0f), yVelocity(0.5f), 0 },
        { "pan", RollRig::xBeat(2.0f), RollRig::yPitch(57), 2 },
        { "delete", RollRig::xBeat(2.5f), RollRig::yPitch(55), 1 },
    };
    for (const Gesture& g: gestures)
        for (float bad: kOdd)
            for (int axis = 0; axis < 3; ++axis) // x, y, both
            {
                roll->clearNotes();
                roll->addNote(55, 2.0f, 1.0f);
                roll->addNote(58, 4.0f, 2.0f);
                roll->setTimeRange(0.0f, 8.0f);
                watch.frame("(setting up)");
                snprintf(what, sizeof what, "a %s with the pointer sent to %g (%s)", g.name, static_cast<double>(bad), axis == 0 ? "x" : axis == 1 ? "y" : "both");
                watch.move(g.x, g.y, what);
                watch.button(g.button, true, what);
                watch.move(axis != 1 ? bad : g.x + 20.0f, axis != 0 ? bad : g.y, what);
                watch.move(g.x + 30.0f, g.y - 5.0f, what);
                watch.move(axis != 1 ? bad : g.x, axis != 0 ? bad : g.y, what);
                watch.button(g.button, false, what);
                watch.move(bad, bad, what);
                if (!notesSane(roll))
                {
                    printf("    %s: a note is not sane\n", what);
                    watch.ok = false;
                }
            }
    check(watch.ok, "a resize, a move, a put, a lane drag, a pan and a delete with the pointer at NaN, infinity or 1e30 leave finite frames and sane notes");
    // after all that the roll still does its job
    roll->clearNotes();
    roll->setTimeRange(0.0f, 8.0f);
    rig.h.frame();
    rig.move(RollRig::xBeat(2.0f) + 3.0f, RollRig::yPitch(55));
    rig.down();
    rig.move(RollRig::xBeat(4.0f) + 3.0f, RollRig::yPitch(55));
    rig.up();
    check(roll->notes().size() == 1 && roll->notes()[0].length == 2.0f, "(and the roll still puts a note of two beats afterwards)");
}

// Notes of enormous starts and lengths, resized and set in the lane in a view where they are
void testHostileNotesInGestures()
{
    RollRig rig;
    PianoRoll* roll = rig.roll;
    Watch watch(rig.h, "enormous notes in gestures");
    roll->setShowVelocityLane(true);
    const float centres[] = { 1.0e9f, -1.0e9f, 16777216.0f, 2147483648.0f, 1.0e30f, 0.0f };
    const float spans[] = { 0.01f, 8.0f, 1.0e6f };
    char what[96];
    for (float centre: centres)
        for (float span: spans)
        {
            roll->clearNotes();
            roll->addNote(55, centre, 1.0f);
            roll->addNote(56, centre, 1.0e9f);
            roll->addNote(57, centre + 1.0f, 0.01f);
            roll->setTimeRange(centre - span * 0.5f, centre + span * 0.5f);
            snprintf(what, sizeof what, "view %g + %g", static_cast<double>(centre), static_cast<double>(span));
            watch.frame(what);
            // every kind of gesture over the middle of the grid and the lane, in steps across the width
            for (int gesture = 0; gesture < 3; ++gesture)
            {
                const float y = gesture == 0 ? RollRig::yPitch(55) : gesture == 1 ? yVelocity(0.5f) : RollRig::yPitch(51);
                watch.move(150.0f, y, what);
                watch.button(0, true, what);
                for (float x = 60.0f; x < 420.0f; x += 40.0f) watch.move(x, y - 3.0f, what);
                watch.button(0, false, what);
            }
            if (!notesSane(roll))
            {
                printf("    %s: a note is not sane\n", what);
                watch.ok = false;
            }
        }
    check(watch.ok, "gestures over notes and views at 1e9, 2^24, 2^31 and 1e30 leave finite frames and sane notes");
}

void testHostileHandlersChangeTheNotes()
{
    // (a) the handler of onNoteResized removes every note
    {
        RollRig rig;
        PianoRoll* roll = rig.roll;
        roll->addNote(55, 2.0f, 1.0f);
        roll->addNote(58, 4.0f, 1.0f);
        int calls = 0;
        roll->onNoteResized.connect([&](int) { ++calls; roll->clearNotes(); });
        const float y = RollRig::yPitch(55);
        rig.move(RollRig::xBeat(3.0f) - 2.5f, y);
        rig.down();
        rig.move(RollRig::xBeat(5.0f) + 1.0f, y);
        rig.up();
        check(calls == 1 && roll->notes().empty(), "a handler of onNoteResized that removes all the notes is called once and the roll goes on with none");
        rig.h.frame();
        // and the roll still works afterwards
        rig.move(RollRig::xBeat(2.0f) + 3.0f, y);
        rig.down();
        rig.move(RollRig::xBeat(3.0f) + 3.0f, y);
        rig.up();
        check(roll->notes().size() == 1 && roll->notes()[0].length == 1.0f, "(a note put afterwards is there, a beat long)");
        rig.h.frame();
    }
    // (b) the handler of onNoteAdded, which is emitted first, removes every note: the signals that say which note do not come
    {
        RollRig rig;
        PianoRoll* roll = rig.roll;
        roll->addNote(55, 2.0f, 1.0f);
        SignalLog log;
        log.connect(roll);
        roll->onNoteAdded.connect([&](int, float, float) { roll->clearNotes(); });
        const float y = RollRig::yPitch(55);
        rig.move(RollRig::xBeat(3.0f) - 2.5f, y);
        rig.down();
        rig.move(RollRig::xBeat(5.0f) + 1.0f, y);
        rig.up();
        check(roll->notes().empty() && log.resized == 0 && log.moved == 0 && log.created == 0, "notes removed by the handler of onNoteAdded: no signal about a note that is not there");
        // a note put with the drag, the same
        rig.move(RollRig::xBeat(2.0f) + 3.0f, y);
        rig.down();
        rig.move(RollRig::xBeat(4.0f) + 3.0f, y);
        rig.up();
        check(roll->notes().empty() && log.created == 0, "and for a note that was just put");
        rig.h.frame();
    }
    // (c) a handler of onNoteClicked, emitted at the press, removes the first note (the dragged one is index 0 from then on), or all
    {
        RollRig rig;
        PianoRoll* roll = rig.roll;
        roll->addNote(50, 0.0f, 1.0f);
        roll->addNote(55, 2.0f, 1.0f);
        SignalLog log;
        log.connect(roll);
        int clicks = 0;
        roll->onNoteClicked.connect([&](int) { if (++clicks == 1) roll->removeNote(0); });
        const float y = RollRig::yPitch(55);
        rig.move(RollRig::xBeat(3.0f) - 2.5f, y);
        rig.down();
        rig.move(RollRig::xBeat(4.0f) + 1.0f, y);
        rig.up();
        check(roll->notes().size() == 1 && roll->notes()[0].pitch == 55 && roll->notes()[0].length == 2.0f, "a handler that removes the note before the dragged one at the press: the drag goes on with the dragged note");
        check(log.resized == 1 && log.resizedIndex == 0, "and says index 0");
        roll->onNoteClicked.disconnectAll();
        roll->onNoteClicked.connect([&](int) { roll->clearNotes(); });
        rig.move(RollRig::xBeat(4.0f) - 2.5f, y);
        rig.down();
        rig.move(RollRig::xBeat(6.0f), y);
        rig.up();
        check(roll->notes().empty(), "a handler that removes all the notes at the press: the drag has nothing to drag and nothing happens");
        rig.h.frame();
    }
    // (d) the handler of onNoteVelocityChanged
    {
        const int kNotes = 8;
        auto build = [&](RollRig& rig)
        {
            for (int i = 0; i < kNotes; ++i) rig.roll->addNote(50 + (i % 5), 1.0f + static_cast<float>(i) * 0.5f, 0.25f); // beats 1 to 4.5
            rig.roll->setShowVelocityLane(true);
            rig.h.frame();
        };
        auto drag = [&](RollRig& rig)
        {
            // from beat 0.9 to 3.1 along the middle of the lane: the notes at beats 1 to 3 (five of them)
            rig.move(RollRig::xBeat(0.9f), yVelocity(0.5f));
            rig.down();
            rig.move(RollRig::xBeat(3.1f), yVelocity(0.5f));
            rig.up();
        };
        {
            RollRig rig;
            build(rig);
            int told[16], count = 0;
            bool valid = true;
            rig.roll->onNoteVelocityChanged.connect([&](int i)
                    {
                        if (i < 0 || i >= static_cast<int>(rig.roll->notes().size())) valid = false;
                        else
                            rig.roll->removeNote(i);
                        if (count < 16) told[count] = i;
                        ++count;
                    });
            drag(rig);
            check(count == 5 && valid, "a handler that removes the note it is told: five notes changed, five calls, every index right when it came");
            check(static_cast<int>(rig.roll->notes().size()) == kNotes - 5, "and the three notes that were not under the pointer are what is left");
            bool leftAreThoseNotTouched = true;
            for (size_t i = 0; i < rig.roll->notes().size(); ++i)
                if (rig.roll->notes()[i].start < 3.4f) leftAreThoseNotTouched = false;
            check(leftAreThoseNotTouched, "(the ones from beat 3.5 on)");
            rig.h.frame();
        }
        {
            RollRig rig;
            build(rig);
            int count = 0;
            rig.roll->onNoteVelocityChanged.connect([&](int) { ++count; rig.roll->clearNotes(); });
            drag(rig);
            check(count == 1 && rig.roll->notes().empty(), "a handler that removes all the notes: called once, the other four are not said");
            rig.h.frame();
            rig.move(RollRig::xBeat(2.0f), yVelocity(0.3f));
            rig.down();
            rig.up();
            check(rig.roll->notes().empty(), "(and a lane gesture on a roll with no notes does nothing)");
        }
        {
            RollRig rig;
            build(rig);
            int count = 0;
            rig.roll->onNoteVelocityChanged.connect([&](int)
                    {
                        ++count;
                        rig.roll->addNote(60, 7.0f, 1.0f);
                        rig.roll->setShowVelocityLane(false);
                        rig.roll->setVelocityLaneHeight(0.0f / 0.0f);
                    });
            drag(rig);
            check(count == 5 && rig.roll->notes().size() == static_cast<size_t>(kNotes + 5), "a handler that adds notes and hides the lane: five calls, the notes are added, nothing breaks");
            check(notesSane(rig.roll), "(and the notes are sane)");
            rig.h.frames(2);
        }
    }
    // (e) the application changes the notes between two events of the mouse, in each kind of gesture
    {
        const char* names[] = { "resize", "move", "put", "lane" };
        for (int gesture = 0; gesture < 4; ++gesture)
            for (int change = 0; change < 4; ++change)
            {
                RollRig rig;
                PianoRoll* roll = rig.roll;
                for (int i = 0; i < 4; ++i) roll->addNote(52 + i, 1.0f + static_cast<float>(i), 1.0f);
                roll->setShowVelocityLane(true);
                rig.h.frame();
                float px = RollRig::xBeat(2.0f) - 2.5f, py = RollRig::yPitch(53);
                if (gesture == 1) px = RollRig::xBeat(1.5f);
                if (gesture == 2) { px = RollRig::xBeat(6.0f) + 3.0f; py = RollRig::yPitch(50); }
                if (gesture == 3) { px = RollRig::xBeat(2.0f); py = yVelocity(0.4f); }
                rig.move(px, py);
                rig.down();
                rig.move(px + 20.0f, py);
                // the change
                if (change == 0) roll->clearNotes();
                else if (change == 1)
                    while (!roll->notes().empty()) roll->removeNote(0);
                else if (change == 2)
                    roll->removeNote(0);
                else
                    for (int i = 0; i < 100; ++i) roll->addNote(60, static_cast<float>(i) * 0.1f, 0.5f);
                rig.move(px + 60.0f, py - 4.0f);
                rig.move(px + 100.0f, py + 4.0f);
                rig.up();
                rig.h.frame();
                const bool ok = notesSane(roll);
                String text = String("notes changed by the application under a ") + names[gesture] + " gesture (change " + String::number(change) + "): the roll stays sane";
                check(ok, text.c_str());
                // and a new gesture works
                const size_t before = roll->notes().size();
                rig.move(RollRig::xBeat(7.0f), RollRig::yPitch(49));
                rig.down();
                rig.up();
                check(roll->notes().size() == before + 1, "(and the next click puts a note)");
            }
    }
    // (f) removal with the right button, the handler of onNoteRemoved removes everything
    {
        RollRig rig;
        PianoRoll* roll = rig.roll;
        roll->addNote(55, 2.0f, 1.0f);
        roll->addNote(56, 4.0f, 1.0f);
        int deleted = 0;
        roll->onNoteRemoved.connect([&](int) { roll->clearNotes(); });
        roll->onNoteDeleted.connect([&](int, const PianoNote&) { ++deleted; });
        rig.move(RollRig::xBeat(2.5f), RollRig::yPitch(55));
        rig.down(1);
        rig.up(1);
        check(roll->notes().empty() && deleted == 1, "a handler of onNoteRemoved that removes everything: the note is gone once and the roll goes on");
        rig.h.frame();
    }
}

void testHostileZeroAndTinyRolls()
{
    const Rect sizes[] = { Rect(20.0f, 20.0f, 0.0f, 0.0f), Rect(20.0f, 20.0f, 36.0f, 200.0f), Rect(20.0f, 20.0f, 400.0f, 0.0f),
        Rect(20.0f, 20.0f, 400.0f, 15.0f), Rect(20.0f, 20.0f, 400.0f, 19.0f), Rect(20.0f, 20.0f, 400.0f, 21.0f), Rect(20.0f, 20.0f, 0.5f, 0.5f),
        Rect(20.0f, 20.0f, 37.0f, 30.0f), Rect(20.0f, 20.0f, -10.0f, -10.0f), Rect(20.0f, 20.0f, 400.0f, 1.0e9f), Rect(20.0f, 20.0f, 1.0e9f, 200.0f) };
    char what[96];
    for (int lane = 0; lane < 2; ++lane)
        for (const Rect& r: sizes)
        {
            RollRig rig;
            PianoRoll* roll = rig.roll;
            Watch watch(rig.h, "zero-size rolls");
            roll->setRect(r);
            roll->addNote(55, 2.0f, 2.0f);
            roll->addNote(56, 3.0f, 0.25f);
            roll->setShowVelocityLane(lane != 0);
            roll->setKeyWidth(36.0f);
            snprintf(what, sizeof what, "a roll of %g x %g with the lane %s", static_cast<double>(r.w), static_cast<double>(r.h), lane ? "on" : "off");
            watch.frame(what);
            // every kind of gesture at points across and around the roll
            for (int button = 0; button < 3; ++button)
                for (float fx = -0.2f; fx < 1.3f; fx += 0.25f)
                    for (float fy = -0.2f; fy < 1.3f; fy += 0.25f)
                    {
                        const float x = r.x + r.w * fx, y = r.y + r.h * fy;
                        watch.move(x, y, what);
                        watch.button(button, true, what);
                        watch.move(x + 30.0f, y - 10.0f, what);
                        watch.move(x - 30.0f, y + 10.0f, what);
                        watch.button(button, false, what);
                    }
            roll->setVelocityLaneHeight(10.0f);
            watch.frame(what);
            if (!notesSane(roll))
            {
                printf("    %s: a note is not sane\n", what);
                watch.ok = false;
            }
            check(watch.ok, (String("a zero-size or tiny roll, ") + what + ": finite frames, sane notes, no crash").c_str());
            roll->setRect(Rect(20.0f, 20.0f, 400.0f, 200.0f));
            watch.frame(what);
        }
}

void testHostileManyNotes()
{
    RollRig rig;
    PianoRoll* roll = rig.roll;
    Watch watch(rig.h, "many notes");
    roll->setShowVelocityLane(true);
    // three thousand notes in view: one lane drag across all of them
    for (int i = 0; i < 3000; ++i) roll->addNote(50 + i % 9, static_cast<float>(i) * (8.0f / 3000.0f), 0.05f, 0.5f);
    watch.frame("3000 notes in view");
    int told = 0;
    roll->onNoteVelocityChanged.connect([&](int) { ++told; });
    watch.move(60.0f, yVelocity(0.9f), "3000 notes");
    watch.button(0, true, "3000 notes");
    watch.move(415.0f, yVelocity(0.1f), "3000 notes");
    watch.button(0, false, "3000 notes");
    check(watch.ok && notesSane(roll), "three thousand notes in view and a lane drag across all of them: bounded frames, sane notes");
    check(told > 2500 && told <= 3000, "(nearly every note says it was set, once)");
    check(watch.mostAsked < 16ull << 20, "and no frame or event asked the heap for more than 16 MiB");
    // two hundred thousand notes out of view (the view is eight beats): the lane draws and answers in a bounded time
    roll->clearNotes();
    for (int i = 0; i < 200000; ++i) roll->addNote(50 + i % 9, 100.0f + static_cast<float>(i), 0.5f);
    watch.frame("200000 notes out of view");
    watch.move(150.0f, yVelocity(0.5f), "200000 notes out of view");
    watch.button(0, true, "200000 notes out of view");
    watch.move(300.0f, yVelocity(0.2f), "200000 notes out of view");
    watch.button(0, false, "200000 notes out of view");
    watch.move(150.0f, RollRig::yPitch(52), "200000 notes out of view");
    check(watch.ok, "two hundred thousand notes out of view: bounded frames and events");
    // four thousand in view, the lane's bars drawn for each: the frame is the roll's own (it draws every note) plus a bar each
    roll->clearNotes();
    for (int i = 0; i < 4000; ++i) roll->addNote(50 + i % 9, static_cast<float>(i) * 0.002f, 0.002f, static_cast<float>(i % 10) * 0.1f);
    watch.frame("4000 notes in view");
    check(watch.ok, "four thousand notes of a quarter of a pixel each in view: a bounded frame");
    // two hundred thousand in view at once, with the lane on: the frames are drawn now (round 6: a roll with more notes than
    // half the cells it shows draws what they cover, a note for each run, and the lane one bar for each pixel column; before
    // that a frame of this asked the heap for 89 MiB and the program stopped at the ceiling), so the vertex bound, the heap
    // ceiling and the finite rectangles are looked at in every frame of a lane drag over all of them, which says each note once
    roll->clearNotes();
    for (int i = 0; i < 200000; ++i) roll->addNote(50 + i % 9, static_cast<float>(i) * 0.00004f, 0.00004f, 0.5f);
    told = 0;
    watch.frame("200000 notes in view");
    watch.move(60.0f, yVelocity(0.9f), "200000 notes in view");
    watch.button(0, true, "200000 notes in view");
    watch.move(415.0f, yVelocity(0.1f), "200000 notes in view");
    watch.button(0, false, "200000 notes in view");
    check(watch.ok, "two hundred thousand notes in view, the lane on: every frame is bounded and finite");
    check(told > 100000 && told <= 200000, "a lane drag over two hundred thousand notes in view says each note once");
    check(watch.mostAsked < 16ull << 20, "and no frame or event of it asked the heap for more than 16 MiB");
    check(notesSane(roll), "(the notes are sane)");
    // the same without the lane, with the velocity shown, and with notes of a length (a million overlapping long notes would be
    // 4 million vertices one by one)
    roll->setShowVelocityLane(false);
    roll->setShowVelocity(true);
    watch.frame("200000 notes in view, no lane");
    roll->clearNotes();
    for (int i = 0; i < 200000; ++i) roll->addNote(48 + i % 12, static_cast<float>(i % 9973) * 0.0008f, 0.5f + static_cast<float>(i % 7), 0.1f * static_cast<float>(i % 11));
    watch.frame("200000 long notes in view");
    check(watch.ok, "two hundred thousand long notes, the velocity shown: a bounded, finite frame");
    check(watch.mostAsked < 16ull << 20, "and a frame asks the heap for little (under 16 MiB)");
    // and past the edges of the view, with a note being dragged among them
    roll->setTimeRange(3.0f, 3.5f);
    watch.frame("200000 notes, 0.5 beats in view");
    watch.move(RollRig::xBeat(0.2f), RollRig::yPitch(50), "200000 notes, dragging");
    watch.button(0, true, "200000 notes, dragging");
    watch.move(RollRig::xBeat(0.4f), RollRig::yPitch(51), "200000 notes, dragging");
    watch.button(0, false, "200000 notes, dragging");
    check(watch.ok && notesSane(roll), "two hundred thousand notes with half a beat in view, one of them dragged: bounded, finite frames and sane notes");
}

// the thickness of the bars that is not a number, infinite, enormous, zero or negative, with every kind of bar in the window
void testHostileScrollBarThickness()
{
    Harness h(480.0f, 320.0f, false);
    Watch watch(h, "the scroll bar thickness");
    Widget* holder = h.root()->createChild<Widget>();
    ScrollView* view = holder->createChild<ScrollView>();
    view->setRect({ 0.0f, 0.0f, 150.0f, 100.0f });
    view->setContent<Wants>(400.0f, 400.0f);
    ListBox* list = holder->createChild<ListBox>();
    list->setRect({ 160.0f, 0.0f, 150.0f, 100.0f });
    for (int i = 0; i < 30; ++i) list->addItem(String("item ") + String::number(i));
    ListWidget* rows = holder->createChild<ListWidget>();
    rows->setRect({ 320.0f, 0.0f, 150.0f, 100.0f });
    for (int i = 0; i < 12; ++i) rows->addRow<Label>(String("row ") + String::number(i));
    DataGrid* data = holder->createChild<DataGrid>();
    data->setRect({ 0.0f, 110.0f, 150.0f, 100.0f });
    for (int c = 0; c < 3; ++c) data->addColumn(String("C") + String::number(c), 80.0f);
    for (int r = 0; r < 50; ++r)
    {
        ct::Vector<String> cells;
        for (int c = 0; c < 3; ++c) cells.push_back(String::number(r));
        data->addRow(cells);
    }
    TreeGrid* treeGrid = holder->createChild<TreeGrid>();
    treeGrid->setRect({ 160.0f, 110.0f, 150.0f, 100.0f });
    for (int c = 0; c < 3; ++c) treeGrid->addColumn(String("C") + String::number(c), 80.0f);
    for (int r = 0; r < 50; ++r)
    {
        ct::Vector<String> cells;
        for (int c = 0; c < 3; ++c) cells.push_back(String::number(r));
        treeGrid->addRoot(cells);
    }
    treeGrid->rebuild();
    ThumbnailGrid* thumbs = holder->createChild<ThumbnailGrid>();
    thumbs->setRect({ 320.0f, 110.0f, 150.0f, 100.0f });
    thumbs->setThumbSize(40.0f, 40.0f);
    for (int i = 0; i < 40; ++i) thumbs->addItem(String("t") + String::number(i));
    Timeline* line = holder->createChild<Timeline>();
    line->setRect({ 0.0f, 220.0f, 150.0f, 100.0f });
    for (int i = 0; i < 12; ++i) line->addTrack(String("track ") + String::number(i), TrackKind::Video);
    TreeView* tree = holder->createChild<TreeView>();
    tree->setRect({ 160.0f, 220.0f, 150.0f, 100.0f });
    for (int i = 0; i < 30; ++i) tree->addRoot(String("node ") + String::number(i));
    ConsoleWidget* console = holder->createChild<ConsoleWidget>();
    console->setRect({ 320.0f, 220.0f, 150.0f, 100.0f });
    for (int i = 0; i < 40; ++i) console->log(LogLevel::Info, String("message ") + String::number(i));
    h.relayout();
    const Theme saved = Theme::instance();
    char what[96];
    for (float v: kOdd)
    {
        Theme::instance().scrollbarWidth = v;
        snprintf(what, sizeof what, "the thickness %g", static_cast<double>(v));
        h.app.requestLayout();
        watch.frame(what);
        watch.frame(what);
        // the bars are as thick as the library keeps them to, and the one a widget asks of the theme is the one it draws
        const float thick = ScrollBar::thickness();
        if (!(thick >= 4.0f && thick <= 100.0f))
        {
            printf("    %s: ScrollBar::thickness() is %g\n", what, static_cast<double>(thick));
            watch.ok = false;
        }
        // presses on the bars and wheel turns on the widgets
        const Rect cells[] = { data->absoluteRect(), treeGrid->absoluteRect(), thumbs->absoluteRect(), line->absoluteRect(), tree->absoluteRect(), view->absoluteRect(), list->absoluteRect() };
        for (const Rect& r: cells)
        {
            watch.move(r.x + r.w - 3.0f, r.y + 50.0f, what);
            watch.button(0, true, what);
            watch.move(r.x + r.w - 3.0f, r.y + 80.0f, what);
            watch.button(0, false, what);
            h.app.feedMouseWheel(0.0f, -3.0f);
            watch.frame(what);
        }
    }
    Theme::instance() = saved;
    check(watch.ok, "a scroll bar thickness that is not a number, infinite, enormous, zero or negative leaves every kind of bar (ScrollView, lists, grids, ThumbnailGrid, Timeline, TreeView, ConsoleWidget) with finite, bounded frames");
    check(ScrollBar::thickness() == 8.0f, "(and the theme is back to 8)");
}

} // namespace

int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testPianoRollResizeWithTheMouse);
    RUN(testPianoRollNewNoteTakesItsLengthFromTheDrag);
    RUN(testPianoRollNoteIndexStaysRightWhenANoteBeforeItGoes);
    RUN(testPianoRollVelocityLaneSetsVelocities);
    RUN(testPianoRollVelocityLanePaintsAcrossBars);
    RUN(testPianoRollVelocityLaneDrawsOneBarPerNote);
    RUN(testPianoRollVelocityLaneFollowsTheView);
    RUN(testPianoRollVelocityLaneHeightAndHitAreas);
    RUN(testSerializerKeepsToolBar);
    RUN(testSerializerKeepsTheOrderOfAToolBarAfterADrag);
    RUN(testToolBarLoadsWhatAFileCanHold);
    RUN(testScrollBarThicknessIsTheThemes);
    RUN(testHostileRollSettersAndNotes);
    RUN(testHostilePointerInGestures);
    RUN(testHostileNotesInGestures);
    RUN(testHostileHandlersChangeTheNotes);
    RUN(testHostileZeroAndTinyRolls);
    RUN(testHostileManyNotes);
    RUN(testHostileScrollBarThickness);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
