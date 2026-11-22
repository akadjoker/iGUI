#pragma once

#include "ChartWidgets.hpp"
#include "ScrollWidgets.hpp"
#include "Signal.hpp"
#include "Widget.hpp"
#include <ct/function.hpp>
#include <ct/vector.hpp>
#include <igui/widgets/String.hpp>

// ═════════════════════════════════════════════════════════════════════════════
//  Timeline — Multi-track timeline with keyframes, clips, scrubbing, zoom
//
//  Features:
//  - Multiple named tracks with independent colours
//  - Keyframe markers (diamond) per track
//  - Clip bars (time ranges) per track, draggable + resizable
//  - Playhead with scrubbing
//  - Several clips selected at once: Ctrl+click toggles one, Shift+click adds one, a drag on the
//  empty
//    part of a track selects the clips inside the rectangle, and dragging a selected clip moves all
//    of them
//  - Grab mode (setClipGrab): a press on a clip goes to the application with the pointer's moves
//    and the release, and the clip is not moved
//  - A trace on a clip: a line the application draws over the waveform (a scratch)
//  - Horizontal zoom (scroll) and pan (middle-drag)
//  - Snapping while dragging: clip edges, keyframes, the playhead and zero within 8 pixels, whole
//  frames otherwise
//  - Keyframes can carry a label (a marker's name) and be renamed in place with a double-click
//  - Track header area with labels
//  - Time ruler with auto-scaled tick marks
//
//  Usage:
//    auto* tl = parent->createChild<Timeline>();
//    int t1 = tl->addTrack("Position");
//    tl->addKeyframe(t1, 0.0f);
//    tl->addClip(t1, 0.5f, 1.5f, "Walk");
//    tl->onPlayheadChanged.connect([](float t) { ... });
// ═════════════════════════════════════════════════════════════════════════════

namespace ig
{
namespace retained
{
enum class KeyShape
{
    Diamond,
    Square,
    Circle
};

struct TimelineKeyframe
{
    float time = 0;
    bool selected = false;
    int user = -1; // the application's own id for the key; the widget only carries it
    String label;  // drawn beside the diamond (a marker's name)
    KeyShape shape = KeyShape::Diamond; // the application's meaning (e.g. its interpolation)
};

struct TimelineClip
{
    float start = 0;
    float end = 1;
    String label;
    Color color = Color(80, 120, 180, 200);
    bool selected = false;

