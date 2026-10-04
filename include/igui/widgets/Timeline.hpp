#pragma once

#include "Widget.hpp"
#include "Signal.hpp"
#include "ChartWidgets.hpp"
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
//  - Horizontal zoom (scroll) and pan (middle-drag)
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
    float speed        = 1;    // how fast the media plays: it takes (length / speed) seconds on the timeline per second of media
    float sourceLength = 0;    // length of the media, 0 = unlimited (limits trimming)
    int   link         = -1;   // clips sharing a link are moved and trimmed together
    int   user         = -1;
    // A curve over the media's own time (seconds), drawn on the clip between envelopeMin and envelopeMax.
    ct::Vector<CurveKey> envelope;
    float envelopeMin  = -40;
    float envelopeMax  = 12;
    float params[4]    = {0, 0, 0, 0}; // free for the application (effects, volume); copied when a clip is split
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
};

class Timeline : public Widget
{
public:
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
    void clearSelection();
    /// @brief First clip on the track covering time t, or -1. Later clips win.
    int  clipAt(int trackId, float t) const;
    /// @brief Snap clip edges to other clips, the playhead and zero while dragging.
    void setSnapping(bool on) { snapping_ = on; }
    bool snapping() const { return snapping_; }

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

    /// @brief Set the frame rate (fps) for tick marks.
    void  setFrameRate(float fps) { fps_ = fps; }
    /// @brief Get the frame rate.
    float frameRate() const       { return fps_; }

    /// @brief Set the background color.
    void setBgColor(const Color& c) { bgColor_ = c; markDirty(); }

    /// @brief Emitted when the playhead moves.
    Signal<float>    onPlayheadChanged;
    /// @brief Emitted when a keyframe is selected.
    Signal<int, int> onKeyframeSelected;
    /// @brief Emitted when a clip is selected.
    Signal<int, int> onClipSelected;
    /// @brief A clip moved or was trimmed. Emitted on every step of the drag.
    Signal<int, int> onClipChanged;
    /// @brief The drag that moved or trimmed a clip ended. Track and clip index.
    Signal<int, int> onClipEdited;
    /// @brief A track header was clicked (-1: selection cleared).
    Signal<int> onTrackSelected;
    /// @brief A key pressed while the timeline has focus (key, modifiers: 1 shift, 2 ctrl); the application decides what it does.
    Signal<int, int> onKeyPressed;
    /// @brief Something dragged from another widget was dropped: payload, track under it (-1 if none) and time.
    Signal<const DragPayload&, int, float> onDrop;

    // ── Overrides ────────────────────────────────────────────────────────
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

    static constexpr float kTrackH  = 28;
    float kHeaderW = 160;
    float edgePadding_ = 16;
    float verticalScroll_ = 0;
    static constexpr float kRulerH  = 24;

    // Interaction state
    enum class DragMode { None, Scrub, Pan, MoveKey, MoveClip, ResizeClipL, ResizeClipR, StretchClipR };
    DragMode dragMode_      = DragMode::None;
    float    dragStartX_    = 0;
    float    panStartView_  = 0;
    float    panStartEnd_   = 0;
    int      dragTrack_     = -1;
    int      dragIndex_     = -1;
    float    dragOrigTime_  = 0;
    float    dragOrigEnd_   = 0;
    float    dragOrigOffset_ = 0;
    float    dragOrigSpeed_ = 1;
    bool     dragMoved_     = false;
    bool     snapping_      = true;
    int      nextClipId_    = 0;
    int      selectedTrack_ = -1;

    float snap(float t, int track, int clip) const;

    // ── Helpers ──────────────────────────────────────────────────────────
    float timeToX(float t) const;
    float xToTime(float x) const;
    int   trackAtY(float y) const;

    void paintRuler(PaintContext& ctx, const Rect& b);
    void paintTracks(PaintContext& ctx, const Rect& b);
    void paintPlayhead(PaintContext& ctx, const Rect& b);
};

} // namespace retained
} // namespace ig
