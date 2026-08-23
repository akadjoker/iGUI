#pragma once

#include "Widget.hpp"
#include "Signal.hpp"
#include <vector>
#include <string>

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

namespace BuGUI
{
struct TimelineKeyframe {
    float time     = 0;
    bool  selected = false;
};

struct TimelineClip {
    float       start = 0;
    float       end   = 1;
    std::string label;
    Color       color    = Color(80, 120, 180, 200);
    bool        selected = false;
};

struct TimelineTrack {
    std::string                    name;
    Color                          color = Color(120, 140, 180, 255);
    std::vector<TimelineKeyframe>  keyframes;
    std::vector<TimelineClip>      clips;
    bool muted  = false;
    bool locked = false;
};

class Timeline : public Widget
{
public:
    Timeline();

    /// @brief Add a named track with optional color.
    int  addTrack(const std::string& name,
                  const Color& color = Color(120, 140, 180, 255));
    /// @brief Remove a track by ID.
    void removeTrack(int trackId);
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
                 const std::string& label = "",
                 const Color& color = Color(80, 120, 180, 200));
    /// @brief Remove a clip by index.
    void removeClip(int trackId, int clipIdx);

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

    // ── Overrides ────────────────────────────────────────────────────────
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseScroll(MouseEvent& e) override;

private:
    std::vector<TimelineTrack> tracks_;
    Color bgColor_ = Color(30, 32, 36, 255);

    float playhead_   = 0;
    float viewStart_  = 0;
    float viewEnd_    = 5;
    float fps_        = 30.0f;

    static constexpr float kTrackH  = 28;
    static constexpr float kHeaderW = 100;
    static constexpr float kRulerH  = 24;

    // Interaction state
    enum class DragMode { None, Scrub, Pan, MoveKey, MoveClip, ResizeClipL, ResizeClipR };
    DragMode dragMode_      = DragMode::None;
    float    dragStartX_    = 0;
    float    panStartView_  = 0;
    float    panStartEnd_   = 0;
    int      dragTrack_     = -1;
    int      dragIndex_     = -1;
    float    dragOrigTime_  = 0;
    float    dragOrigEnd_   = 0;

    // ── Helpers ──────────────────────────────────────────────────────────
    float timeToX(float t) const;
    float xToTime(float x) const;
    int   trackAtY(float y) const;

    void paintRuler(PaintContext& ctx, const Rect& b);
    void paintTracks(PaintContext& ctx, const Rect& b);
    void paintPlayhead(PaintContext& ctx, const Rect& b);
};

} // namespace BuGUI