    // The timeline only carries these; the application gives them meaning.
    int id = -1;        // unique in the timeline, set by addClip
    int media = -1;     // what the clip plays
    float offset = 0;   // where in the media the clip starts (seconds)
    int transition = 0; // how it comes in over the clip before it, when they overlap on a track
                        // (the application's list)
    float speed = 1; // how fast the media plays: it takes (length / speed) seconds on the timeline
                     // per second of media
    float sourceLength = 0; // length of the media, 0 = unlimited (limits trimming)
    int link = -1;          // clips sharing a link are moved and trimmed together
    int user = -1;
    // What the application says about the clip's media, and the timeline shows: Ready draws the
    // clip as usual; Loading greys it, with a bar along its bottom filled to `progress` (0 to 1);
    // Failed paints it dark red with a cross. The widget only draws it, the application sets it.
    enum class Status : unsigned char
    {
        Ready,
        Loading,
        Failed
    };
    Status status = Status::Ready;
    float progress = 0; // of Loading, 0 to 1
    int generator =
            -1; // an overlay made by the application (a title, a balloon): no media, drawn on top
    // A curve over the media's own time (seconds), drawn on the clip between envelopeMin and
    // envelopeMax.
    ct::Vector<CurveKey> envelope;
    bool editableEnvelope =
            false;          // double-click on the clip adds a point to the curve, drag moves it
    float envelopeRest = 0; // the value a new curve starts flat at
    float envelopeMin = -40;
    float envelopeMax = 12;
    // The colour of the envelope's line, and its name in the legend of the clip (below).
    Color envelopeColor = Color(255, 255, 255, 230);
    String envelopeLabel;
    // More curves of the clip, to look at while the envelope is the one being edited. Each is
    // drawn like the envelope (over the media's own time, between its own min and max), thinner
    // and in its own colour, under the envelope; it has no points and never takes the mouse: a
    // press on it is a press on the clip. With a tall enough bar the clip names what it shows
    // along its top: the envelope's label first, then the label of every line that has keys.
    struct Line
    {
        ct::Vector<CurveKey> keys;
        float min = 0;
        float max = 1;
        Color color = Color(255, 255, 255, 150);
        String label;
    };
    ct::Vector<Line> lines;
    float params[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0 }; // free for the application (effects, volume); copied when a clip is split
    // A curve per entry of params over the media's own time (seconds); when there is one it
    // replaces the constant.
    ct::Vector<CurveKey> automation[12];
    // A line the application lays over the clip, on top of the waveform and the curve: points of
    // the media's own time (seconds, like the envelope's) and a height from 0 (the bottom of the
    // bar) to 1 (the top), straight between points. The stretch between the first and the last
    // point is tinted. It is drawn only: the timeline never edits it, and a press on it is a press
    // on the clip (the path of a scratch, for instance).
    struct TracePoint
    {
        float time = 0;
        float value = 0;
    };
    ct::Vector<TracePoint> trace;
    Color traceColor = Color(255, 70, 170, 255);
    float traceBaseline = -1; // a height 0..1 drawn as a thin line across the tinted stretch; < 0 none
    // The fades the application says the clip has, in seconds of the timeline from its start and
    // to its end. They are drawn as the line the loudness follows over them (equal power, like the
    // mixer's), with the bar above the line darkened. With editableFades a handle sits at the top
    // end of each line: dragging it asks for another length (onFadeChanged) and the application
    // gives back the length it kept (a crossfade can make it longer than asked).
    float fadeIn = 0;
    float fadeOut = 0;
    bool editableFades = false;
};

/// A box the application lays over a stretch of time, from the row of one track to the row of
/// another (or over one row): what two clips do together, like a transition. The timeline draws
/// it over the clips: a tinted box, the lines the application gives (x from 0 at its start to 1 at
/// its end, y from 0 at its bottom to 1 at its top), and along its top a tab with a dot and the
/// label. A press on the tab goes to the application (onSpanPressed); the two grips at the top
/// corners are dragged to move its edges (onSpanEdgeDragged, with the time snapped like a clip's
/// edge: the application decides what that does). The rest of the box lets the mouse through to the
/// clips under it.
struct TimelineSpan
{
    int id = -1;
    int trackA = 0;
    int trackB = 0;
    float start = 0;
    float end = 0;
    String label;
    String tooltip; // shown after a moment over the tab
    Color color = Color(255, 255, 255, 255);
    int mark = -1; // a dot before the label: 0 red, 1 amber, 2 green; -1 none
    struct Line
    {
        ct::Vector<Vec2> points;
        Color color = Color(255, 255, 255, 255);
        float thickness = 1.5f;
    };
    ct::Vector<Line> lines;
};

/// @brief The candidate nearest to t when it lies within threshold of it (false when none does).
bool findSnap(const ct::Vector<float>& candidates, float t, float threshold, float& out);
/// @brief t moved to the nearest candidate within threshold, or t itself when none is that close.
float snapTime(const ct::Vector<float>& candidates, float t, float threshold);
/// @brief t rounded to a whole frame at fps (t itself when fps is not positive).
float snapFrame(float t, float fps);

/// A clip by its track and its index in the track.
struct TimelineClipRef
{
    int track = 0;
    int clip = 0;
};

/// A keyframe by its track and its index in the track.
struct TimelineKeyRef
{
    int track = 0;
    int key = 0;
};

/// What a track carries; the timeline draws the same, the application decides.
enum class TrackKind
{
    Generic,
    Video,
    Audio
};

struct TimelineTrack
{
    String name;
    Color color = Color(120, 140, 180, 255);
    ct::Vector<TimelineKeyframe> keyframes;
    ct::Vector<TimelineClip> clips;
    bool muted = false;
    bool locked = false;
    int parent = -1;
    bool expanded = true;
    TrackKind kind = TrackKind::Generic;
    bool rulerMarks =
            false; // the keyframes are markers: a flag with the label is also drawn on the ruler
    bool keysInert = false; // the keyframes are drawn (along the bottom of the row) and snapped to, but
                            // cannot be picked or moved: a click goes to the clip (a beat grid)
};

class Timeline : public Widget
{
public:
    IG_WIDGET_CLASS(Timeline, Widget);

