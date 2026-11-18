// Round 5 (docs/ROUND5.md): the piano roll's note length with the mouse and its velocity lane, hostile inputs for that
// new code, the tool bar in the serializer, the scroll bar thickness from the theme. Headless: the real frame loop
// (tests/RetainedHarness.hpp), the synthetic input of WidgetApp (feedMouseMove/Button/Wheel/Key/Text) where a mouse is
// involved, pixels of the software rasteriser where a picture is the point. No standard library: ct and C only.

#include "RasterScreen.hpp"
#include "RetainedHarness.hpp"

#include <igui/widgets/AudioWidgets.hpp>

using namespace rh;

namespace
{

// ---- a piano roll in a window, and a mouse on it ----

// The roll of these tests: 400x200 at (20, 20) in a 480x260 window, 12 rows of 12 pixels (pitches 48 to 59, the top row
// is 59), the keys 36 pixels wide so the grid starts at x = 56 and 8 beats take 364 pixels, 45.5 a beat.
struct RollRig
{
    explicit RollRig(float width = 480.0f, float height = 260.0f)
            : h(width, height, false),
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
    Harness h;
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

} // namespace

int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testPianoRollResizeWithTheMouse);
    RUN(testPianoRollNewNoteTakesItsLengthFromTheDrag);
    RUN(testPianoRollNoteIndexStaysRightWhenANoteBeforeItGoes);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
