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

// The bands drawn where two clips of a track overlap, against a plain pairwise count: the
// painting looks for them with a sweep over the clips in order of start. The count follows the
// painting's own rule: an overlap of 0.01 s or more between clips that are not generators, whose
// span is not wholly outside the rectangle of the clips (the default header width and padding).
int expectedBands(Rig& r, int track, float viewStart, float viewEnd)
{
    const Rect b = r.tl->absoluteRect();
    const float left = b.x + 160.0f + 16.0f, width = b.w - 160.0f - 32.0f;
    const ct::Vector<TimelineClip>& clips = r.tl->track(track).clips;
    int expected = 0;
    for (size_t a = 0; a < clips.size(); ++a)
        for (size_t o = a + 1; o < clips.size(); ++o)
        {
            const float s = maxf(clips[a].start, clips[o].start);
            const float e = minf(clips[a].end, clips[o].end);
            if (e - s < 0.01f || clips[a].generator >= 0 || clips[o].generator >= 0) continue;
            const float x0 = left + (s - viewStart) / (viewEnd - viewStart) * width;
            const float x1 = left + (e - viewStart) / (viewEnd - viewStart) * width;
            if (x1 < b.x + 160.0f || x0 > b.x + b.w) continue;
            ++expected;
        }
    return expected;
}

// The quads of the band's colour (white, alpha 70) in the frame just drawn.
int bandQuads(Rig& r)
{
    r.h.frame();
    const DrawList& dl = GetDrawList();
    int vertices = 0;
    for (size_t i = 0; i < dl.vertices().size(); ++i)
    {
        const Color c = dl.vertices()[i].color;
        if (c.r == 255 && c.g == 255 && c.b == 255 && c.a == 70) ++vertices;
    }
    return vertices / 4;
}

// ---- scenarios ----
void testOverlapBandsMatchThePairwiseRule()
{
    printf("overlap bands equal the pairwise rule, in any order of the clips\n");
    Rng rng(5);
    for (int round = 0; round < 6; ++round)
    {
        Rig r;
        const int n = round < 3 ? 12 : 150;
        for (int i = 0; i < n; ++i)
        {
            const float start = rng.range(-2.0f, 24.0f);
            const float length = rng.chance(10) ? 0.005f : rng.range(0.2f, 4.0f);
            r.tl->addClip(0, start, start + length, "c");
            if (rng.chance(10)) r.tl->track(0).clips[static_cast<size_t>(i)].generator = 1;
        }
        if (round % 2) // in order of start, the usual case (the other rounds: any order)
        {
            ct::Vector<TimelineClip>& clips = r.tl->track(0).clips;
            for (size_t i = 0; i < clips.size(); ++i)
                for (size_t j = i + 1; j < clips.size(); ++j)
                    if (clips[j].start < clips[i].start)
                    {
                        TimelineClip t = clips[i];
                        clips[i] = clips[j];
                        clips[j] = t;
                    }
        }
        r.tl->setTimeRange(0.0f, 20.0f);
        r.h.relayout();
        const int expected = expectedBands(r, 0, 0.0f, 20.0f);
        check(expected > 0, "the clips do overlap");
        check(bandQuads(r) == expected, "the bands drawn are the overlaps of the pairwise rule");
    }
}

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

// ---- fades (TimelineClip::fadeIn, fadeOut, editableFades, onFadeChanged) ----

// The vertices of one colour in the frame just drawn.
int verticesOf(Rig& r, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    r.h.frame();
    const DrawList& dl = GetDrawList();
    int n = 0;
    for (size_t i = 0; i < dl.vertices().size(); ++i)
    {
        const Color c = dl.vertices()[i].color;
        if (c.r == red && c.g == green && c.b == blue && c.a == alpha) ++n;
    }
    return n;
}

bool frameIsFinite(Rig& r)
{
    r.h.frame();
    const DrawList& dl = GetDrawList();
    for (size_t i = 0; i < dl.vertices().size(); ++i)
    {
        const float x = dl.vertices()[i].x, y = dl.vertices()[i].y;
        if (!(x - x == 0.0f) || !(y - y == 0.0f)) return false;
    }
    return true;
}

// The window point of a fade handle: the top of the bar of the clip, at the end of the fade line.
void fadeHandlePoint(Rig& r, int track, int clip, bool out, float& x, float& y)
{
    const TimelineClip& c = r.tl->track(track).clips[clip];
    r.tl->pointAt(track, out ? c.end - c.fadeOut : c.start + c.fadeIn, x, y);
    y += -r.tl->trackHeight() * 0.5f + 3.0f + 5.0f;
}