    Timeline();
    /// Joint IDs are track indices; removing tracks invalidates subsequent IDs.
    int addJoint(const String& name, int parent = -1);
    void setExpanded(int joint, bool expanded);
    ct::Vector<int> visibleTracks() const;
    void setHeaderWidth(float width);
    /// Height of a track row in pixels (clamped to 20-200). The clips, their waveforms and curves grow with it;
    /// Ctrl + mouse wheel changes it too and emits onTrackHeightChanged (Shift + wheel zooms the time, the width).
    void setTrackHeight(float height);
    float trackHeight() const { return kTrackH; }
    /// Space in pixels inside both timeline edges (minimum 8 for full diamonds).
    void setEdgePadding(float pixels);
    float edgePadding() const
    {
        return edgePadding_;
    }
    /// Header buttons: M toggles mute, L toggles lock. Emitted with the track id and new state.
    Signal<int, bool> onTrackMuteChanged;
    Signal<int, bool> onTrackLockChanged;
    Signal<int, int, float> onKeyframeMoved;
    /// Emitted once when a dragged keyframe is released: track id, key index, start time, end time.
    Signal<int, int, float, float> onKeyframeDropped;
    Signal<int, int> onKeyframeAdded;

    /// @brief Add a named track with optional color.
    int addTrack(const String& name, const Color& color = Color(120, 140, 180, 255));
    /// @brief Add a track of a kind (video or audio) with a default colour for it.
    int addTrack(const String& name, TrackKind kind);
    /// @brief Mute or unmute a track (emits onTrackMuteChanged).
    void setTrackMuted(int trackId, bool muted);
    /// @brief Insert a track of a kind at a row (0 = top). Later track ids move down by one.
    int insertTrack(int index, const String& name, TrackKind kind);
    /// @brief Remove a track by ID.
    void removeTrack(int trackId);
    /// @brief The track whose header was clicked, -1 if none.
    int selectedTrack() const
    {
        return selectedTrack_;
    }
    void setSelectedTrack(int trackId);
    /// @brief Remove all tracks.
    void clearTracks();

    /// @brief Get the number of tracks.
    int trackCount() const
    {
        return static_cast<int>(tracks_.size());
    }
    /// @brief Every track, to keep a copy of the whole timeline (undo) and put it back.
    const ct::Vector<TimelineTrack>& allTracks() const
    {
        return tracks_;
    }
    void setAllTracks(const ct::Vector<TimelineTrack>& tracks);
    /// @brief Get a mutable track reference.
    TimelineTrack& track(int id)
    {
        return tracks_[id];
    }
    /// @brief Get a const track reference.
    const TimelineTrack& track(int id) const
    {
        return tracks_[id];
    }

    /// @brief Add a keyframe at a time position.
    int addKeyframe(int trackId, float time);
    /// @brief Remove a keyframe by index.
    void removeKeyframe(int trackId, int keyIdx);

    /// @brief Add a clip spanning a time range.
    int addClip(int trackId, float start, float end, const String& label = "",
            const Color& color = Color(80, 120, 180, 200));
    /// @brief Remove a clip by index.
    void removeClip(int trackId, int clipIdx);
    /// @brief Cut a clip in two at time t. Returns the index of the second part (-1 if t is not
    /// inside).
    int splitClip(int trackId, int clipIdx, float t);
    /// @brief The first selected clip. False when none is selected.
    bool selectedClip(int& trackId, int& clipIdx) const;
    /// @brief Every selected clip, track by track, in the order of the clips.
    void selectedClips(ct::Vector<TimelineClipRef>& out) const;
    int selectedClipCount() const;
    /// @brief Every selected keyframe, track by track, in the order of the keyframes. Ctrl+click
    /// toggles a key,
    ///        Shift+click adds one, a rectangle on an empty part of a track selects the keys (and
    ///        clips) inside it, and dragging a selected key moves all the selected keys by the same
    ///        amount.
    void selectedKeys(ct::Vector<TimelineKeyRef>& out) const;
    int selectedKeyCount() const;
    void clearSelection();
    /// @brief First clip on the track covering time t, or -1. Later clips win.
    int clipAt(int trackId, float t) const;
    /// @brief The mouse wheel scrolls the tracks up and down everywhere and Ctrl+wheel or
    /// Shift+wheel zooms the time
    ///        under the pointer (true), or the wheel zooms the time and only scrolls over the track
    ///        names (false, the default).
    void setWheelScrollsTracks(bool on)
    {
        wheelScrollsTracks_ = on;
    }
    bool wheelScrollsTracks() const
    {
        return wheelScrollsTracks_;
    }
    /// @brief How far the tracks are scrolled down, in pixels.
    float verticalScroll() const
    {
        return verticalScroll_;
    }
    void setVerticalScroll(float pixels);
    /// @brief Snap clips and keyframes while dragging: to the edges of the clips on every track, to
    /// keyframes, to
    ///        the playhead, to zero and to the application's own targets when one is within 8
    ///        pixels, and to whole frames otherwise.
    void setSnapping(bool on)
    {
        snapping_ = on;
    }
    bool snapping() const
    {
        return snapping_;
    }
    /// @brief Extra times to snap to (the end of the sequence); the application refreshes them when
    /// they change.
    void setSnapTargets(const ct::Vector<float>& targets)
    {
        snapTargets_ = targets;
    }
    const ct::Vector<float>& snapTargets() const
    {
        return snapTargets_;
    }
    /// @brief The text shown after a moment of hovering over a clip (track, clip index); empty text
    /// shows nothing.
    void setClipTooltip(ct::Function<String(int, int)> provider)
    {
        clipTooltip_ = std::move(provider);
    }
    /// @brief A mode in which a left press on a clip is handed to the application instead of
    /// moving, trimming or editing it: the clip is selected, and onClipGrabbed, onClipGrabMoved
    /// (every pointer move, also when the pointer has left the clip or the widget) and
    /// onClipGrabReleased follow until the button goes up. Presses elsewhere (the ruler, the
    /// headers, the empty part of a track) behave as usual. Turning the mode off ends a grab.
    void setClipGrab(bool on);
    bool clipGrab() const
    {
        return grabClips_;
    }
    /// @brief Ends a grab that is going on without a release signal (the application stopped it:
    /// Escape, the window lost the focus, the clip is gone). The button may still be down; its
    /// release does nothing.
    void cancelGrab();
    /// @brief Whether a grab is going on.
    bool grabbing() const
    {
        return dragMode_ == DragMode::Grab;
    }

