#pragma once

#include "Widget.hpp"
#include "Signal.hpp"
#include "ChartWidgets.hpp"
#include "ScrollWidgets.hpp"
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
//  - Several clips selected at once: Ctrl+click toggles one, Shift+click adds one, a drag on the empty
//    part of a track selects the clips inside the rectangle, and dragging a selected clip moves all of them
//  - Horizontal zoom (scroll) and pan (middle-drag)
//  - Snapping while dragging: clip edges, keyframes, the playhead and zero within 8 pixels, whole frames otherwise
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

namespace ig { namespace retained
{
struct TimelineKeyframe {
    float time     = 0;
    bool  selected = false;
    int   user     = -1;   // the application's own id for the key; the widget only carries it
    String label;          // drawn beside the diamond (a marker's name)
};

struct TimelineClip {
    float       start = 0;
    float       end   = 1;
    String label;
    Color       color    = Color(80, 120, 180, 200);
    bool        selected = false;

    // The timeline only carries these; the application gives them meaning.
    int   id           = -1;   // unique in the timeline, set by addClip
    int   media        = -1;   // what the clip plays
    float offset       = 0;    // where in the media the clip starts (seconds)
    int   transition   = 0;    // how it comes in over the clip before it, when they overlap on a track (the application's list)
    float speed        = 1;    // how fast the media plays: it takes (length / speed) seconds on the timeline per second of media
    float sourceLength = 0;    // length of the media, 0 = unlimited (limits trimming)
    int   link         = -1;   // clips sharing a link are moved and trimmed together
    int   user         = -1;
    int   generator    = -1;   // an overlay made by the application (a title, a balloon): no media, drawn on top
    // A curve over the media's own time (seconds), drawn on the clip between envelopeMin and envelopeMax.
    ct::Vector<CurveKey> envelope;
    bool  editableEnvelope = false; // double-click on the clip adds a point to the curve, drag moves it
    float envelopeRest = 0;         // the value a new curve starts flat at
    float envelopeMin  = -40;
    float envelopeMax  = 12;
    float params[16]   = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // free for the application (effects, volume); copied when a clip is split
    // A curve per entry of params over the media's own time (seconds); when there is one it replaces the constant.
    ct::Vector<CurveKey> automation[12];
};

/// @brief The candidate nearest to t when it lies within threshold of it (false when none does).
bool findSnap(const ct::Vector<float>& candidates, float t, float threshold, float& out);
/// @brief t moved to the nearest candidate within threshold, or t itself when none is that close.
float snapTime(const ct::Vector<float>& candidates, float t, float threshold);
/// @brief t rounded to a whole frame at fps (t itself when fps is not positive).
float snapFrame(float t, float fps);

/// A clip by its track and its index in the track.
struct TimelineClipRef {
    int track = 0;
    int clip  = 0;
};

/// What a track carries; the timeline draws the same, the application decides.
enum class TrackKind { Generic, Video, Audio };

struct TimelineTrack {
    String                    name;
    Color                          color = Color(120, 140, 180, 255);
    ct::Vector<TimelineKeyframe>  keyframes;
    ct::Vector<TimelineClip>      clips;
    bool muted  = false;
    bool locked = false;
    int parent = -1;
    bool expanded = true;
    TrackKind kind = TrackKind::Generic;
    bool rulerMarks = false; // the keyframes are markers: a flag with the label is also drawn on the ruler
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
    /// Space in pixels inside both timeline edges (minimum 8 for full diamonds).
    void setEdgePadding(float pixels);
    float edgePadding() const { return edgePadding_; }
    /// Header buttons: M toggles mute, L toggles lock. Emitted with the track id and new state.
    Signal<int, bool> onTrackMuteChanged;
    Signal<int, bool> onTrackLockChanged;
    Signal<int, int, float> onKeyframeMoved;
    /// Emitted once when a dragged keyframe is released: track id, key index, start time, end time.
    Signal<int, int, float, float> onKeyframeDropped;
    Signal<int, int> onKeyframeAdded;

    /// @brief Add a named track with optional color.
    int  addTrack(const String& name,
                  const Color& color = Color(120, 140, 180, 255));
    /// @brief Add a track of a kind (video or audio) with a default colour for it.
    int  addTrack(const String& name, TrackKind kind);
    /// @brief Mute or unmute a track (emits onTrackMuteChanged).
    void setTrackMuted(int trackId, bool muted);
    /// @brief Insert a track of a kind at a row (0 = top). Later track ids move down by one.
    int  insertTrack(int index, const String& name, TrackKind kind);
    /// @brief Remove a track by ID.
    void removeTrack(int trackId);
    /// @brief The track whose header was clicked, -1 if none.
    int  selectedTrack() const { return selectedTrack_; }
    void setSelectedTrack(int trackId);
    /// @brief Remove all tracks.
    void clearTracks();

