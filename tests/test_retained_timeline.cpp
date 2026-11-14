// The retained Timeline under the abuse a project gives it: clips removed, tracks
// cleared or the whole widget deleted while a clip is being dragged, thousands of
// clips, zero length and negative times, NaN, zoom to the extremes. Driven through
// the real frame loop (tests/RetainedHarness.hpp); most only fail under ASan.

#include "RetainedHarness.hpp"

#include <igui/widgets/BasicWidgets.hpp>
#include <igui/widgets/LayoutWidgets.hpp>
#include <igui/widgets/Timeline.hpp>

using namespace rh;

namespace
{

// A timeline filling the stage: two video tracks, an audio one, 0 to 20 seconds.
struct Rig
{
    Harness h;
    BoxLayout* box = nullptr;
    Timeline* tl = nullptr;

    // Signals the application handles by indexing track().clips[]: an index out of range there is
    // an out-of-bounds read in the application.
    int badIndex = 0;
    int changed = 0;
    int edited = 0;

    Rig()
    {
        box = h.root()->createChild<BoxLayout>(LayoutDir::Vertical);
        box->setPadding(8.0f);
        tl = box->createChild<Timeline>();
        tl->setStretch(1.0f);
        tl->addTrack("Video 2", TrackKind::Video);
        tl->addTrack("Video 1", TrackKind::Video);
        tl->addTrack("Audio 1", TrackKind::Audio);
        tl->setTimeRange(0.0f, 20.0f);
        tl->setFrameRate(30.0f);
        h.relayout();
        tl->onClipChanged.connect([this](int t, int c) { ++changed; checkIndex(t, c); });
        tl->onClipEdited.connect([this](int t, int c) { ++edited; checkIndex(t, c); });
    }

    void checkIndex(int t, int c)
    {
        if (t < 0 || t >= tl->trackCount() || c < 0 || c >= static_cast<int>(tl->track(t).clips.size()))
            ++badIndex;
    }