    /// @brief Opens a small text field over a keyframe to type its label. Enter or leaving the
    /// field keeps the text
    ///        (onKeyframeLabelEdited), Escape drops it.
    void editKeyframeLabel(int trackId, int keyIdx);
    bool editingKeyframeLabel() const
    {
        return labelTrack_ >= 0;
    }

    /// @brief Window-space point of a time on a track's row (the centre of the row), or on the
    /// ruler when
    ///        trackId is -1; the tracks scroll so that the row is in view. False when the track is
    ///        not shown (unknown, or under a collapsed parent) or the time is outside the visible
    ///        range.
    bool pointAt(int trackId, float time, float& x, float& y);

    /// @brief Set the playhead position in seconds.
    void setPlayhead(float t)
    {
        playhead_ = t;
        markDirty();
    }
    /// @brief Get the playhead position in seconds.
    float playhead() const
    {
        return playhead_;
    }

    /// @brief Set the visible time range.
    void setTimeRange(float start, float end);
    /// @brief Get the visible start time.
    float viewStart() const
    {
        return viewStart_;
    }
    /// @brief Get the visible end time.
    float viewEnd() const
    {
        return viewEnd_;
    }

    /// @brief Shade the range between two times (an in/out selection); out <= in hides it.
    void setMarkedRange(float in, float out)
    {
        rangeIn_ = in;
        rangeOut_ = out;
        markDirty();
    }
    void clearMarkedRange()
    {
        rangeIn_ = 0.0f;
        rangeOut_ = -1.0f;
        markDirty();
    }

    /// @brief Set the frame rate (fps) for tick marks.
    void setFrameRate(float fps)
    {
        fps_ = fps;
    }
    /// @brief Get the frame rate.
    float frameRate() const
    {
        return fps_;
    }

    /// @brief Where the loudness of a clip's sound comes from, to draw its waveform: media, the
    /// part of the media (seconds), and the answer, 0 to 1 (or negative when it is not known yet).
    void setPeakProvider(ct::Function<float(int, float, float)> provider)
    {
        peakProvider_ = std::move(provider);
        markDirty();
    }

    /// @brief The boxes laid over the clips (see TimelineSpan). The application sets them again
    /// whenever what they show changes.
    void setSpans(const ct::Vector<TimelineSpan>& spans)
    {
        spans_ = spans;
        markDirty();
    }
    const ct::Vector<TimelineSpan>& spans() const
    {
        return spans_;
    }
    /// @brief A press on the tab of a span: its id, the pointer in window pixels, the button (0
    /// left, 1 right).
    Signal<int, float, float, int> onSpanPressed;
    /// @brief A grip of a span is being dragged, on every step: its id, false for its start and
    /// true for its end, and the time under the pointer, snapped like the edge of a clip.
    Signal<int, bool, float> onSpanEdgeDragged;
    /// @brief The drag of a grip ended: the id of the span.
    Signal<int> onSpanEdgeReleased;