void testFadeLinesAndHandles()
{
    printf("fade lines: drawn, dragged by their handles, the edge of the bar still trims\n");
    Rig r;
    r.tl->setSnapping(false);
    r.tl->addClip(0, 4.0f, 14.0f, "a");
    r.h.relayout();
    check(verticesOf(r, 120, 225, 255, 240) == 0, "a clip with no fade has no fade line");
    check(verticesOf(r, 70, 190, 235, 255) == 0, "and no handle");
    r.tl->track(0).clips[0].fadeIn = 3.0f;
    r.tl->track(0).clips[0].fadeOut = 2.0f;
    r.tl->markDirty();
    check(verticesOf(r, 120, 225, 255, 240) > 0, "the fades are drawn as lines");
    check(verticesOf(r, 0, 0, 0, 70) > 0, "with the bar above them darkened");
    check(verticesOf(r, 70, 190, 235, 255) == 0, "fades that cannot be dragged have no handle");
    // Not editable: a press where the handle would be moves the clip.
    int fades = 0;
    bool lastOut = false;
    float lastLength = -1.0f;
    r.tl->onFadeChanged.connect(
            [&](int t, int c, bool out, float length)
            {
                ++fades;
                lastOut = out;
                lastLength = length;
                r.checkIndex(t, c);
            });
    float x = 0, y = 0;
    fadeHandlePoint(r, 0, 0, false, x, y);
    r.press(x, y);
    r.h.moveTo(x + 53.0f, y);
    r.h.up();
    check(fades == 0 && r.tl->track(0).clips[0].start != 4.0f, "without editableFades the press there moves the clip");
    r.tl->track(0).clips[0].start = 4.0f;
    r.tl->track(0).clips[0].end = 14.0f;
    r.tl->track(0).clips[0].editableFades = true;
    r.tl->markDirty();
    check(verticesOf(r, 70, 190, 235, 255) == 8, "editable fades have two handles");
    // The fade in grows to the right by what the pointer moved.
    const float second = (r.tl->absoluteRect().w - 160.0f - 32.0f) / 20.0f; // pixels of one second
    r.changed = r.edited = 0;
    fadeHandlePoint(r, 0, 0, false, x, y);
    r.press(x, y);
    r.h.moveTo(x + second, y);
    r.h.moveTo(x + 2.0f * second, y);
    check(fades == 2 && !lastOut && fabsf(lastLength - 5.0f) < 0.05f, "dragging the handle of the fade in asks for a longer fade");
    check(fabsf(r.tl->track(0).clips[0].fadeIn - 5.0f) < 0.05f, "and the clip has it while the drag goes on");
    r.h.moveTo(x + 40.0f * second, y);
    check(lastLength == 10.0f, "a fade is never longer than its clip");
    r.h.moveTo(x - 40.0f * second, y);
    check(lastLength == 0.0f, "nor shorter than nothing");
    r.h.up();
    check(r.edited == 1 && r.changed == 0, "the drag ends with onClipEdited, once, and the clip did not move");
    check(r.tl->track(0).clips[0].start == 4.0f && r.tl->track(0).clips[0].end == 14.0f, "its times are the same");
    // The fade out grows to the left.
    r.tl->track(0).clips[0].fadeIn = 3.0f;
    fades = 0;
    fadeHandlePoint(r, 0, 0, true, x, y);
    r.press(x, y);
    r.h.moveTo(x - second, y);
    r.h.up();
    check(fades == 1 && lastOut && fabsf(lastLength - 3.0f) < 0.05f, "the handle of the fade out, dragged left, asks for a longer fade out");
    // A fade of nothing keeps its handle off the edge, and the edge still trims.
    r.tl->track(0).clips[0].fadeIn = 0.0f;
    r.tl->track(0).clips[0].fadeOut = 0.0f;
    fades = r.changed = 0;
    r.tl->pointAt(0, 4.0f, x, y);
    r.press(x + 1.0f, y);
    r.h.moveTo(x + 1.0f + second, y);
    r.h.up();
    check(fades == 0 && r.changed > 0 && fabsf(r.tl->track(0).clips[0].start - 5.0f) < 0.05f, "the left edge of the bar trims the clip, with the handle next to it");
    fadeHandlePoint(r, 0, 0, false, x, y);
    r.press(x + 9.0f, y);
    r.h.moveTo(x + 9.0f + second, y);
    r.h.up();
    check(fades == 1 && fabsf(lastLength - 1.0f) < 0.05f, "and the handle of a fade of nothing is 9 pixels inside it");
    // The application answers with the length it keeps (a crossfade makes it longer than asked).
    r.tl->onFadeChanged.connect([&](int t, int c, bool out, float) { (out ? r.tl->track(t).clips[c].fadeOut : r.tl->track(t).clips[c].fadeIn) = 2.5f; });
    fadeHandlePoint(r, 0, 0, false, x, y);
    r.press(x, y);
    r.h.moveTo(x + second, y);
    r.h.up();
    check(r.tl->track(0).clips[0].fadeIn == 2.5f, "the length the application gave back in the signal is the one that stays");
    check(r.badIndex == 0, "no signal named a clip that is not there");
}

