#include "Timeline.hpp"
#include "Theme.hpp"
#include "Retained.hpp"
#include "TextInputWidgets.hpp"
#include "WidgetApp.hpp"
#include <cmath>
#include <algorithm>
#include <cstdio>

// ─────────────────────────────────────────────────────────────────────────────
//  Font helper
// ─────────────────────────────────────────────────────────────────────────────
namespace {
    inline float setupFont(PaintContext& ctx, const Color& color, float size)
    {
        ctx.font.SetFontSize(size);
        ctx.font.SetBatch(&ctx.text);
        ctx.font.SetColor(color);
        return ctx.font.GetAscender();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
//  Snapping
// ═════════════════════════════════════════════════════════════════════════════

namespace ig { namespace retained
{

bool findSnap(const ct::Vector<float>& candidates, float t, float threshold, float& out)
{
    bool found = false;
    float bestDistance = 0.0f;
    for (size_t i = 0; i < candidates.size(); ++i) {
        const float distance = std::fabs(candidates[i] - t);
        if (distance > threshold) continue;
        if (found && distance >= bestDistance) continue;
        found = true;
        bestDistance = distance;
        out = candidates[i];
    }
    return found;
}

float snapTime(const ct::Vector<float>& candidates, float t, float threshold)
{
    float out = t;
    return findSnap(candidates, t, threshold, out) ? out : t;
}

float snapFrame(float t, float fps)
{
    if (!(fps > 0.0f) || !std::isfinite(t)) return t;
    return std::round(t * fps) / fps;
}

} // namespace retained
} // namespace ig

// ═════════════════════════════════════════════════════════════════════════════
//  Timeline
// ═════════════════════════════════════════════════════════════════════════════

// The small text field that names a keyframe in place. Every key it gets is its own, so typing a name never
// reaches the shortcuts of the timeline behind it.
class Timeline::LabelInput : public TextInput
{
public:
    explicit LabelInput(Timeline* owner) : owner_(owner) { setTabStop(false); }

    void onKeyPress(KeyEvent& e) override
    {
        if (e.key == Key::Escape) {
            owner_->finishLabelEdit(false, true);
        } else if (e.key == Key::Return || e.key == Key::KPEnter) {
            owner_->finishLabelEdit(true, true);
        } else {
            TextInput::onKeyPress(e);
        }
        e.consumed = true;
    }

    void onKeyRelease(KeyEvent& e) override { e.consumed = true; }

    void onFocusLost() override
    {
        TextInput::onFocusLost();
        owner_->finishLabelEdit(true, false);
    }

private:
    Timeline* owner_;
};

Timeline::Timeline()
{
    acceptsFocus_ = true;
    vbar_ = createChild<ScrollBar>(ScrollBarOrientation::Vertical);
    vbar_->setVisible(false);
    vbar_->scrolled.connect([this](float value) { verticalScroll_ = value; markDirty(); });
}

float Timeline::maxVerticalScroll() const
{
    return std::max(0.0f, visibleTracks().size() * kTrackH - (rect_.h - kRulerH));
}

void Timeline::setVerticalScroll(float pixels)
{
    if (!std::isfinite(pixels)) return;
    verticalScroll_ = std::clamp(pixels, 0.0f, maxVerticalScroll());
    syncScrollBar();
    markDirty();
}

void Timeline::syncScrollBar()
{
    const float view = std::max(0.0f, rect_.h - kRulerH);
    const float content = visibleTracks().size() * kTrackH;
    verticalScroll_ = std::clamp(verticalScroll_, 0.0f, std::max(0.0f, content - view));
    vbar_->setContentSize(content);
    vbar_->setViewSize(view);
    vbar_->setValue(verticalScroll_);
    vbar_->setVisible(ScrollBar::neededFor(content, view));
}

void Timeline::layout()
{
    const float thick = ScrollBar::kBarThickness;
    vbar_->setRect({rect_.w - thick - 2.0f, kRulerH + 1.0f, thick, std::max(0.0f, rect_.h - kRulerH - 2.0f)});
    syncScrollBar();
    Widget::layout();
}

int Timeline::addJoint(const String& name, int parent)
{
    if (parent < -1 || parent >= trackCount()) return -1;
    const int id = addTrack(name);
    tracks_[id].parent = parent;
    return id;
}

void Timeline::setExpanded(int joint, bool expanded)
{
    if (joint < 0 || joint >= trackCount()) return;
    tracks_[joint].expanded = expanded;
    dragMode_ = DragMode::None;
    verticalScroll_ = 0;
    markDirty();
}

namespace
{
constexpr float kBtn = 14.0f;
constexpr float kBtnGap = 4.0f;

// Header buttons sit at the right edge: [M][L].
Rect lockButton(float headerX, float headerW, float trackY, float trackH)
{
    return { headerX + headerW - kBtn - 6.0f, trackY + (trackH - kBtn) * 0.5f, kBtn, kBtn };
}

Rect muteButton(float headerX, float headerW, float trackY, float trackH)
{
    Rect r = lockButton(headerX, headerW, trackY, trackH);
    r.x -= kBtn + kBtnGap;
    return r;
}

bool hitsButton(const Rect& r, float x, float y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}
}

int Timeline::addTrack(const String& name, TrackKind kind)
{
    const int id = addTrack(name, kind == TrackKind::Audio ? Color(220, 145, 75, 255) : Color(90, 150, 220, 255));
    tracks_[id].kind = kind;
    return id;
}

void Timeline::setTrackMuted(int trackId, bool muted)
{
    if (trackId < 0 || trackId >= static_cast<int>(tracks_.size()) || tracks_[trackId].muted == muted)
        return;
    tracks_[trackId].muted = muted;
    onTrackMuteChanged.emit(trackId, muted);
    markDirty();
}

void Timeline::setHeaderWidth(float width) { if (std::isfinite(width)) kHeaderW = std::max(40.0f, width); markDirty(); }
void Timeline::setEdgePadding(float pixels) { if (std::isfinite(pixels)) edgePadding_ = std::max(8.0f, pixels); markDirty(); }

ct::Vector<int> Timeline::visibleTracks() const
{
    ct::Vector<int> rows;
    auto visit = [&](auto&& self, int parent) -> void {
        for (int i = 0; i < trackCount(); ++i) if (tracks_[i].parent == parent) {
            rows.push_back(i);
            if (tracks_[i].expanded) self(self, i);
        }
    };
    visit(visit, -1);
    return rows;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Track / Keyframe / Clip management
// ─────────────────────────────────────────────────────────────────────────────

int Timeline::addTrack(const String& name, const Color& color)
{
    tracks_.push_back({name, color, {}, {}, false, false});
    markDirty();
    return static_cast<int>(tracks_.size()) - 1;
}

int Timeline::insertTrack(int index, const String& name, TrackKind kind)
{
    index = std::max(0, std::min(index, trackCount()));
    addTrack(name, kind);
    TimelineTrack added = tracks_[tracks_.size() - 1];
    tracks_.erase(tracks_.begin() + (tracks_.size() - 1));
    tracks_.insert(tracks_.begin() + index, added);
    for (int i = 0; i < trackCount(); ++i) {
        if (i != index && tracks_[i].parent >= index) ++tracks_[i].parent;
    }
    if (selectedTrack_ >= index) ++selectedTrack_;
    dragMode_ = DragMode::None;
    markDirty();
    return index;
}

void Timeline::setSelectedTrack(int trackId)
{
    if (trackId < -1 || trackId >= trackCount()) trackId = -1;
    if (trackId == selectedTrack_) return;
    selectedTrack_ = trackId;
    onTrackSelected.emit(trackId);
    markDirty();
}

void Timeline::removeTrack(int trackId)
{
    if (trackId >= 0 && trackId < static_cast<int>(tracks_.size())) {
        tracks_.erase(tracks_.begin() + trackId);
        for (auto& tr : tracks_) {
            if (tr.parent == trackId) tr.parent = -1;
            else if (tr.parent > trackId) --tr.parent;
        }
        if (selectedTrack_ == trackId) selectedTrack_ = -1;
        else if (selectedTrack_ > trackId) --selectedTrack_;
        dragMode_ = DragMode::None;
        verticalScroll_ = 0;
        markDirty();
    }
}

void Timeline::setAllTracks(const ct::Vector<TimelineTrack>& tracks)
{
    tracks_ = tracks;
    selectedTrack_ = -1;
    dragMode_ = DragMode::None;
    verticalScroll_ = 0;
    markDirty();
}

void Timeline::clearTracks()
{
    tracks_.clear();
    selectedTrack_ = -1;
    dragMode_ = DragMode::None;
    verticalScroll_ = 0;
    markDirty();
}

int Timeline::addKeyframe(int trackId, float time)
{
    if (trackId < 0 || trackId >= static_cast<int>(tracks_.size())) return -1;
    dragMode_ = DragMode::None;
    auto& kfs = tracks_[trackId].keyframes;
    TimelineKeyframe kf;
    kf.time = time;
    // Dragging preserves key indices, so the array need not remain sorted.
    auto it = kfs.begin();
    while (it != kfs.end() && it->time < time) ++it;
    int idx = static_cast<int>(it - kfs.begin());
    kfs.insert(it, kf);
    markDirty();
    return idx;
}

void Timeline::removeKeyframe(int trackId, int keyIdx)
{
    dragMode_ = DragMode::None;
    if (trackId < 0 || trackId >= static_cast<int>(tracks_.size())) return;
    auto& kfs = tracks_[trackId].keyframes;
    if (keyIdx >= 0 && keyIdx < static_cast<int>(kfs.size())) {
        kfs.erase(kfs.begin() + keyIdx);
        markDirty();
    }
}

int Timeline::addClip(int trackId, float start, float end,
                      const String& label, const Color& color)
{
    if (trackId < 0 || trackId >= static_cast<int>(tracks_.size())) return -1;
    TimelineClip clip;
    clip.start = start;
    clip.end   = end;
    clip.label = label;
    clip.color = color;
    clip.id    = nextClipId_++;
    tracks_[trackId].clips.push_back(std::move(clip));
    markDirty();
    return static_cast<int>(tracks_[trackId].clips.size()) - 1;
}

void Timeline::removeClip(int trackId, int clipIdx)
{
    if (trackId < 0 || trackId >= static_cast<int>(tracks_.size())) return;
    auto& clips = tracks_[trackId].clips;
    if (clipIdx >= 0 && clipIdx < static_cast<int>(clips.size())) {
        clips.erase(clips.begin() + clipIdx);
        markDirty();
    }
}

int Timeline::splitClip(int trackId, int clipIdx, float t)
{
    if (trackId < 0 || trackId >= static_cast<int>(tracks_.size())) return -1;
    auto& clips = tracks_[trackId].clips;
    if (clipIdx < 0 || clipIdx >= static_cast<int>(clips.size())) return -1;
    TimelineClip second = clips[clipIdx];
    if (t <= second.start + 0.02f || t >= second.end - 0.02f) return -1;
    second.id       = nextClipId_++;
    second.offset   = second.offset + (t - second.start) * second.speed;
    second.start    = t;
    second.selected = false;
    second.link     = -1;
    second.transition = 0; // the two halves meet end to end: nothing to blend
    second.user     = -1;
    clips[clipIdx].end = t;
    clips.push_back(second);
    markDirty();
    return static_cast<int>(clips.size()) - 1;
}

bool Timeline::selectedClip(int& trackId, int& clipIdx) const
{
    for (int ti = 0; ti < trackCount(); ++ti)
        for (int ci = 0; ci < static_cast<int>(tracks_[ti].clips.size()); ++ci)
            if (tracks_[ti].clips[ci].selected) { trackId = ti; clipIdx = ci; return true; }
    return false;
}

void Timeline::selectedClips(ct::Vector<TimelineClipRef>& out) const
{
    out.clear();
    for (int ti = 0; ti < trackCount(); ++ti)
        for (int ci = 0; ci < static_cast<int>(tracks_[ti].clips.size()); ++ci)
            if (tracks_[ti].clips[ci].selected) {
                TimelineClipRef ref;
                ref.track = ti;
                ref.clip = ci;
                out.push_back(ref);
            }
}

int Timeline::selectedClipCount() const
{
    int count = 0;
    for (const auto& trk : tracks_)
        for (const auto& clip : trk.clips) if (clip.selected) ++count;
    return count;
}

void Timeline::clearSelection()
{
    for (auto& trk : tracks_) {
        for (auto& clip : trk.clips) clip.selected = false;
        for (auto& kf : trk.keyframes) kf.selected = false;
    }
    markDirty();
}

void Timeline::clearKeySelection()
{
    for (auto& trk : tracks_)
        for (auto& kf : trk.keyframes) kf.selected = false;
    markDirty();
}

int Timeline::clipAt(int trackId, float t) const
{
    if (trackId < 0 || trackId >= trackCount()) return -1;
    const auto& clips = tracks_[trackId].clips;
    for (int ci = static_cast<int>(clips.size()) - 1; ci >= 0; --ci)
        if (t >= clips[ci].start && t < clips[ci].end) return ci;
    return -1;
}

void Timeline::setTimeRange(float start, float end)
{
    if (!std::isfinite(start) || !std::isfinite(end) || end - start < 0.0001f) return;
    viewStart_ = start;
    viewEnd_   = end;
    markDirty();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Coordinate helpers
// ─────────────────────────────────────────────────────────────────────────────

float Timeline::timeToX(float t) const
{
    Rect b = absoluteRect();
    float contentW = std::max(1.0f, b.w - kHeaderW - edgePadding_ * 2);
    return b.x + kHeaderW + edgePadding_ + (t - viewStart_) / (viewEnd_ - viewStart_) * contentW;
}

float Timeline::xToTime(float x) const
{
    Rect b = absoluteRect();
    float contentW = std::max(1.0f, b.w - kHeaderW - edgePadding_ * 2);
    return viewStart_ + (x - b.x - kHeaderW - edgePadding_) / contentW * (viewEnd_ - viewStart_);
}

int Timeline::trackAtY(float y) const
{
    Rect b = absoluteRect();
    float trackArea = y - b.y - kRulerH;
    if (trackArea < 0) return -1;
    const auto rows = visibleTracks();
    int idx = static_cast<int>((trackArea + verticalScroll_) / kTrackH);
    if (idx >= static_cast<int>(rows.size()) || y >= b.bottom()) return -1;
    return rows[idx];
}

float Timeline::envelopeX(const TimelineClip& clip, const CurveKey& key) const
{
    return timeToX(clip.start + (key.time - clip.offset) / clip.speed);
}

float Timeline::envelopeY(const TimelineClip& clip, float value, float clipY, float clipH) const
{
    const float f = std::max(0.0f, std::min(1.0f, (value - clip.envelopeMin) / (clip.envelopeMax - clip.envelopeMin)));
    return clipY + clipH - 2.0f - f * (clipH - 4.0f);
}

int Timeline::envelopeKeyAt(const TimelineClip& clip, float mx, float my, float clipY, float clipH) const
{
    for (int k = 0; k < static_cast<int>(clip.envelope.size()); ++k) {
        const float kx = envelopeX(clip, clip.envelope[k]);
        const float ky = envelopeY(clip, clip.envelope[k].value, clipY, clipH);
        if (std::fabs(mx - kx) + std::fabs(my - ky) < 7.0f) return k;
    }
    return -1;
}

float Timeline::snapLimit() const
{
    Rect b = absoluteRect();
    const float contentW = std::max(1.0f, b.w - kHeaderW - edgePadding_ * 2);
    return 8.0f * (viewEnd_ - viewStart_) / contentW;
}

// Every time worth snapping to: zero, the playhead, the application's targets, the keyframes of every track and the
// edges of the clips. The clip being dragged and its linked partners are left out, and so is the keyframe being dragged.
void Timeline::snapCandidates(int track, int clip, int keyTrack, int key, ct::Vector<float>& out) const
{
    out.clear();
    const int link = track >= 0 && clip >= 0 ? tracks_[track].clips[clip].link : -1;
    out.push_back(0.0f);
    out.push_back(playhead_);
    for (size_t i = 0; i < snapTargets_.size(); ++i) out.push_back(snapTargets_[i]);
    for (int ti = 0; ti < trackCount(); ++ti) {
        const auto& keys = tracks_[ti].keyframes;
        for (int ki = 0; ki < static_cast<int>(keys.size()); ++ki) {
            if (ti == keyTrack && ki == key) continue;
            out.push_back(keys[ki].time);
        }
        const auto& clips = tracks_[ti].clips;
        for (int ci = 0; ci < static_cast<int>(clips.size()); ++ci) {
            if (ti == track && ci == clip) continue;
            if (link >= 0 && clips[ci].link == link) continue;
            if (dragGroup_.size() > 1 && clips[ci].selected) continue; // they move with the clip
            out.push_back(clips[ci].start);
            out.push_back(clips[ci].end);
        }
    }
}

bool Timeline::snapToEdges(float t, int track, int clip, int keyTrack, int key, float& out) const
{
    if (!snapping_) return false;
    ct::Vector<float> candidates;
    snapCandidates(track, clip, keyTrack, key, candidates);
    return findSnap(candidates, t, snapLimit(), out);
}

// Pulls t to the nearest edge, keyframe or the playhead when it is within a few pixels, and to a whole frame when
// nothing is that close.
float Timeline::snap(float t, int track, int clip, int keyTrack, int key) const
{
    if (!snapping_) return t;
    float edge = t;
    if (snapToEdges(t, track, clip, keyTrack, key, edge)) return edge;
    return snapFrame(t, fps_);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Mouse interaction
// ─────────────────────────────────────────────────────────────────────────────

void Timeline::onMousePress(MouseEvent& e)
{
    Rect b = absoluteRect();
    if (!tooltip().empty()) setTooltip("");

    // Middle button → pan
    if (e.button == 2) {
        dragMode_      = DragMode::Pan;
        dragStartX_    = e.x;
        panStartView_  = viewStart_;
        panStartEnd_   = viewEnd_;
        e.consumed     = true;
        return;
    }
    if (e.button != 0) return;

    // Left-click on ruler → scrub playhead
    if (e.y < b.y + kRulerH && e.x > b.x + kHeaderW) {
        dragMode_ = DragMode::Scrub;
        playhead_ = xToTime(e.x);
        onPlayheadChanged.emit(playhead_);
        markDirty();
        e.consumed = true;
        return;
    }

    int ti = trackAtY(e.y);
    if (ti < 0) return;

    auto& trk = tracks_[ti];
    if (e.x < b.x + kHeaderW) {
        const auto visible = visibleTracks();
        int hrow = 0;
        while (hrow < static_cast<int>(visible.size()) && visible[hrow] != ti) ++hrow;
        const float hy = b.y + kRulerH + hrow * kTrackH - verticalScroll_;
        if (hitsButton(muteButton(b.x, kHeaderW, hy, kTrackH), e.x, e.y)) {
            setTrackMuted(ti, !trk.muted);
        } else if (hitsButton(lockButton(b.x, kHeaderW, hy, kTrackH), e.x, e.y)) {
            trk.locked = !trk.locked;
            onTrackLockChanged.emit(ti, trk.locked);
            markDirty();
        } else {
            bool children = false;
            for (const auto& child : tracks_) if (child.parent == ti) children = true;
            if (children) setExpanded(ti, !trk.expanded);
            setSelectedTrack(ti);
        }
        e.consumed = true;
        return;
    }
    if (trk.locked) {
        clearSelection();
        return;
    }
    const auto rows = visibleTracks();
    int row = 0;
    while (row < static_cast<int>(rows.size()) && rows[row] != ti) ++row;

    // Keyframe hit (diamond proximity)
    for (int ki = 0; ki < static_cast<int>(trk.keyframes.size()); ++ki) {
        float kx = timeToX(trk.keyframes[ki].time);
        float ky = b.y + kRulerH + row * kTrackH - verticalScroll_ + kTrackH * 0.5f;
        if (std::fabs(e.x - kx) + std::fabs(e.y - ky) < 8) {
            clearSelection();
            trk.keyframes[ki].selected = true;
            dragMode_     = DragMode::MoveKey;
            dragTrack_    = ti;
            dragIndex_    = ki;
            dragOrigTime_ = trk.keyframes[ki].time;
            dragStartX_   = e.x;
            onKeyframeSelected.emit(ti, ki);
            if (e.clickCount >= 2) onKeyframeDoubleClicked.emit(ti, ki);
            e.consumed = true;
            return;
        }
    }

    // Clip hit
    for (int ci = 0; ci < static_cast<int>(trk.clips.size()); ++ci) {
        float cx0 = timeToX(trk.clips[ci].start);
        float cx1 = timeToX(trk.clips[ci].end);
        float cy  = b.y + kRulerH + row * kTrackH - verticalScroll_ + 3;
        float ch  = kTrackH - 6;

        if (e.x >= cx0 && e.x <= cx1 && e.y >= cy && e.y <= cy + ch) {
            // Ctrl+click toggles the clip, Shift+click adds it, a plain click selects only it; a plain click on a clip
            // that is already selected keeps the others, so they can be dragged together (a click without a drag
            // keeps only that clip). The edges of the bar resize and stretch, they do not change the selection.
            const bool edge = e.x < cx0 + 6 || e.x > cx1 - 6;
            bool collapse = false;
            if (!edge && e.ctrl) {
                clearKeySelection();
                trk.clips[ci].selected = !trk.clips[ci].selected;
                if (trk.clips[ci].selected) onClipSelected.emit(ti, ci);
                markDirty();
                e.consumed = true;
                return;
            }
            if (!edge && e.shift) {
                clearKeySelection();
                trk.clips[ci].selected = true;
            } else if (!edge && trk.clips[ci].selected) {
                clearKeySelection();
                collapse = selectedClipCount() > 1;
            } else {
                clearSelection();
                trk.clips[ci].selected = true;
            }
            onClipSelected.emit(ti, ci);

            // The volume curve inside the bar: a point is grabbed, a double-click adds or removes one.
            // The first and last points sit on the edges of the bar, where a drag would resize it: a point is
            // grabbed there too when the pointer is on it.
            TimelineClip& clip = trk.clips[ci];
            if (clip.editableEnvelope) {
                const bool inside = e.x > cx0 + 6 && e.x < cx1 - 6;
                const int hit = envelopeKeyAt(clip, e.x, e.y, cy, ch);
                if (inside && e.clickCount >= 2) {
                    if (hit >= 0) {
                        clip.envelope.erase(clip.envelope.begin() + hit);
                    } else {
                        const float time = std::max(clip.offset, std::min(clip.offset + (clip.end - clip.start) * clip.speed,
                                                    clip.offset + (xToTime(e.x) - clip.start) * clip.speed));
                        const float f = 1.0f - (e.y - cy - 2.0f) / (ch - 4.0f);
                        const float value = std::max(clip.envelopeMin, std::min(clip.envelopeMax,
                                                     clip.envelopeMin + f * (clip.envelopeMax - clip.envelopeMin)));
                        CurveKey added;
                        added.time = time;
                        added.value = value;
                        if (clip.envelope.empty()) {
                            // A new curve starts flat from one end of the clip to the other.
                            CurveKey first, last;
                            first.time = clip.offset;
                            last.time = clip.offset + (clip.end - clip.start) * clip.speed;
                            first.value = last.value = clip.envelopeRest;
                            clip.envelope.push_back(first);
                            clip.envelope.push_back(last);
                        }
                        int at = 0;
                        while (at < static_cast<int>(clip.envelope.size()) && clip.envelope[at].time < time) ++at;
                        clip.envelope.insert(clip.envelope.begin() + at, added);
                    }
                    for (int k = 0; k < static_cast<int>(clip.envelope.size()); ++k)
                        CurveEditor::autoTangentsAt(clip.envelope, k, true);
                    onEnvelopeChanged.emit(ti, ci);
                    markDirty();
                    e.consumed = true;
                    return;
                }
                if (hit >= 0) {
                    dragMode_  = DragMode::MoveEnvelopeKey;
                    dragTrack_ = ti;
                    dragIndex_ = ci;
                    dragKey_   = hit;
                    e.consumed = true;
                    return;
                }
            }

            if      (e.x < cx0 + 6)  dragMode_ = DragMode::ResizeClipL;
            else if (e.x > cx1 - 6)  dragMode_ = e.ctrl ? DragMode::StretchClipR : DragMode::ResizeClipR; // Ctrl: change the speed
            else                      dragMode_ = DragMode::MoveClip;

            dragTrack_    = ti;
            dragIndex_    = ci;
            dragOrigTime_ = trk.clips[ci].start;
            dragOrigEnd_  = trk.clips[ci].end;
            dragOrigOffset_ = trk.clips[ci].offset;
            dragOrigSpeed_  = trk.clips[ci].speed;
            dragMoved_    = false;
            dragStartX_   = e.x;
            dragGroup_.clear();
            if (dragMode_ == DragMode::MoveClip) {
                ct::Vector<TimelineClipRef> picked;
                selectedClips(picked);
                if (picked.size() > 1)
                    for (size_t i = 0; i < picked.size(); ++i) {
                        DragMember member;
                        member.track = picked[i].track;
                        member.clip  = picked[i].clip;
                        member.start = tracks_[member.track].clips[member.clip].start;
                        member.end   = tracks_[member.track].clips[member.clip].end;
                        dragGroup_.push_back(member);
                    }
            }
            collapseOnRelease_ = collapse && dragMode_ == DragMode::MoveClip;
            e.consumed    = true;
            return;
        }
    }
    if (e.clickCount >= 2) {
        clearSelection();
        const int key = addKeyframe(ti, std::clamp(xToTime(e.x), viewStart_, viewEnd_));
        onKeyframeAdded.emit(ti, key);
        e.consumed = true;
        return;
    }
    // Empty part of a track: a drag draws a rectangle that selects the clips inside it.
    if (e.shift || e.ctrl) clearKeySelection();
    else clearSelection();
    marqueeBase_.clear();
    selectedClips(marqueeBase_);
    marqueeX0_ = marqueeX1_ = e.x;
    marqueeY0_ = marqueeY1_ = e.y;
    dragMode_ = DragMode::Marquee;
    e.consumed = true;
}

void Timeline::updateMarquee()
{
    const Rect b = absoluteRect();
    const float x0 = std::min(marqueeX0_, marqueeX1_), x1 = std::max(marqueeX0_, marqueeX1_);
    const float y0 = std::min(marqueeY0_, marqueeY1_), y1 = std::max(marqueeY0_, marqueeY1_);
    const auto rows = visibleTracks();
    for (int ti = 0; ti < trackCount(); ++ti) {
        int row = 0;
        while (row < static_cast<int>(rows.size()) && rows[row] != ti) ++row;
        const bool shown = row < static_cast<int>(rows.size()) && !tracks_[ti].locked;
        const float cy = b.y + kRulerH + row * kTrackH - verticalScroll_ + 3;
        const float ch = kTrackH - 6;
        for (int ci = 0; ci < static_cast<int>(tracks_[ti].clips.size()); ++ci) {
            auto& clip = tracks_[ti].clips[ci];
            bool selected = false;
            for (size_t i = 0; i < marqueeBase_.size() && !selected; ++i)
                selected = marqueeBase_[i].track == ti && marqueeBase_[i].clip == ci;
            if (shown) {
                const float cx0 = timeToX(clip.start), cx1 = timeToX(clip.end);
                if (cx1 > b.x + kHeaderW && cx1 >= x0 && cx0 <= x1 && cy + ch >= y0 && cy <= y1) selected = true;
            }
            clip.selected = selected;
        }
    }
}

void Timeline::onMouseRelease(MouseEvent& e)
{
    const bool marquee = dragMode_ == DragMode::Marquee;
    const bool collapse = collapseOnRelease_ && !dragMoved_ && dragMode_ == DragMode::MoveClip &&
        dragTrack_ >= 0 && dragTrack_ < static_cast<int>(tracks_.size()) && dragIndex_ >= 0 &&
        dragIndex_ < static_cast<int>(tracks_[dragTrack_].clips.size());
    if (collapse) {
        clearSelection();
        tracks_[dragTrack_].clips[dragIndex_].selected = true;
    }
    collapseOnRelease_ = false;
    ct::Vector<DragMember> group = dragGroup_;
    dragGroup_.clear();
    const bool edited = dragMoved_ && dragTrack_ >= 0 && dragIndex_ >= 0 &&
        (dragMode_ == DragMode::MoveClip || dragMode_ == DragMode::ResizeClipL || dragMode_ == DragMode::ResizeClipR ||
         dragMode_ == DragMode::StretchClipR);
    const int editedTrack = dragTrack_;
    const int editedClip = dragIndex_;
    const bool keyDragged = dragMode_ == DragMode::MoveKey && dragTrack_ >= 0 &&
        dragTrack_ < static_cast<int>(tracks_.size()) && dragIndex_ >= 0 &&
        dragIndex_ < static_cast<int>(tracks_[dragTrack_].keyframes.size());
    const float droppedTime = keyDragged ? tracks_[dragTrack_].keyframes[dragIndex_].time : 0.0f;
    const float startTime = dragOrigTime_;
    dragMoved_ = false;
    dragMode_  = DragMode::None;
    dragTrack_ = -1;
    dragIndex_ = -1;
    e.consumed = true;
    if (marquee) {
        markDirty();
        ct::Vector<TimelineClipRef> picked;
        selectedClips(picked);
        if (!picked.empty()) onClipSelected.emit(picked[0].track, picked[0].clip);
    }
    if (edited && group.size() > 1) {
        for (size_t i = 0; i < group.size(); ++i) onClipEdited.emit(group[i].track, group[i].clip);
    } else if (edited) {
        onClipEdited.emit(editedTrack, editedClip);
    }
    if (keyDragged && droppedTime != startTime)
        onKeyframeDropped.emit(editedTrack, editedClip, startTime, droppedTime);
}

bool Timeline::clipAtPoint(float x, float y, int& track, int& clip) const
{
    const Rect b = absoluteRect();
    if (x < b.x + kHeaderW || y < b.y + kRulerH) return false;
    const int ti = trackAtY(y);
    if (ti < 0) return false;
    const auto rows = visibleTracks();
    int row = 0;
    while (row < static_cast<int>(rows.size()) && rows[row] != ti) ++row;
    const float cy = b.y + kRulerH + row * kTrackH - verticalScroll_ + 3;
    const float ch = kTrackH - 6;
    if (y < cy || y > cy + ch) return false;
    const auto& clips = tracks_[ti].clips;
    for (int ci = 0; ci < static_cast<int>(clips.size()); ++ci) {
        if (x < timeToX(clips[ci].start) || x > timeToX(clips[ci].end)) continue;
        track = ti;
        clip = ci;
        return true;
    }
    return false;
}

void Timeline::updateClipTooltip(float x, float y)
{
    int ti = 0, ci = 0;
    if (!clipTooltip_ || !clipAtPoint(x, y, ti, ci)) {
        if (!tooltip().empty()) setTooltip("");
        return;
    }
    const String text = clipTooltip_(ti, ci);
    if (text != tooltip()) setTooltip(text);
}

void Timeline::onMouseMove(MouseEvent& e)
{
    if (dragMode_ == DragMode::None) updateClipTooltip(e.x, e.y);
    switch (dragMode_) {
    case DragMode::Scrub:
        playhead_ = xToTime(e.x);
        onPlayheadChanged.emit(playhead_);
        markDirty();
        e.consumed = true;
        break;

    case DragMode::Pan: {
        float dt  = xToTime(dragStartX_) - xToTime(e.x);
        viewStart_ = panStartView_ + dt;
        viewEnd_   = panStartEnd_  + dt;
        markDirty();
        e.consumed = true;
        break;
    }
    case DragMode::MoveKey: {
        float dt = xToTime(e.x) - xToTime(dragStartX_);
        const float snapped = snap(dragOrigTime_ + dt, -1, -1, dragTrack_, dragIndex_);
        const float time = std::clamp(snapped, viewStart_, viewEnd_);
        tracks_[dragTrack_].keyframes[dragIndex_].time = time;
        onKeyframeMoved.emit(dragTrack_, dragIndex_, time);
        markDirty();
        e.consumed = true;
        break;
    }
    case DragMode::Marquee:
        marqueeX1_ = e.x;
        marqueeY1_ = e.y;
        updateMarquee();
        markDirty();
        e.consumed = true;
        break;

    case DragMode::MoveClip: {
        const float dt = xToTime(e.x) - xToTime(dragStartX_);
        const float len = dragOrigEnd_ - dragOrigTime_;
        float start = dragOrigTime_ + dt;
        float edge = 0.0f;
        if (snapToEdges(start, dragTrack_, dragIndex_, -1, -1, edge)) start = edge;
        else if (snapToEdges(start + len, dragTrack_, dragIndex_, -1, -1, edge)) start = edge - len;
        else if (snapping_) start = snapFrame(start, fps_);
        start = std::max(0.0f, start);
        // Several selected clips move together by the same amount, each on its own track.
        if (dragGroup_.size() > 1) {
            float delta = start - dragOrigTime_;
            float earliest = dragGroup_[0].start;
            for (size_t i = 1; i < dragGroup_.size(); ++i) earliest = std::min(earliest, dragGroup_[i].start);
            delta = std::max(delta, -earliest);
            for (size_t i = 0; i < dragGroup_.size(); ++i) {
                auto& moved = tracks_[dragGroup_[i].track].clips[dragGroup_[i].clip];
                moved.start = dragGroup_[i].start + delta;
                moved.end   = dragGroup_[i].end + delta;
            }
            dragMoved_ = true;
            for (size_t i = 0; i < dragGroup_.size(); ++i) onClipChanged.emit(dragGroup_[i].track, dragGroup_[i].clip);
            markDirty();
            e.consumed = true;
            break;
        }
        // Dragging onto another track of the same kind moves the clip there.
        const int under = trackAtY(e.y);
        if (under >= 0 && under != dragTrack_ && tracks_[under].kind == tracks_[dragTrack_].kind && !tracks_[under].locked) {
            TimelineClip moved = tracks_[dragTrack_].clips[dragIndex_];
            tracks_[dragTrack_].clips.erase(tracks_[dragTrack_].clips.begin() + dragIndex_);
            tracks_[under].clips.push_back(moved);
            dragTrack_ = under;
            dragIndex_ = static_cast<int>(tracks_[under].clips.size()) - 1;
        }
        auto& clip = tracks_[dragTrack_].clips[dragIndex_];
        clip.start = start;
        clip.end   = start + len;
        dragMoved_ = true;
        onClipChanged.emit(dragTrack_, dragIndex_);
        markDirty();
        e.consumed = true;
        break;
    }
    case DragMode::ResizeClipL: {
        const float dt = xToTime(e.x) - xToTime(dragStartX_);
        auto& clip = tracks_[dragTrack_].clips[dragIndex_];
        float start = snap(dragOrigTime_ + dt, dragTrack_, dragIndex_);
        // Trimming the head cannot reveal media before its first frame.
        if (clip.sourceLength > 0) start = std::max(start, dragOrigTime_ - dragOrigOffset_ / clip.speed);
        start = std::max(0.0f, std::min(start, clip.end - 0.04f));
        if (clip.sourceLength > 0) clip.offset = dragOrigOffset_ + (start - dragOrigTime_) * clip.speed;
        clip.start = start;
        dragMoved_ = true;
        onClipChanged.emit(dragTrack_, dragIndex_);
        markDirty();
        e.consumed = true;
        break;
    }
    case DragMode::ResizeClipR: {
        const float dt = xToTime(e.x) - xToTime(dragStartX_);
        auto& clip = tracks_[dragTrack_].clips[dragIndex_];
        float end = snap(dragOrigEnd_ + dt, dragTrack_, dragIndex_);
        end = std::max(end, clip.start + 0.04f);
        if (clip.sourceLength > 0) end = std::min(end, clip.start + (clip.sourceLength - clip.offset) / clip.speed);
        clip.end = end;
        dragMoved_ = true;
        onClipChanged.emit(dragTrack_, dragIndex_);
        markDirty();
        e.consumed = true;
        break;
    }
    case DragMode::MoveEnvelopeKey: {
        auto& clip = tracks_[dragTrack_].clips[dragIndex_];
        if (dragKey_ < 0 || dragKey_ >= static_cast<int>(clip.envelope.size())) break;
        Rect b = absoluteRect();
        const auto rows = visibleTracks();
        int row = 0;
        while (row < static_cast<int>(rows.size()) && rows[row] != dragTrack_) ++row;
        const float cy = b.y + kRulerH + row * kTrackH - verticalScroll_ + 3;
        const float ch = kTrackH - 6;
        float time = clip.offset + (xToTime(e.x) - clip.start) * clip.speed;
        const float lo = clip.offset, hi = clip.offset + (clip.end - clip.start) * clip.speed;
        time = std::max(lo, std::min(hi, time));
        // Points keep their order along the clip.
        if (dragKey_ > 0) time = std::max(time, clip.envelope[dragKey_ - 1].time + 0.001f);
        if (dragKey_ + 1 < static_cast<int>(clip.envelope.size())) time = std::min(time, clip.envelope[dragKey_ + 1].time - 0.001f);
        const float f = 1.0f - (e.y - cy - 2.0f) / (ch - 4.0f);
        const float value = std::max(clip.envelopeMin, std::min(clip.envelopeMax,
                                     clip.envelopeMin + f * (clip.envelopeMax - clip.envelopeMin)));
        clip.envelope[dragKey_].time = time;
        clip.envelope[dragKey_].value = value;
        for (int k = std::max(0, dragKey_ - 1); k <= std::min(static_cast<int>(clip.envelope.size()) - 1, dragKey_ + 1); ++k)
            CurveEditor::autoTangentsAt(clip.envelope, k, true);
        onEnvelopeChanged.emit(dragTrack_, dragIndex_);
        markDirty();
        e.consumed = true;
        break;
    }
    case DragMode::StretchClipR: {
        // The clip keeps the same piece of media but takes more or less time: that is the speed.
        const float dt = xToTime(e.x) - xToTime(dragStartX_);
        auto& clip = tracks_[dragTrack_].clips[dragIndex_];
        const float used = (dragOrigEnd_ - dragOrigTime_) * dragOrigSpeed_; // seconds of media
        float end = snap(dragOrigEnd_ + dt, dragTrack_, dragIndex_);
        end = std::max(end, clip.start + 0.04f);
        float speed = used / (end - clip.start);
        speed = std::max(0.1f, std::min(32.0f, speed));
        clip.speed = speed;
        clip.end = clip.start + used / speed;
        dragMoved_ = true;
        onClipChanged.emit(dragTrack_, dragIndex_);
        markDirty();
        e.consumed = true;
        break;
    }
    case DragMode::None:
        break;
    }
}

void Timeline::onKeyPress(KeyEvent& e)
{
    onKeyPressed.emit(e.key, (e.shift ? 1 : 0) | (e.ctrl ? 2 : 0));
    e.consumed = true;
}

bool Timeline::acceptsDrop(const DragPayload& p)
{
    (void)p;
    return true;
}

void Timeline::onDropReceive(const DragPayload& p)
{
    onDrop.emit(p, trackAtY(p.dropY), xToTime(p.dropX));
}

void Timeline::onMouseScroll(MouseEvent& e)
{
    if (e.scrollY == 0) return;
    const bool overHeader = e.x < absoluteRect().x + kHeaderW;
    const bool overBar = vbar_->isVisible() && vbar_->absoluteRect().contains(e.x, e.y);
    if (labelTrack_ >= 0) finishLabelEdit(true, true);
    if (overHeader || overBar || (wheelScrollsTracks_ && !e.ctrl && !e.shift)) {
        setVerticalScroll(verticalScroll_ - e.scrollY * kTrackH);
        e.consumed = true; return;
    }
    // Zoom centred on mouse X
    float tAtMouse = xToTime(e.x);
    float factor   = (e.scrollY > 0) ? 0.9f : 1.1f;
    const float nextRange = (viewEnd_ - viewStart_) * factor;
    if (!std::isfinite(nextRange) || nextRange < 0.001f || nextRange > 1000000.0f) return;
    viewStart_ = tAtMouse + (viewStart_ - tAtMouse) * factor;
    viewEnd_   = tAtMouse + (viewEnd_   - tAtMouse) * factor;
    markDirty();
    e.consumed = true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Naming a keyframe in place
// ─────────────────────────────────────────────────────────────────────────────

void Timeline::editKeyframeLabel(int trackId, int keyIdx)
{
    if (trackId < 0 || trackId >= trackCount() || tracks_[trackId].locked) return;
    auto& keys = tracks_[trackId].keyframes;
    if (keyIdx < 0 || keyIdx >= static_cast<int>(keys.size())) return;
    if (labelTrack_ >= 0) finishLabelEdit(true, false);
    const auto rows = visibleTracks();
    int row = 0;
    while (row < static_cast<int>(rows.size()) && rows[row] != trackId) ++row;
    if (row >= static_cast<int>(rows.size())) return;
    if (!labelInput_) {
        labelInput_ = createChild<LabelInput>(this);
        labelInput_->setVisible(false);
    }
    const Rect b = absoluteRect();
    constexpr float width = 150.0f;
    float x = timeToX(keys[keyIdx].time) - b.x + 8.0f;
    x = std::min(x, std::max(kHeaderW, b.w - width - 2.0f));
    const float y = kRulerH + row * kTrackH - verticalScroll_ + 2.0f;
    labelInput_->setRect({x, y, width, kTrackH - 4.0f});
    labelInput_->setText(keys[keyIdx].label);
    labelInput_->setVisible(true);
    labelTrack_ = trackId;
    labelKey_ = keyIdx;
    WidgetApp::instance().setFocused(labelInput_);
    labelInput_->selectAll();
    markDirty();
}

void Timeline::finishLabelEdit(bool keep, bool refocus)
{
    if (labelTrack_ < 0) return;
    const int ti = labelTrack_;
    const int ki = labelKey_;
    labelTrack_ = -1;
    labelKey_ = -1;
    const String text = labelInput_ ? labelInput_->text() : String();
    if (labelInput_) labelInput_->setVisible(false);
    markDirty();
    if (refocus) WidgetApp::instance().setFocused(this);
    if (keep && ti < trackCount() && ki < static_cast<int>(tracks_[ti].keyframes.size())) {
        tracks_[ti].keyframes[ki].label = text;
        onKeyframeLabelEdited.emit(ti, ki, text);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Painting — main entry
// ─────────────────────────────────────────────────────────────────────────────

void Timeline::paint(PaintContext& ctx)
{
    if (!visible_) return;
    Rect b = absoluteRect();
    syncScrollBar();

    ctx.pushClip(b);

    // Background
    ctx.fill.SetColor(bgColor_.r, bgColor_.g, bgColor_.b, bgColor_.a);
    ctx.fillRect(b.x, b.y, b.w, b.h);

    paintRuler(ctx, b);
    ctx.pushClip({b.x, b.y + kRulerH, b.w, std::max(0.0f, b.h - kRulerH)});
    paintTracks(ctx, b);
    ctx.popClip();
    paintMarkedRange(ctx, b);
    paintMarquee(ctx, b);
    paintPlayhead(ctx, b);

    // Border (4 edges)
    ctx.fill.SetColor(50, 52, 58, 255);
    ctx.fillRect(b.x,           b.y,           b.w, 1);
    ctx.fillRect(b.x,           b.y + b.h - 1, b.w, 1);
    ctx.fillRect(b.x,           b.y,           1,   b.h);
    ctx.fillRect(b.x + b.w - 1, b.y,           1,   b.h);

    ctx.popClip();

    Widget::paint(ctx);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ruler — time ticks with auto-scaled step
// ─────────────────────────────────────────────────────────────────────────────

void Timeline::paintRuler(PaintContext& ctx, const Rect& b)
{
    // Ruler background
    ctx.fill.SetColor(38, 40, 46, 255);
    ctx.fillRect(b.x, b.y, b.w, kRulerH);

    // Compute nice tick step
    float range = viewEnd_ - viewStart_;
    float rough = range / 10.0f;
    float mag   = std::pow(10.0f, std::floor(std::log10(rough)));
    float norm  = rough / mag;
    float step;
    if      (norm < 1.5f) step = mag;
    else if (norm < 3.5f) step = 2 * mag;
    else if (norm < 7.5f) step = 5 * mag;
    else                  step = 10 * mag;

    float asc = setupFont(ctx, Color(150, 152, 160, 200), 10.0f);

    float tStart = std::floor(viewStart_ / step) * step;
    for (float t = tStart; t <= viewEnd_; t += step) {
        float x = timeToX(t);
        if (x < b.x + kHeaderW || x > b.x + b.w) continue;

        // Major tick line
        ctx.fill.SetColor(80, 82, 90, 255);
        ctx.fillRect(x, b.y + kRulerH - 10, 1, 10);

        // Label
        char buf[16];
        if (fps_ > 0 && step < 1.0f)
            snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::round(t * fps_)));
        else if (t >= 60.0f)
            snprintf(buf, sizeof(buf), "%d:%02d", static_cast<int>(t) / 60, static_cast<int>(t) % 60);
        else
            snprintf(buf, sizeof(buf), "%gs", std::round(t * 100.0f) / 100.0f);

        ctx.font.SetColor(Color(150, 152, 160, 200));
        ctx.font.Print(buf, x + 2, b.y + (kRulerH - 10.0f) * 0.5f + asc);

        // Minor ticks (4 subdivisions)
        float minor = step / 4;
        ctx.fill.SetColor(55, 57, 63, 255);
        for (int m = 1; m < 4; ++m) {
            float mx = timeToX(t + m * minor);
            if (mx < b.x + kHeaderW || mx > b.x + b.w) continue;
            ctx.fillRect(mx, b.y + kRulerH - 5, 1, 5);
        }
    }

    // The markers: a flag with the name, wherever the tracks are scrolled to
    for (const auto& trk : tracks_) {
        if (!trk.rulerMarks) continue;
        const float ascLabel = setupFont(ctx, trk.color, 9.0f);
        for (const auto& kf : trk.keyframes) {
            const float x = timeToX(kf.time);
            if (x < b.x + kHeaderW || x > b.x + b.w) continue;
            ctx.fill.SetColor(trk.color.r, trk.color.g, trk.color.b, 230);
            ctx.fillRect(x, b.y + 1, 1, kRulerH - 2);
            ctx.fillTriangle(x, b.y + 1, x + 6, b.y + 4, x, b.y + 7);
            if (!kf.label.empty()) ctx.font.Print(kf.label.c_str(), x + 8, b.y + 1 + ascLabel);
        }
    }

    // Separator line at ruler bottom
    ctx.fill.SetColor(60, 62, 68, 255);
    ctx.fillRect(b.x, b.y + kRulerH, b.w, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tracks — header, clips, keyframes
// ─────────────────────────────────────────────────────────────────────────────

void Timeline::paintTracks(PaintContext& ctx, const Rect& b)
{
    float ascName = setupFont(ctx, Color(180, 180, 180, 255), 10.0f);

    const auto rows = visibleTracks();
    for (int row = 0; row < static_cast<int>(rows.size()); ++row) {
        const int ti = rows[row];
        const auto& trk = tracks_[ti];
        float ty = b.y + kRulerH + row * kTrackH - verticalScroll_;
        if (ty + kTrackH <= b.y + kRulerH || ty >= b.bottom()) continue;

        // Track background (alternating)
        Color bg = (ti % 2 == 0) ? Color(34, 36, 40, 255) : Color(30, 32, 36, 255);
        ctx.fill.SetColor(bg.r, bg.g, bg.b, bg.a);
        ctx.fillRect(b.x, ty, b.w, kTrackH);

        // Header area
        if (ti == selectedTrack_) ctx.fill.SetColor(52, 60, 80, 255);
        else                      ctx.fill.SetColor(36, 38, 44, 255);
        ctx.fillRect(b.x, ty, kHeaderW, kTrackH);

        // Track name — vertically centred in track header
        Color tc = trk.muted ? Color(80, 80, 80, 180) : trk.color;
        ctx.font.SetColor(tc);
        float nameY = ty + (kTrackH - 10.0f) * 0.5f + ascName;
        int depth = 0;
        for (int p = trk.parent; p >= 0; p = tracks_[p].parent) ++depth;
        bool children = false;
        for (const auto& child : tracks_) if (child.parent == ti) children = true;
        ctx.pushClip({b.x, b.y + kRulerH, kHeaderW - 2 * kBtn - kBtnGap - 10.0f, std::max(0.0f, b.h - kRulerH)});
        if (children) ctx.font.Print(trk.expanded ? "v" : ">", b.x + 6 + depth * 14, nameY);
        ctx.font.Print(trk.name.c_str(), b.x + 20 + depth * 14, nameY);
        ctx.popClip();

        // Mute and lock buttons
        ctx.pushClip({b.x, std::max(ty, b.y + kRulerH), kHeaderW, kTrackH});
        const Rect mb = muteButton(b.x, kHeaderW, ty, kTrackH);
        const Rect lb = lockButton(b.x, kHeaderW, ty, kTrackH);
        ctx.fill.SetColor(trk.muted ? Color(200, 70, 70, 255) : Color(56, 58, 66, 255));
        ctx.fillRect(mb.x, mb.y, mb.w, mb.h);
        ctx.fill.SetColor(trk.locked ? Color(210, 170, 60, 255) : Color(56, 58, 66, 255));
        ctx.fillRect(lb.x, lb.y, lb.w, lb.h);
        const float asc9 = setupFont(ctx, Color(235, 235, 240, 255), 9.0f);
        ctx.font.Print("M", mb.x + 3.5f, mb.y + (kBtn - 9.0f) * 0.5f + asc9);
        ctx.font.Print("L", lb.x + 4.5f, lb.y + (kBtn - 9.0f) * 0.5f + asc9);
        ctx.popClip();
        ctx.pushClip({b.x + kHeaderW, b.y + kRulerH, std::max(0.0f, b.w - kHeaderW), std::max(0.0f, b.h - kRulerH)});

        // Header separator
        ctx.fill.SetColor(50, 52, 58, 255);
        ctx.fillRect(b.x + kHeaderW, ty, 1, kTrackH);

        // ── Clips ────────────────────────────────────────────────────────
        for (int ci = 0; ci < static_cast<int>(trk.clips.size()); ++ci) {
            const auto& clip = trk.clips[ci];
            float cx0  = timeToX(clip.start);
            float cx1  = timeToX(clip.end);
            float clipY = ty + 3;
            float clipH = kTrackH - 6;

            if (cx1 < b.x + kHeaderW || cx0 > b.x + b.w) continue;

            // Clip body
            Color cc = clip.selected ? Color(255, 200, 60, 200) : clip.color;
            ctx.fill.SetColor(cc.r, cc.g, cc.b, cc.a);
            ctx.fillRect(cx0, clipY, cx1 - cx0, clipH);

            // Clip border (4 edges)
            uint8_t ba = clip.selected ? 255 : 120;
            ctx.fill.SetColor(200, 202, 210, ba);
            ctx.fillRect(cx0,         clipY,             cx1 - cx0, 1);
            ctx.fillRect(cx0,         clipY + clipH - 1, cx1 - cx0, 1);
            ctx.fillRect(cx0,         clipY,             1,         clipH);
            ctx.fillRect(cx1 - 1,     clipY,             1,         clipH);

            // The waveform of the sound: a bar for every couple of pixels, as loud as the sound is there
            if (peakProvider_ && trk.kind == TrackKind::Audio && clip.media >= 0) {
                const float x0 = std::max(cx0, b.x + kHeaderW);
                const float x1 = std::min(cx1, b.x + b.w);
                const float mid = clipY + clipH * 0.5f;
                float previous = clip.offset + (xToTime(x0) - clip.start) * clip.speed;
                ctx.fill.SetColor(255, 255, 255, 105);
                for (float x = x0; x < x1; x += 2.0f) {
                    const float source = clip.offset + (xToTime(x + 2.0f) - clip.start) * clip.speed;
                    const float peak = peakProvider_(clip.media, std::min(previous, source), std::max(previous, source));
                    previous = source;
                    if (peak <= 0.0f) continue;
                    // the quiet parts show too: the loudness is drawn by its square root
                    const float h = std::min(1.0f, std::sqrt(peak)) * (clipH * 0.5f - 1.0f);
                    ctx.fillRect(x, mid - h, 2.0f, std::max(1.0f, h * 2.0f));
                }
            }

            // Envelope: the curve of the clip (volume), over the media's time
            if (!clip.envelope.empty() && clip.envelopeMax > clip.envelopeMin) {
                ctx.fill.SetColor(255, 255, 255, 230);
                const float x0 = std::max(cx0, b.x + kHeaderW);
                const float x1 = std::min(cx1, b.x + b.w);
                float px = 0, py = 0;
                for (float x = x0; x <= x1 + 2.0f; x += 2.0f) {
                    const float xs = std::min(x, x1);
                    const float t = xToTime(xs);
                    const float v = CurveEditor::evaluateKeys(clip.envelope, clip.offset + (t - clip.start) * clip.speed);
                    const float f = std::max(0.0f, std::min(1.0f, (v - clip.envelopeMin) / (clip.envelopeMax - clip.envelopeMin)));
                    const float y = clipY + clipH - 2.0f - f * (clipH - 4.0f);
                    if (x > x0) {
                        const float dx = xs - px, dy = y - py;
                        const float len = std::sqrt(dx * dx + dy * dy);
                        if (len > 0.1f) {
                            const float nx = -dy / len * 0.6f, ny = dx / len * 0.6f;
                            ctx.fillTriangle(px + nx, py + ny, px - nx, py - ny, xs - nx, y - ny);
                            ctx.fillTriangle(px + nx, py + ny, xs - nx, y - ny, xs + nx, y + ny);
                        }
                    }
                    px = xs; py = y;
                }
            }

            // The points of the curve
            if (clip.editableEnvelope && !clip.envelope.empty()) {
                for (int k = 0; k < static_cast<int>(clip.envelope.size()); ++k) {
                    const float kx = envelopeX(clip, clip.envelope[k]);
                    if (kx < cx0 || kx > cx1) continue;
                    const float ky = envelopeY(clip, clip.envelope[k].value, clipY, clipH);
                    ctx.fill.SetColor(255, 255, 255, 255);
                    ctx.fillRect(kx - 3, ky - 3, 6, 6);
                    ctx.fill.SetColor(20, 20, 20, 255);
                    ctx.fillRect(kx - 2, ky - 2, 4, 4);
                }
            }

            // Clip label
            if (!clip.label.empty() && cx1 - cx0 > 30) {
                float ascClip = setupFont(ctx, Color(240, 242, 250, 230), 9.0f);
                ctx.font.Print(clip.label.c_str(), cx0 + 4, clipY + (clipH - 9.0f) * 0.5f + ascClip);
            }
        }

        // ── Overlaps: where two clips of a track are on top of each other ──
        for (int a = 0; a < static_cast<int>(trk.clips.size()); ++a) {
            for (int o = a + 1; o < static_cast<int>(trk.clips.size()); ++o) {
                const float s = std::max(trk.clips[a].start, trk.clips[o].start);
                const float e = std::min(trk.clips[a].end, trk.clips[o].end);
                if (e - s < 0.01f || trk.clips[a].generator >= 0 || trk.clips[o].generator >= 0) continue;
                const float x0 = timeToX(s), x1 = timeToX(e);
                if (x1 < b.x + kHeaderW || x0 > b.x + b.w) continue;
                ctx.fill.SetColor(255, 255, 255, 70);
                ctx.fillRect(x0, ty + 3, x1 - x0, kTrackH - 6);
                // a diagonal across the band: two clips crossing
                ctx.fill.SetColor(255, 255, 255, 200);
                ctx.fillTriangle(x0, ty + kTrackH - 4, x0 + 1.5f, ty + kTrackH - 4, x1, ty + 3);
                ctx.fillTriangle(x0, ty + kTrackH - 4, x1, ty + 3, x1 - 1.5f, ty + 3);
            }
        }

        // ── Keyframes (diamonds) ─────────────────────────────────────────
        for (int ki = 0; ki < static_cast<int>(trk.keyframes.size()); ++ki) {
            const auto& kf = trk.keyframes[ki];
            float kx = timeToX(kf.time);
            float ky = ty + kTrackH * 0.5f;

            if (kx < b.x + kHeaderW || kx > b.x + b.w) continue;

            float ds = kf.selected ? 6.0f : 5.0f;
            Color kc = kf.selected ? Color(255, 200, 60, 255) : trk.color;
            ctx.fill.SetColor(kc.r, kc.g, kc.b, kc.a);
            // Two triangles make a diamond
            ctx.fillTriangle(kx, ky - ds, kx + ds, ky, kx, ky + ds);
            ctx.fillTriangle(kx, ky - ds, kx, ky + ds, kx - ds, ky);

            if (!kf.label.empty() && !(labelTrack_ == ti && labelKey_ == ki)) {
                const float ascLabel = setupFont(ctx, Color(225, 228, 238, 235), 9.0f);
                ctx.font.Print(kf.label.c_str(), kx + ds + 4.0f, ty + (kTrackH - 9.0f) * 0.5f + ascLabel);
            }
        }

        ctx.popClip();
        // Track separator
        ctx.fill.SetColor(45, 47, 52, 255);
        ctx.fillRect(b.x, ty + kTrackH, b.w, 1);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Playhead — red vertical line + triangle marker
// ─────────────────────────────────────────────────────────────────────────────

void Timeline::paintMarquee(PaintContext& ctx, const Rect& b)
{
    if (dragMode_ != DragMode::Marquee) return;
    const float x0 = std::max(std::min(marqueeX0_, marqueeX1_), b.x + kHeaderW);
    const float x1 = std::min(std::max(marqueeX0_, marqueeX1_), b.x + b.w);
    const float y0 = std::max(std::min(marqueeY0_, marqueeY1_), b.y + kRulerH);
    const float y1 = std::min(std::max(marqueeY0_, marqueeY1_), b.y + b.h);
    if (x1 <= x0 || y1 <= y0) return;
    ctx.fill.SetColor(255, 200, 60, 36);
    ctx.fillRect(x0, y0, x1 - x0, y1 - y0);
    ctx.fill.SetColor(255, 200, 60, 200);
    ctx.fillRect(x0, y0, x1 - x0, 1);
    ctx.fillRect(x0, y1 - 1, x1 - x0, 1);
    ctx.fillRect(x0, y0, 1, y1 - y0);
    ctx.fillRect(x1 - 1, y0, 1, y1 - y0);
}

void Timeline::paintMarkedRange(PaintContext& ctx, const Rect& b)
{
    if (rangeOut_ <= rangeIn_) return;
    const float left = std::max(timeToX(rangeIn_), b.x + kHeaderW);
    const float right = std::min(timeToX(rangeOut_), b.x + b.w);
    if (right <= left) return;
    const float bottom = std::min(b.y + b.h, b.y + kRulerH + static_cast<float>(tracks_.size()) * kTrackH);
    ctx.fill.SetColor(90, 170, 255, 110);
    ctx.fillRect(left, b.y + kRulerH - 6, right - left, 6);
    ctx.fill.SetColor(90, 170, 255, 28);
    ctx.fillRect(left, b.y + kRulerH, right - left, std::max(0.0f, bottom - b.y - kRulerH));
    ctx.fill.SetColor(90, 170, 255, 200);
    ctx.fillRect(left, b.y, 1, bottom - b.y);
    ctx.fillRect(right - 1, b.y, 1, bottom - b.y);
}

void Timeline::paintPlayhead(PaintContext& ctx, const Rect& b)
{
    float x = timeToX(playhead_);
    if (x < b.x + kHeaderW || x > b.x + b.w) return;

    float top = b.y;
    float bot = b.y + kRulerH + static_cast<float>(tracks_.size()) * kTrackH;

    // Playhead line (2px for visibility)
    ctx.fill.SetColor(255, 80, 80, 220);
    ctx.fillRect(x, top, 1, bot - top);

    // Triangle marker at ruler bottom
    ctx.fill.SetColor(255, 80, 80, 240);
    ctx.fillTriangle(x - 6, b.y + kRulerH, x + 6, b.y + kRulerH, x, b.y + kRulerH - 8);
}