    /// @brief How long the last paint() of the timeline took on the CPU (building the geometry of
    /// the ruler, the tracks and the clips; not the GPU's work), in seconds.
    double lastPaintSeconds() const
    {
        return paintSeconds_;
    }

    /// @brief Set the background color.
    void setBgColor(const Color& c)
    {
        bgColor_ = c;
        markDirty();
    }

    /// @brief Emitted when the playhead moves.
    Signal<float> onPlayheadChanged;
    /// @brief Emitted when a keyframe is selected.
    Signal<int, int> onKeyframeSelected;
    /// @brief Emitted when a keyframe is double-clicked: track, key index.
    Signal<int, int> onKeyframeDoubleClicked;
    /// @brief Emitted when the label typed over a keyframe is kept: track, key index, text.
    Signal<int, int, const String&> onKeyframeLabelEdited;
    /// @brief Emitted when a clip is selected.
    Signal<int, int> onClipSelected;
    /// @brief A clip moved or was trimmed. Emitted on every step of the drag.
    Signal<int, int> onClipChanged;
    /// @brief The curve of a clip changed (a point added, moved or removed): track and clip index.
    Signal<int, int> onEnvelopeChanged;
    /// @brief A fade handle of a clip was dragged (see TimelineClip::editableFades), on every step:
    /// track, clip index, false for the fade in and true for the fade out, the length asked for
    /// in seconds (0 to the length of the clip). The drag ends with onClipEdited.
    Signal<int, int, bool, float> onFadeChanged;
    /// @brief The drag that moved or trimmed a clip ended. Track and clip index.
    Signal<int, int> onClipEdited;
    /// @brief In grab mode (setClipGrab), a press on a clip: track, clip index, the pointer's x in
    /// window pixels and the time under it.
    Signal<int, int, float, float> onClipGrabbed;
    /// @brief The pointer moved during a grab: x in window pixels and the time under it.
    Signal<float, float> onClipGrabMoved;
    /// @brief The button of a grab went up: x and the time under it.
    Signal<float, float> onClipGrabReleased;
    /// @brief The track height was changed with Ctrl + mouse wheel.
    Signal<float> onTrackHeightChanged;
    /// @brief A clip was right-clicked (it is selected now): track, clip index and the screen position of the pointer.
    Signal<int, int, float, float> onClipContextMenu;
    /// @brief A track header was clicked (-1: selection cleared).
    Signal<int> onTrackSelected;
    /// @brief A key pressed while the timeline has focus (key, modifiers: 1 shift, 2 ctrl); the
    /// application decides what it does.
    Signal<int, int> onKeyPressed;
    /// @brief Something dragged from another widget was dropped: payload, track under it (-1 if
    /// none) and time.
    Signal<const DragPayload&, int, float> onDrop;

    // ── Overrides ────────────────────────────────────────────────────────
    void layout() override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseScroll(MouseEvent& e) override;
    void onKeyPress(KeyEvent& e) override;
    bool acceptsDrop(const DragPayload& p) override;
    void onDropReceive(const DragPayload& p) override;

private:
    ct::Vector<TimelineTrack> tracks_;
    Color bgColor_ = Color(30, 32, 36, 255);

    double paintSeconds_ = 0;
    float playhead_ = 0;
    float viewStart_ = 0;
    float viewEnd_ = 5;
    float fps_ = 30.0f;
    float rangeIn_ = 0.0f;
    float rangeOut_ = -1.0f;

    float kTrackH = 28;
    float kHeaderW = 160;
    float edgePadding_ = 16;
    float verticalScroll_ = 0;
    bool wheelScrollsTracks_ = false;
    ScrollBar* vbar_ = nullptr;
    float maxVerticalScroll() const;
    void syncScrollBar();
    static constexpr float kRulerH = 24;