void testFadeHostile()
{
    printf("fades that are not numbers, a clip or the timeline gone from onFadeChanged\n");
    const float odd[] = { nanf(""), INFINITY, -INFINITY, -3.0f, 1e30f, 0.0f, 1e-30f };
    for (size_t i = 0; i < sizeof(odd) / sizeof(odd[0]); ++i)
        for (size_t j = 0; j < sizeof(odd) / sizeof(odd[0]); ++j)
        {
            Rig r;
            r.tl->addClip(0, 2.0f, 9.0f, "a");
            r.tl->addClip(1, 3.0f, 3.0f, "zero");
            r.tl->addClip(1, 9.0f, 4.0f, "backwards");
            for (int t = 0; t < 2; ++t)
                for (size_t c = 0; c < r.tl->track(t).clips.size(); ++c)
                {
                    r.tl->track(t).clips[c].fadeIn = odd[i];
                    r.tl->track(t).clips[c].fadeOut = odd[j];
                    r.tl->track(t).clips[c].editableFades = true;
                }
            r.h.relayout();
            check(frameIsFinite(r), "every vertex is a number");
            // press and drag where the handles are, and all along the top of the bars
            float x = 0, y = 0;
            r.tl->pointAt(0, 2.0f, x, y);
            y += -r.tl->trackHeight() * 0.5f + 8.0f;
            for (float dx = -4.0f; dx < 400.0f; dx += 37.0f)
            {
                r.press(x + dx, y);
                r.h.moveTo(x + dx + 30.0f, y);
                r.h.moveTo(x + dx - 500.0f, y + 3.0f);
                r.h.up();
            }
            for (int t = 0; t < 2; ++t)
                for (size_t c = 0; c < r.tl->track(t).clips.size(); ++c)
                {
                    const TimelineClip& clip = r.tl->track(t).clips[c];
                    // a fade the drag wrote is a length; one it did not touch is the one given
                    check(clip.fadeIn == odd[i] || (odd[i] != odd[i] && clip.fadeIn != clip.fadeIn) || (clip.fadeIn >= 0.0f && clip.fadeIn < 1e13f), "no drag left a fade in that is not a length");
                }
            check(frameIsFinite(r), "also after the drags");
        }
    // a window of one pixel, and a view of nothing
    {
        Rig r;
        r.tl->addClip(0, 2.0f, 9.0f, "a");
        r.tl->track(0).clips[0].fadeIn = 2.0f;
        r.tl->track(0).clips[0].fadeOut = 2.0f;
        r.tl->track(0).clips[0].editableFades = true;
        r.h.resize(1.0f, 1.0f);
        r.h.frames(2);
        r.h.resize(1280.0f, 720.0f);
        r.tl->setTimeRange(5.0f, 5.0f);
        r.h.frames(2);
        r.tl->setTimeRange(0.0f, 1e-6f);
        check(frameIsFinite(r), "a view of a microsecond");
        r.tl->setTimeRange(-1e9f, 1e9f);
        check(frameIsFinite(r), "a view of sixty years");
    }
    // the handler takes the clip away, the application does between two moves, the handler takes the timeline out of the tree
    for (int variant = 0; variant < 3; ++variant)
    {
        Rig r;
        r.tl->setSnapping(false);
        r.tl->addClip(0, 4.0f, 14.0f, "a");
        r.tl->addClip(0, 15.0f, 18.0f, "b");
        r.tl->track(0).clips[0].fadeIn = 3.0f;
        r.tl->track(0).clips[0].editableFades = true;
        r.h.relayout();
        Timeline* self = r.tl;
        int fades = 0;
        r.tl->onFadeChanged.connect(
                [&](int t, int c, bool, float)
                {
                    ++fades;
                    r.checkIndex(t, c);
                    if (variant == 0) r.tl->removeClip(t, c);
                    if (variant == 2 && self)
                    {
                        Timeline* dead = self;
                        self = nullptr;
                        r.box->removeChild(dead);
                    }
                });
        float x = 0, y = 0;
        fadeHandlePoint(r, 0, 0, false, x, y);
        r.press(x, y);
        r.h.moveTo(x + 20.0f, y);
        if (variant == 1) r.tl->removeClip(0, 0);
        r.h.moveTo(x + 40.0f, y);
        r.h.moveTo(x + 60.0f, y);
        r.h.up();
        r.h.frames(2);
        check(fades == 1, "the drag ended with its clip (or its timeline): one signal and no more");
        check(r.badIndex == 0, "no signal named a clip that is not there");
        if (variant == 2) check(self == nullptr, "the handler removed the timeline");
        else
        {
            check(r.edited == 0, "a clip that is gone is not reported as edited");
            check(r.tl->track(0).clips.size() == 1u && r.tl->track(0).clips[0].start == 15.0f && r.tl->track(0).clips[0].fadeIn == 0.0f, "the clip that slid into its place was not given a fade");
        }
    }
}