    /// @brief Get the number of tracks.
    int                trackCount() const { return static_cast<int>(tracks_.size()); }
    /// @brief Every track, to keep a copy of the whole timeline (undo) and put it back.
    const ct::Vector<TimelineTrack>& allTracks() const { return tracks_; }
    void setAllTracks(const ct::Vector<TimelineTrack>& tracks);
    /// @brief Get a mutable track reference.
    TimelineTrack&       track(int id)       { return tracks_[id]; }
    /// @brief Get a const track reference.
    const TimelineTrack& track(int id) const { return tracks_[id]; }

    /// @brief Add a keyframe at a time position.
    int  addKeyframe(int trackId, float time);
    /// @brief Remove a keyframe by index.
    void removeKeyframe(int trackId, int keyIdx);

    /// @brief Add a clip spanning a time range.
    int  addClip(int trackId, float start, float end,
                 const String& label = "",
                 const Color& color = Color(80, 120, 180, 200));
    /// @brief Remove a clip by index.
    void removeClip(int trackId, int clipIdx);
    /// @brief Cut a clip in two at time t. Returns the index of the second part (-1 if t is not inside).
    int  splitClip(int trackId, int clipIdx, float t);
    /// @brief The first selected clip. False when none is selected.
    bool selectedClip(int& trackId, int& clipIdx) const;
    /// @brief Every selected clip, track by track, in the order of the clips.
    void selectedClips(ct::Vector<TimelineClipRef>& out) const;
    int  selectedClipCount() const;
    void clearSelection();
    /// @brief First clip on the track covering time t, or -1. Later clips win.
    int  clipAt(int trackId, float t) const;
    /// @brief The mouse wheel scrolls the tracks up and down everywhere and Ctrl+wheel or Shift+wheel zooms the time
    ///        under the pointer (true), or the wheel zooms the time and only scrolls over the track names (false,
    ///        the default).
    void setWheelScrollsTracks(bool on) { wheelScrollsTracks_ = on; }
    bool wheelScrollsTracks() const { return wheelScrollsTracks_; }
    /// @brief How far the tracks are scrolled down, in pixels.
    float verticalScroll() const { return verticalScroll_; }
    void  setVerticalScroll(float pixels);
    /// @brief Snap clips and keyframes while dragging: to the edges of the clips on every track, to keyframes, to
    ///        the playhead, to zero and to the application's own targets when one is within 8 pixels, and to whole
    ///        frames otherwise.
    void setSnapping(bool on) { snapping_ = on; }
    bool snapping() const { return snapping_; }
    /// @brief Extra times to snap to (the end of the sequence); the application refreshes them when they change.
    void setSnapTargets(const ct::Vector<float>& targets) { snapTargets_ = targets; }
    const ct::Vector<float>& snapTargets() const { return snapTargets_; }
    /// @brief The text shown after a moment of hovering over a clip (track, clip index); empty text shows nothing.
    void setClipTooltip(ct::Function<String(int, int)> provider) { clipTooltip_ = std::move(provider); }
    /// @brief Opens a small text field over a keyframe to type its label. Enter or leaving the field keeps the text
    ///        (onKeyframeLabelEdited), Escape drops it.
    void editKeyframeLabel(int trackId, int keyIdx);
    bool editingKeyframeLabel() const { return labelTrack_ >= 0; }

    /// @brief Window-space point of a time on a track's row (the centre of the row), or on the ruler when
    ///        trackId is -1; false when the track is not shown (collapsed, scrolled out) or the time is outside the
    ///        visible range.
    bool pointAt(int trackId, float time, float& x, float& y) const;

    /// @brief Set the playhead position in seconds.
    void  setPlayhead(float t) { playhead_ = t; markDirty(); }
    /// @brief Get the playhead position in seconds.
    float playhead() const     { return playhead_; }

    /// @brief Set the visible time range.
    void  setTimeRange(float start, float end);
    /// @brief Get the visible start time.
    float viewStart() const { return viewStart_; }
    /// @brief Get the visible end time.
    float viewEnd()   const { return viewEnd_; }

    /// @brief Shade the range between two times (an in/out selection); out <= in hides it.
    void  setMarkedRange(float in, float out) { rangeIn_ = in; rangeOut_ = out; markDirty(); }
    void  clearMarkedRange()                  { rangeIn_ = 0.0f; rangeOut_ = -1.0f; markDirty(); }

    /// @brief Set the frame rate (fps) for tick marks.
    void  setFrameRate(float fps) { fps_ = fps; }
    /// @brief Get the frame rate.
    float frameRate() const       { return fps_; }