    // Window point at the middle of a clip (its row, the middle of its time span).
    bool clipPoint(int track, int clip, float& x, float& y)
    {
        const TimelineClip& c = tl->track(track).clips[clip];
        return tl->pointAt(track, (c.start + c.end) * 0.5f, x, y);
    }
    void press(float x, float y)
    {
        h.moveTo(x, y);
        h.down();
    }
};

// ---- scenarios ----

// ---- scenarios ----
// A project command (Delete, undo of an add) removes the clip that is being
// dragged: the drag state held its index and the next move wrote past the end.
void testRemoveDraggedClip()
{
    printf("a clip removed while it is dragged\n");
    Rig r;
    r.tl->addClip(0, 1.0f, 4.0f, "a");
    r.tl->addClip(0, 6.0f, 9.0f, "b");
    r.h.relayout();
    float x = 0, y = 0;
    check(r.clipPoint(0, 0, x, y), "the clip is on screen");
    r.press(x, y);
    r.h.moveTo(x + 30.0f, y);
    r.h.moveTo(x + 60.0f, y);
    const int before = r.changed;
    check(before > 0, "the drag changed the clip");
    r.tl->removeClip(0, 0);
    r.h.moveTo(x + 90.0f, y);
    r.h.moveTo(x + 120.0f, y);
    r.h.up();
    r.h.frames(2);
    check(r.tl->track(0).clips.size() == 1u, "one clip is left");
    check(r.tl->track(0).clips[0].start == 6.0f && r.tl->track(0).clips[0].end == 9.0f,
            "the clip that slid into the removed one's place is not dragged along");
    check(r.changed == before && r.edited == 0, "the drag ended with the clip: no more changes, no edit");
    check(r.badIndex == 0, "no signal named a clip that is not there");
}

void testRemoveLastClipWhileDragged()
{
    printf("the only clip removed while it is dragged (every drag mode)\n");
    for (int mode = 0; mode < 3; ++mode)
    {
        Rig r;
        r.tl->addClip(1, 2.0f, 8.0f, "only");
        r.h.relayout();
        float x = 0, y = 0;
        r.clipPoint(1, 0, x, y);
        const float edgeX = mode == 0 ? x : (mode == 1 ? x - 3.0f * 53.0f + 2.0f : x + 3.0f * 53.0f - 2.0f);
        r.press(edgeX, y);
        r.h.moveTo(edgeX + 20.0f, y);
        r.tl->removeClip(1, 0);
        r.h.moveTo(edgeX + 40.0f, y);
        r.h.moveTo(edgeX - 40.0f, y + 40.0f);
        r.h.up();
        r.h.frames(2);
        check(r.badIndex == 0, "no signal named the clip that is gone");
        check(r.edited == 0, "a clip that is gone is not reported as edited");
    }
}

void testRemoveClipOfDraggedGroup()
{
    printf("a clip of a dragged group removed\n");
    Rig r;
    r.tl->addClip(0, 1.0f, 3.0f, "a");
    r.tl->addClip(0, 4.0f, 6.0f, "b");
    r.tl->addClip(1, 2.0f, 5.0f, "c");
    r.h.relayout();
    float x0, y0, x1, y1, x2, y2;
    r.clipPoint(0, 0, x0, y0);
    r.clipPoint(0, 1, x1, y1);
    r.clipPoint(1, 0, x2, y2);
    // Ctrl+click selects a second and a third clip.
    r.h.moveTo(x0, y0);
    r.h.down();
    r.h.up();
    r.h.io().keyCtrl = true;
    r.h.click(x1, y1);
    r.h.click(x2, y2);
    r.h.io().keyCtrl = false;
    check(r.tl->selectedClipCount() == 3, "three clips are selected");
    r.press(x0, y0);
    r.h.moveTo(x0 + 25.0f, y0);
    r.h.moveTo(x0 + 50.0f, y0);
    r.tl->removeClip(1, 0);
    r.h.moveTo(x0 + 75.0f, y0);
    r.tl->removeClip(0, 1);
    r.h.moveTo(x0 + 100.0f, y0);
    r.h.up();
    r.h.frames(2);
    check(r.badIndex == 0, "no signal named a clip that is gone");
    check(r.tl->track(0).clips.size() == 1u && r.tl->track(1).clips.empty(), "the clips are gone");
}

void testHandlerRemovesDraggedClip()
{
    printf("onClipChanged removes the clip that is changing\n");
    Rig r;
    r.tl->addClip(0, 1.0f, 4.0f, "a");
    r.h.relayout();
    int calls = 0;
    r.tl->onClipChanged.connect(
            [&](int track, int clip)
            {
                ++calls;
                r.tl->removeClip(track, clip);
            });
    float x = 0, y = 0;
    r.clipPoint(0, 0, x, y);
    r.press(x, y);
    r.h.moveTo(x + 30.0f, y);
    r.h.moveTo(x + 60.0f, y);
    r.h.up();
    r.h.frames(2);
    check(calls >= 1, "the handler ran");
}

void testTimelineDeletedFromItsSignals()
{
    printf("the timeline deleted from its own signals\n");
    for (int variant = 0; variant < 4; ++variant)
    {
        Rig r;
        r.tl->addClip(0, 1.0f, 4.0f, "a");
        r.h.relayout();
        Timeline* self = r.tl;
        auto kill = [&]
        {
            if (!self) return;
            Timeline* dead = self;
            self = nullptr;
            r.box->removeChild(dead);
        };
        if (variant == 0) r.tl->onClipSelected.connect([&](int, int) { kill(); });
        else if (variant == 1)
            r.tl->onClipChanged.connect([&](int, int) { kill(); });
        else if (variant == 2)
            r.tl->onClipEdited.connect([&](int, int) { kill(); });
        else
            r.tl->onPlayheadChanged.connect([&](float) { kill(); });
        float x = 0, y = 0;
        if (variant == 3)
        {
            r.h.moveTo(x = 300.0f, y = 20.0f); // the ruler: scrub
            r.h.down();
            r.h.moveTo(400.0f, 20.0f);
        }
        else
        {
            r.clipPoint(0, 0, x, y);
            r.press(x, y);
            r.h.moveTo(x + 30.0f, y);
            r.h.moveTo(x + 60.0f, y);
        }
        r.h.up();
        r.h.frames(2);
        check(self == nullptr, "the handler removed the timeline");
        r.h.moveTo(5.0f, 5.0f);
    }
}

void testClearAndReplaceTracksWhileDragging()
{
    printf("tracks cleared, removed, inserted or replaced under a drag\n");
    for (int variant = 0; variant < 5; ++variant)
    {
        Rig r;
        r.tl->addClip(1, 2.0f, 8.0f, "a");
        r.tl->addKeyframe(1, 12.0f);
        r.h.relayout();
        float x = 0, y = 0;
        r.clipPoint(1, 0, x, y);
        r.press(x, y);
        r.h.moveTo(x + 20.0f, y);
        switch (variant)
        {
            case 0: r.tl->clearTracks(); break;
            case 1: r.tl->removeTrack(1); break;
            case 2: r.tl->insertTrack(0, "new", TrackKind::Video); break;
            case 3:
            {
                ct::Vector<TimelineTrack> tracks;
                tracks.push_back(TimelineTrack());
                r.tl->setAllTracks(tracks);
                break;
            }
            default: r.tl->removeTrack(0); break;
        }
        r.h.moveTo(x + 50.0f, y);
        r.h.moveTo(x + 80.0f, y + 30.0f);
        r.h.up();
        r.h.frames(2);
    }
}

void testRemoveKeyframeWhileDragging()
{
    printf("a keyframe removed while it is dragged\n");
    Rig r;
    r.tl->addKeyframe(0, 5.0f);
    r.tl->addKeyframe(0, 8.0f);
    r.h.relayout();
    float x = 0, y = 0;
    r.tl->pointAt(0, 5.0f, x, y);
    r.press(x, y);
    r.h.moveTo(x + 20.0f, y);
    r.tl->removeKeyframe(0, 0);
    r.h.moveTo(x + 40.0f, y);
    r.tl->removeKeyframe(0, 0);
    r.h.moveTo(x + 60.0f, y);
    r.h.up();
    r.h.frames(2);
}

void testEnvelopePointRemovedWhileDragging()
{
    printf("an envelope point and then its clip removed while the point is dragged\n");
    Rig r;
    r.tl->addClip(2, 2.0f, 12.0f, "wave");
    TimelineClip& c = r.tl->track(2).clips[0];
    c.editableEnvelope = true;
    CurveKey a, b, d;
    a.time = 0.0f; a.value = 0.0f;
    b.time = 5.0f; b.value = 6.0f;
    d.time = 10.0f; d.value = 0.0f;
    c.envelope.push_back(a);
    c.envelope.push_back(b);
    c.envelope.push_back(d);
    r.h.relayout();
    float x = 0, y = 0;
    r.tl->pointAt(2, 7.0f, x, y); // the middle point: media time 5 at timeline 7
    r.press(x, y - 2.0f);
    r.h.moveTo(x + 10.0f, y - 6.0f);
    r.tl->track(2).clips[0].envelope.clear();
    r.h.moveTo(x + 20.0f, y - 8.0f);
    r.tl->removeClip(2, 0);
    r.h.moveTo(x + 30.0f, y - 10.0f);
    r.h.up();
    r.h.frames(2);
}

void testGrabModeClipRemoved()
{
    printf("a grabbed clip removed: the grab goes on to its release\n");
    Rig r;
    r.tl->addClip(0, 1.0f, 6.0f, "a");
    r.tl->setClipGrab(true);
    int moved = 0, released = 0;
    r.tl->onClipGrabMoved.connect([&](float, float) { ++moved; });
    r.tl->onClipGrabReleased.connect([&](float, float) { ++released; });
    r.h.relayout();
    float x = 0, y = 0;
    r.clipPoint(0, 0, x, y);
    r.press(x, y);
    r.h.moveTo(x + 10.0f, y);
    r.tl->removeClip(0, 0);
    r.h.moveTo(x + 20.0f, y);
    check(r.tl->grabbing(), "the grab is still going on");
    r.h.moveTo(x + 30.0f, y);
    r.h.up();
    r.h.frames(2);
    check(moved >= 3, "the grab reported every move, also after the clip was removed");
    check(released == 1 && !r.tl->grabbing(), "and the release, once");
    // The application may end a grab itself (Escape): its release then does nothing.
    r.tl->addClip(0, 1.0f, 6.0f, "b");
    r.h.relayout();
    r.clipPoint(0, 0, x, y);
    r.press(x, y);
    r.tl->cancelGrab();
    r.h.up();
    check(released == 1, "a cancelled grab has no release signal");
    r.tl->setClipGrab(false);
}

void testSnapIgnoresNaN()
{
    printf("snapping with NaN candidates, NaN times and no frame rate\n");
    ct::Vector<float> none;
    float out = 0;
    check(!findSnap(none, 1.0f, 0.5f, out), "no candidates, no snap");
    ct::Vector<float> some;
    some.push_back(1.0f);
    some.push_back(nanf(""));
    some.push_back(2.0f);
    check(findSnap(some, 1.1f, 0.5f, out) && out == 1.0f, "a NaN candidate does not win over the nearest one");
    ct::Vector<float> onlyNaN;
    onlyNaN.push_back(nanf(""));
    check(!findSnap(onlyNaN, 1.0f, 0.5f, out), "a NaN candidate alone is nowhere near");
    check(snapTime(onlyNaN, 1.0f, 0.5f) == 1.0f, "and the time stays what it was");
    check(!findSnap(some, nanf(""), 0.5f, out), "a NaN time snaps to nothing");
    check(snapFrame(1.234f, 0.0f) == 1.234f, "no snapping without a frame rate");
    check(snapFrame(1.234f, nanf("")) == 1.234f, "nor with a NaN one");
    check(snapFrame(nanf(""), 30.0f) != snapFrame(nanf(""), 30.0f), "a NaN time stays NaN, no crash");
}

// A split at a time that is not a number made two clips of NaN.
void testSplitAndRemoveArguments()
{
    printf("splitting and removing with bad arguments\n");
    Rig r;
    r.tl->addClip(0, 1.0f, 5.0f, "a");
    check(r.tl->splitClip(0, 0, nanf("")) == -1, "a NaN split is refused");
    check(r.tl->track(0).clips[0].end == 5.0f && r.tl->track(0).clips.size() == 1u, "and nothing changed");
    check(r.tl->splitClip(0, 0, 3.0f) == 1, "a split in the middle works");
    check(r.tl->splitClip(0, 5, 3.0f) == -1, "a bad clip index is refused");
    check(r.tl->splitClip(9, 0, 3.0f) == -1, "a bad track index is refused");
    check(r.tl->clipAt(0, nanf("")) == -1, "no clip at a NaN time");
    r.tl->removeClip(0, 99);
    r.tl->removeClip(-1, 0);
    r.tl->removeKeyframe(0, 5);
    r.tl->editKeyframeLabel(0, 5);
    r.tl->setSelectedTrack(99);
    r.h.frames(2);
}

// Thousands of clips, and times no editor should produce but a project file can.
void testThousandsAndOddTimes()
{
    printf("3000 clips, zero length, negative, huge and NaN times\n");
    Rig r;
    for (int i = 0; i < 3000; ++i)
    {
        const float start = static_cast<float>(i) * 0.05f;
        r.tl->addClip(i % 3, start, start + 0.04f + static_cast<float>(i % 7) * 0.2f, "clip Música", Color(80, 120, 180, 200));
    }
    r.tl->addClip(0, 3.0f, 3.0f, "zero");
    r.tl->addClip(0, -5.0f, -1.0f, "negative");
    r.tl->addClip(1, 5.0f, 1.0f, "backwards");
    r.tl->addClip(1, -1e30f, 1e30f, "enormous");
    r.tl->addClip(2, nanf(""), 4.0f, "nan start");
    r.tl->addClip(2, 4.0f, nanf(""), "nan end");
    r.tl->addClip(2, 1.0f, INFINITY, "infinite");
    r.tl->addKeyframe(0, nanf(""));
    r.tl->addKeyframe(0, -1e30f);
    r.tl->addKeyframe(1, INFINITY);
    r.h.relayout();
    r.h.frames(3);
    check(r.tl->track(0).clips.size() > 1000u, "the clips are there");
    // Click, drag, double-click and right-click all over the tracks.
    Rng rng(7);
    for (int i = 0; i < 400; ++i)
    {
        const float x = rng.range(0.0f, 1280.0f), y = rng.range(0.0f, 300.0f);
        r.h.moveTo(x, y);
        const int button = rng.below(3);
        r.h.io().keyCtrl = rng.chance(20);
        r.h.io().keyShift = rng.chance(20);
        r.h.down(button);
        if (rng.chance(70)) r.h.moveTo(rng.range(0.0f, 1280.0f), rng.range(0.0f, 300.0f));
        r.h.up(button);
        r.h.io().keyCtrl = r.h.io().keyShift = false;
    }
    r.h.frames(2);
}

void testZoomAndScrollExtremes()
{
    printf("zoom to the extremes, scroll past the ends, odd ranges\n");
    Rig r;
    for (int i = 0; i < 200; ++i)
        r.tl->addClip(i % 3, static_cast<float>(i), static_cast<float>(i) + 1.5f, "c");
    r.tl->setTimeRange(0.0f, 20.0f);
    r.h.relayout();
    float x = 0, y = 0;
    r.tl->pointAt(0, 10.0f, x, y);
    r.h.moveTo(x, y);
    for (int i = 0; i < 400; ++i) r.h.wheel(1.0f); // zoom in as far as it goes
    for (int i = 0; i < 400; ++i) r.h.wheel(-1.0f); // and out
    r.h.io().keyCtrl = true;
    for (int i = 0; i < 50; ++i) r.h.wheel(1.0f); // track height
    for (int i = 0; i < 100; ++i) r.h.wheel(-1.0f);
    r.h.io().keyCtrl = false;
    r.h.moveTo(50.0f, y); // over the headers: vertical scroll
    for (int i = 0; i < 50; ++i) r.h.wheel(-1.0f);
    for (int i = 0; i < 50; ++i) r.h.wheel(1.0f);
    r.tl->setTimeRange(1e7f, 1e7f + 0.5f); // precision of a float runs out
    r.tl->setTimeRange(-1e7f, -1e7f + 0.5f);
    r.tl->setTimeRange(0.0f, 0.0001f);
    r.tl->setTimeRange(5.0f, 5.0f);
    r.tl->setTimeRange(nanf(""), 5.0f);
    r.tl->setTimeRange(0.0f, INFINITY);
    r.h.frames(2);
    r.tl->setPlayhead(nanf(""));
    r.tl->setPlayhead(1e30f);
    r.tl->setMarkedRange(-1e30f, 1e30f);
    r.tl->setMarkedRange(nanf(""), 3.0f);
    r.tl->setFrameRate(0.0f);
    r.tl->setFrameRate(-30.0f);
    r.tl->setFrameRate(nanf(""));
    r.tl->setTrackHeight(nanf(""));
    r.tl->setVerticalScroll(1e30f);
    r.tl->setHeaderWidth(1e9f);
    r.tl->setEdgePadding(1e9f);
    r.h.frames(2);
    r.tl->setHeaderWidth(160.0f);
    r.tl->setEdgePadding(16.0f);
    r.h.resize(1.0f, 1.0f); // a window squeezed to a point
    r.h.resize(200.0f, 30.0f);
    r.h.resize(1280.0f, 720.0f);
}

// A keyframe label typed in place, with accents, and the timeline changing under it.
void testKeyframeLabelEdit()
{
    printf("a keyframe label edited in place while the timeline changes\n");
    Rig r;
    r.tl->addKeyframe(0, 4.0f);
    r.h.relayout();
    r.tl->editKeyframeLabel(0, 0);
    check(r.tl->editingKeyframeLabel(), "the editor is open");
    r.h.type("Música ação");
    r.tl->removeKeyframe(0, 0);
    r.h.key(Key::Return);
    r.h.frames(2);
    r.tl->addKeyframe(0, 4.0f);
    r.tl->editKeyframeLabel(0, 0);
    r.tl->clearTracks();
    r.h.key(Key::Escape);
    r.tl->addTrack("again", TrackKind::Video);
    r.h.frames(2);
}

} // namespace

// A scenario name on the command line runs only that one.
int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testRemoveDraggedClip);
    RUN(testRemoveLastClipWhileDragged);
    RUN(testRemoveClipOfDraggedGroup);
    RUN(testHandlerRemovesDraggedClip);
    RUN(testTimelineDeletedFromItsSignals);
    RUN(testClearAndReplaceTracksWhileDragging);
    RUN(testRemoveKeyframeWhileDragging);
    RUN(testEnvelopePointRemovedWhileDragging);
    RUN(testGrabModeClipRemoved);
    RUN(testSnapIgnoresNaN);
    RUN(testSplitAndRemoveArguments);
    RUN(testThousandsAndOddTimes);
    RUN(testZoomAndScrollExtremes);
    RUN(testKeyframeLabelEdit);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