// ---- more curves on a clip (TimelineClip::lines, envelopeColor, envelopeLabel) ----

// The vertices of a colour whatever their alpha (the edge of a stroke fades to nothing).
int verticesOfRgb(Rig& r, uint8_t red, uint8_t green, uint8_t blue)
{
    r.h.frame();
    const DrawList& dl = GetDrawList();
    int n = 0;
    for (size_t i = 0; i < dl.vertices().size(); ++i)
    {
        const Color c = dl.vertices()[i].color;
        if (c.r == red && c.g == green && c.b == blue) ++n;
    }
    return n;
}

CurveKey keyAt(float time, float value)
{
    CurveKey k;
    k.time = time;
    k.value = value;
    return k;
}

TimelineClip::Line lineOf(float v0, float v1, const Color& color, const char* label)
{
    TimelineClip::Line line;
    line.keys.push_back(keyAt(0.0f, v0));
    line.keys.push_back(keyAt(10.0f, v1));
    line.color = color;
    line.label = label;
    return line;
}

void testClipLines()
{
    printf("more curves on a clip: drawn, each in its colour, named in a legend, and never under the mouse\n");
    const Color red(201, 37, 41, 255), green(33, 197, 43, 255), gold(203, 171, 31, 255);
    Rig r;
    r.tl->setSnapping(false);
    r.tl->addClip(0, 4.0f, 14.0f, "a");
    r.h.relayout();
    check(verticesOfRgb(r, 201, 37, 41) == 0, "no line, nothing in its colour");
    r.tl->track(0).clips[0].lines.push_back(lineOf(0.2f, 0.9f, red, "Low cut"));
    r.tl->track(0).clips[0].lines.push_back(lineOf(0.5f, 0.5f, green, "Pan"));
    r.tl->markDirty();
    const int bending = verticesOfRgb(r, 201, 37, 41), flat = verticesOfRgb(r, 33, 197, 43);
    check(bending > 0 && flat > 0, "each line is drawn in its own colour");
    check(flat < bending && flat <= 16, "a flat line is one stretch between two points, not a point for every other pixel");
    // the envelope has its colour too
    TimelineClip& c = r.tl->track(0).clips[0];
    c.envelope.push_back(keyAt(0.0f, -40.0f));
    c.envelope.push_back(keyAt(10.0f, 12.0f));
    r.tl->markDirty();
    check(verticesOfRgb(r, 203, 171, 31) == 0, "the envelope is white until it is given a colour");
    r.tl->track(0).clips[0].envelopeColor = gold;
    r.tl->markDirty();
    check(verticesOfRgb(r, 203, 171, 31) > 0, "envelopeColor is the colour of the envelope's line");
    // a line with no keys, or with a range of nothing, draws nothing
    r.tl->track(0).clips[0].lines[0].keys.clear();
    r.tl->track(0).clips[0].lines[1].max = r.tl->track(0).clips[0].lines[1].min;
    r.tl->markDirty();
    check(verticesOfRgb(r, 201, 37, 41) == 0 && verticesOfRgb(r, 33, 197, 43) == 0, "a line with no keys or with max == min draws nothing");
    // the legend: only on a bar tall enough for it
    r.tl->track(0).clips[0].lines[0] = lineOf(0.2f, 0.9f, red, "Low cut");
    r.tl->track(0).clips[0].lines[1] = lineOf(0.5f, 0.5f, green, "Pan");
    r.tl->markDirty();
    const int low = verticesOfRgb(r, 201, 37, 41);
    r.tl->setTrackHeight(60.0f);
    r.h.relayout();
    r.tl->track(0).clips[0].lines[0].keys.clear(); // only the legend could be in this colour now
    r.tl->markDirty();
    check(verticesOfRgb(r, 201, 37, 41) == 0, "a line with no keys is not in the legend");
    r.tl->track(0).clips[0].lines[0] = lineOf(0.2f, 0.2f, red, "Low cut");
    r.tl->markDirty();
    const int tall = verticesOfRgb(r, 201, 37, 41);
    r.tl->track(0).clips[0].lines[0].label = "";
    r.tl->markDirty();
    check(tall - verticesOfRgb(r, 201, 37, 41) == 4, "the legend of a tall bar has a swatch in the colour of each line that has a label");
    check(low > 0, "a low bar draws the lines without a legend");

    // The lines never take the mouse: the same presses do the same with and without them.
    for (int editable = 0; editable < 2; ++editable)
    {
        float starts[2] = { 0, 0 };
        size_t points[2] = { 0, 0 };
        int envelopeSignals[2] = { 0, 0 };
        for (int withLines = 0; withLines < 2; ++withLines)
        {
            Rig q;
            q.tl->setSnapping(false);
            q.tl->setTrackHeight(60.0f);
            q.tl->addClip(0, 4.0f, 14.0f, "a");
            TimelineClip& clip = q.tl->track(0).clips[0];
            clip.editableEnvelope = editable != 0;
            clip.envelope.push_back(keyAt(0.0f, 0.0f));
            clip.envelope.push_back(keyAt(10.0f, 0.0f));
            if (withLines)
            {
                // a line through the middle of the bar, where the presses go, and its ends on the bar's edges
                clip.lines.push_back(lineOf(0.5f, 0.5f, red, "mid"));
                clip.lines.push_back(lineOf(0.0f, 1.0f, green, "ramp"));
            }
            q.tl->onEnvelopeChanged.connect([&, withLines](int, int) { ++envelopeSignals[withLines]; });
            q.h.relayout();
            float x = 0, y = 0;
            q.clipPoint(0, 0, x, y); // on the flat line and on the middle of the ramp
            q.press(x, y);
            q.h.moveTo(x + 40.0f, y);
            q.h.up();
            q.h.moveTo(x - 100.0f, y);
            q.h.down();
            q.h.up();
            q.h.down(); // a double click away from the envelope: a point of the envelope when it is editable
            q.h.up();
            starts[withLines] = q.tl->track(0).clips[0].start;
            points[withLines] = q.tl->track(0).clips[0].envelope.size();
            check(q.badIndex == 0, "no signal named a clip that is not there");
        }
        check(starts[0] == starts[1] && points[0] == points[1] && envelopeSignals[0] == envelopeSignals[1],
                "the clip moved and the envelope changed the same with the lines as without");
        check(starts[0] != 4.0f, "the press on a line moved the clip under it");
    }
}