    /// @brief Where the loudness of a clip's sound comes from, to draw its waveform: media, the part of the media
    /// (seconds), and the answer, 0 to 1 (or negative when it is not known yet).
    void setPeakProvider(ct::Function<float(int, float, float)> provider) { peakProvider_ = std::move(provider); markDirty(); }

    /// @brief Set the background color.
    void setBgColor(const Color& c) { bgColor_ = c; markDirty(); }

    /// @brief Emitted when the playhead moves.
    Signal<float>    onPlayheadChanged;
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
    /// @brief The drag that moved or trimmed a clip ended. Track and clip index.
    Signal<int, int> onClipEdited;
    /// @brief A track header was clicked (-1: selection cleared).
    Signal<int> onTrackSelected;
    /// @brief A key pressed while the timeline has focus (key, modifiers: 1 shift, 2 ctrl); the application decides what it does.
    Signal<int, int> onKeyPressed;
    /// @brief Something dragged from another widget was dropped: payload, track under it (-1 if none) and time.
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

    float playhead_   = 0;
    float viewStart_  = 0;
    float viewEnd_    = 5;
    float fps_        = 30.0f;
    float rangeIn_    = 0.0f;
    float rangeOut_   = -1.0f;

    static constexpr float kTrackH  = 28;
    float kHeaderW = 160;
    float edgePadding_ = 16;
    float verticalScroll_ = 0;
    bool  wheelScrollsTracks_ = false;
    ScrollBar* vbar_ = nullptr;
    float maxVerticalScroll() const;
    void  syncScrollBar();
    static constexpr float kRulerH  = 24;

    // Interaction state
    enum class DragMode { None, Scrub, Pan, MoveKey, MoveClip, ResizeClipL, ResizeClipR, StretchClipR, MoveEnvelopeKey, Marquee };
    struct DragMember {
        int   track = 0;
        int   clip  = 0;
        float start = 0;
        float end   = 0;
    };
    DragMode dragMode_      = DragMode::None;
    float    dragStartX_    = 0;
    float    panStartView_  = 0;
    float    panStartEnd_   = 0;
    int      dragTrack_     = -1;
    int      dragIndex_     = -1;
    int      dragKey_       = -1;
    float    dragOrigTime_  = 0;
    float    dragOrigEnd_   = 0;
    float    dragOrigOffset_ = 0;
    float    dragOrigSpeed_ = 1;
    bool     dragMoved_     = false;
    bool     snapping_      = true;
    int      nextClipId_    = 0;
    int      selectedTrack_ = -1;
    ct::Vector<DragMember>       dragGroup_;    // the selected clips a move drags together (more than one)
    bool                         collapseOnRelease_ = false; // a click without a drag on a selected clip keeps only that clip
    float                        marqueeX0_ = 0, marqueeY0_ = 0, marqueeX1_ = 0, marqueeY1_ = 0;
    ct::Vector<TimelineClipRef>  marqueeBase_;  // what was selected when the rectangle started (Shift or Ctrl)

    ct::Function<float(int, float, float)> peakProvider_;
    ct::Function<String(int, int)> clipTooltip_;
    ct::Vector<float> snapTargets_;
    // A clip (track, clip) or a keyframe (keyTrack, key) being dragged is left out of the candidates.
    void  snapCandidates(int track, int clip, int keyTrack, int key, ct::Vector<float>& out) const;
    float snapLimit() const;
    bool  snapToEdges(float t, int track, int clip, int keyTrack, int key, float& out) const;
    float snap(float t, int track, int clip, int keyTrack = -1, int key = -1) const;
    bool  clipAtPoint(float x, float y, int& track, int& clip) const;
    void  updateClipTooltip(float x, float y);
    class LabelInput;
    LabelInput* labelInput_ = nullptr;
    int  labelTrack_ = -1;
    int  labelKey_   = -1;
    void finishLabelEdit(bool keep, bool refocus);
    // The curve of a clip is drawn inside its bar: where a point sits, and the reverse.
    float envelopeX(const TimelineClip& clip, const CurveKey& key) const;
    float envelopeY(const TimelineClip& clip, float value, float clipY, float clipH) const;
    int   envelopeKeyAt(const TimelineClip& clip, float mx, float my, float clipY, float clipH) const;

    // ── Helpers ──────────────────────────────────────────────────────────
    float timeToX(float t) const;
    float xToTime(float x) const;
    int   trackAtY(float y) const;

    void paintRuler(PaintContext& ctx, const Rect& b);
    void paintTracks(PaintContext& ctx, const Rect& b);
    void paintMarkedRange(PaintContext& ctx, const Rect& b);
    void paintPlayhead(PaintContext& ctx, const Rect& b);
    void paintMarquee(PaintContext& ctx, const Rect& b);
    void clearKeySelection();
    void updateMarquee();
};

} // namespace retained
} // namespace ig