    // Interaction state
    enum class DragMode
    {
        None,
        Scrub,
        Pan,
        MoveKey,
        MoveClip,
        ResizeClipL,
        ResizeClipR,
        StretchClipR,
        MoveEnvelopeKey,
        MoveFadeIn,
        MoveFadeOut,
        Marquee,
        Grab,
        SpanEdge
    };
    struct DragMember
    {
        int track = 0;
        int clip = 0;
        float start = 0;
        float end = 0;
    };
    DragMode dragMode_ = DragMode::None;
    float dragStartX_ = 0;
    float panStartView_ = 0;
    float panStartEnd_ = 0;
    int dragTrack_ = -1;
    int dragIndex_ = -1;
    int dragKey_ = -1;
    float dragOrigTime_ = 0;
    float dragOrigEnd_ = 0;
    float dragOrigOffset_ = 0;
    float dragOrigSpeed_ = 1;
    float dragOrigFade_ = 0; // the fade a handle was grabbed at
    bool dragMoved_ = false;
    bool snapping_ = true;
    bool grabClips_ = false;
    int nextClipId_ = 0;
    int selectedTrack_ = -1;
    ct::Vector<DragMember> dragGroup_; // the selected clips a move drags together (more than one)
    bool collapseOnRelease_ =
            false; // a click without a drag on a selected clip keeps only that clip
    float marqueeX0_ = 0, marqueeY0_ = 0, marqueeX1_ = 0, marqueeY1_ = 0;
    ct::Vector<TimelineClipRef>
            marqueeBase_; // what was selected when the rectangle started (Shift or Ctrl)
    ct::Vector<TimelineKeyRef> marqueeKeyBase_;
    struct KeyMember
    {
        int track = 0;
        int key = 0;
        float time = 0;
    };
    ct::Vector<KeyMember> keyGroup_; // the selected keyframes a drag moves together (more than one)
    bool collapseKeyOnRelease_ =
            false; // a click without a drag on a selected key keeps only that key

    ct::Function<float(int, float, float)> peakProvider_;
    ct::Function<String(int, int)> clipTooltip_;
    ct::Vector<float> snapTargets_;
    // A clip (track, clip) or a keyframe (keyTrack, key) being dragged is left out of the
    // candidates.
    void snapCandidates(int track, int clip, int keyTrack, int key, ct::Vector<float>& out) const;
    float snapLimit() const;
    bool snapToEdges(float t, int track, int clip, int keyTrack, int key, float& out) const;
    float snap(float t, int track, int clip, int keyTrack = -1, int key = -1) const;
    bool clipAtPoint(float x, float y, int& track, int& clip) const;
    void updateClipTooltip(float x, float y);
    class LabelInput;
    LabelInput* labelInput_ = nullptr;
    int labelTrack_ = -1;
    int labelKey_ = -1;
    void finishLabelEdit(bool keep, bool refocus);
    // The curve of a clip is drawn inside its bar: where a point sits, and the reverse.
    float envelopeX(const TimelineClip& clip, const CurveKey& key) const;
    float envelopeY(const TimelineClip& clip, float value, float clipY, float clipH) const;
    int envelopeKeyAt(const TimelineClip& clip, float mx, float my, float clipY, float clipH) const;
    // The handle of a fade sits on the top edge of the bar at the end of the fade line, kept off
    // the edge of the bar so that the edge can still be dragged to trim.
    // One curve of a clip as a line across its bar (the envelope, or one of its lines).
    void paintClipCurve(PaintContext& ctx, const TimelineClip& clip, const ct::Vector<CurveKey>& keys,
            float min, float max, const Color& color, float thickness, float x0, float x1, float clipY,
            float clipH) const;
    mutable ct::Vector<Vec2> curvePath_; // the points of the stroke, kept between curves
    float fadeHandleX(const TimelineClip& clip, bool out) const;
    int fadeHandleAt(const TimelineClip& clip, float mx, float my, float clipY) const; // 0 none, 1 in, 2 out

    // ── Helpers ──────────────────────────────────────────────────────────
    float timeToX(float t) const;
    float xToTime(float x) const;
    int trackAtY(float y) const;

    void paintRuler(PaintContext& ctx, const Rect& b);
    void paintTracks(PaintContext& ctx, const Rect& b);
    void paintMarkedRange(PaintContext& ctx, const Rect& b);
    void paintPlayhead(PaintContext& ctx, const Rect& b);
    void paintMarquee(PaintContext& ctx, const Rect& b);
    void paintSpans(PaintContext& ctx, const Rect& b);
    // The box of a span in window pixels; false when its tracks are not shown.
    bool spanRect(const TimelineSpan& span, Rect& out) const;
    // What of a span is under a point: 0 nothing, 1 its tab, 2 the grip of its start, 3 of its end.
    int spanPartAt(float x, float y, int& index) const;
    ct::Vector<TimelineSpan> spans_;
    ct::Vector<Rect> spanTabs_; // where each span's tab was drawn last
    int dragSpan_ = -1;
    bool dragSpanEnd_ = false;
    void clearKeySelection();
    void updateMarquee();
};

} // namespace retained
} // namespace ig