void testClipLinesHostile()
{
    printf("curves with keys, ranges and speeds that are not numbers\n");
    const float odd[] = { nanf(""), INFINITY, -INFINITY, -1e30f, 1e30f, 0.0f };
    const size_t n = sizeof(odd) / sizeof(odd[0]);
    for (size_t i = 0; i < n; ++i)
        for (size_t j = 0; j < n; ++j)
        {
            Rig r;
            r.tl->setTrackHeight(60.0f);
            r.tl->addClip(0, 2.0f, 12.0f, "a");
            r.tl->addClip(1, 2.0f, 12.0f, "speed");
            r.tl->addClip(2, 2.0f, 12.0f, "range");
            TimelineClip::Line line;
            line.label = "odd";
            line.keys.push_back(keyAt(1.0f, 0.2f));
            line.keys.push_back(keyAt(odd[i], odd[j])); // a time and a value that are not numbers
            line.keys.push_back(keyAt(0.5f, 0.9f));     // and a key out of order after it
            line.keys.push_back(keyAt(odd[j], 0.4f));
            line.keys.push_back(keyAt(8.0f, odd[i]));
            r.tl->track(0).clips[0].lines.push_back(line);
            r.tl->track(0).clips[0].envelope = line.keys;
            r.tl->track(0).clips[0].envelopeLabel = "env";
            // (not editable: the squares of the points of an envelope are drawn where the keys are, and a key
            // whose time is not a number is a square that is not one; that is the envelope's, not the lines')
            r.tl->track(1).clips[0].lines.push_back(lineOf(0.0f, 1.0f, Color(9, 9, 9, 255), "s"));
            r.tl->track(1).clips[0].speed = odd[i];
            r.tl->track(1).clips[0].offset = odd[j];
            r.tl->track(2).clips[0].lines.push_back(lineOf(0.0f, 1.0f, Color(9, 9, 9, 255), "r"));
            r.tl->track(2).clips[0].lines[0].min = odd[i];
            r.tl->track(2).clips[0].lines[0].max = odd[j];
            r.tl->track(2).clips[0].envelope.push_back(keyAt(0.0f, 0.0f));
            r.tl->track(2).clips[0].envelopeMin = odd[j];
            r.tl->track(2).clips[0].envelopeMax = odd[i];
            r.h.relayout();
            check(frameIsFinite(r), "every vertex is a number");
            r.tl->setTimeRange(0.0f, 1e-5f);
            check(frameIsFinite(r), "zoomed in to ten microseconds");
            r.tl->setTimeRange(-1e9f, 1e9f);
            check(frameIsFinite(r), "and out to sixty years");
        }
    // a hundred thousand keys in one clip, in a window of any size: the points of a stroke are bounded by the keys and the pixels
    {
        Rig r;
        r.tl->addClip(0, 0.0f, 20.0f, "many");
        TimelineClip::Line line;
        line.color = Color(201, 37, 41, 255);
        for (int k = 0; k < 100000; ++k) line.keys.push_back(keyAt(static_cast<float>(k) * 0.0002f, (k % 2) ? 1.0f : 0.0f));
        r.tl->track(0).clips[0].lines.push_back(line);
        r.h.relayout();
        check(frameIsFinite(r), "100000 keys");
        r.tl->track(0).clips[0].lines[0].keys.resize(40);
        // keys that go back and forth in time: the pen does not go back, so the stroke is no longer than the bar
        for (size_t k = 0; k < 40; ++k) r.tl->track(0).clips[0].lines[0].keys[k].time = (k % 2) ? 19.0f : 1.0f;
        r.tl->markDirty();
        check(verticesOfRgb(r, 201, 37, 41) < 40 * 8 + 1300 * 4, "keys out of order do not multiply the points of the stroke");
        r.h.resize(1.0f, 1.0f);
        r.h.frames(2);
        r.h.resize(30000.0f, 40.0f);
        check(frameIsFinite(r), "a window 30000 pixels wide");
    }
}

} // namespace

// A scenario name on the command line runs only that one.
int main(int argc, char** argv)
{
    const char* only = argc > 1 ? argv[1] : nullptr;
#define RUN(fn) \
    if (!only || strcmp(only, #fn) == 0) fn()
    RUN(testOverlapBandsMatchThePairwiseRule);
    RUN(testRemoveDraggedClip);
    RUN(testRemoveLastClipWhileDragged);
    RUN(testRemoveClipOfDraggedGroup);
    RUN(testHandlerRemovesDraggedClip);
    RUN(testTimelineDeletedFromItsSignals);
    RUN(testClearAndReplaceTracksWhileDragging);
    RUN(testRemoveKeyframeWhileDragging);
    RUN(testEnvelopePointRemovedWhileDragging);
    RUN(testGrabModeClipRemoved);
    RUN(testThousandsAndOddTimes);
    RUN(testZoomAndScrollExtremes);
    RUN(testSnapIgnoresNaN);
    RUN(testSplitAndRemoveArguments);
    RUN(testKeyframeLabelEdit);
    RUN(testFadeLinesAndHandles);
    RUN(testFadeHostile);
    RUN(testClipLines);
    RUN(testClipLinesHostile);
    shutdown();
    if (failures()) printf("%d checks failed\n", failures());
    else
        printf("ok\n");
    return failures() ? 1 : 0;
}
