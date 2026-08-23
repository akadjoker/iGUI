#pragma once

#include "Widget.hpp"
#include "Signal.hpp"
#include <vector>
#include <string>
#include <cmath>

namespace BuGUI
{
// ═════════════════════════════════════════════════════════════════════════════
//  Knob — Rotary knob control (270° arc style)
//
//  Interaction: drag up/down, double-click reset, scroll to nudge
// ═════════════════════════════════════════════════════════════════════════════

class Knob : public Widget
{
public:
    Knob();

    /// @brief Set the knob value.
    void  setValue(float v);
    /// @brief Get the current value.
    float value()      const { return value_; }
    /// @brief Get the value normalised to 0..1.
    float normalised() const { return (max_ - min_ > 0) ? (value_ - min_) / (max_ - min_) : 0.f; }

    /// @brief Set the min/max range.
    void  setRange(float mn, float mx) { min_ = mn; max_ = mx; setValue(value_); }
    /// @brief Set the default value for double-click reset.
    void  setDefault(float v)          { default_ = v; }
    /// @brief Get the minimum value.
    float minimum() const { return min_; }
    /// @brief Get the maximum value.
    float maximum() const { return max_; }

    /// @brief Set the display label.
    void setLabel(const std::string& l) { label_ = l; markDirty(); }
    /// @brief Get the current label.
    const std::string& label() const    { return label_; }

    /// @brief Set the arc color.
    void setArcColor(const Color& c)    { arcColor_   = c; markDirty(); }
    /// @brief Set the track background color.
    void setTrackColor(const Color& c)  { trackColor_ = c; markDirty(); }
    /// @brief Set the background color.
    void setBgColor(const Color& c)     { bgColor_    = c; markDirty(); }
    /// @brief Set whether to show the numeric value.
    void setShowValue(bool s)           { showValue_  = s; markDirty(); }
    /// @brief Set the drag sensitivity.
    void setSensitivity(float s)        { sensitivity_ = s; }

    /// @brief Emitted when the value changes.
    Signal<float> onChanged;

    Vec2f sizeHint() const override { return {56, 72}; }
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onMouseMove(MouseEvent& e) override;
    void  onMouseRelease(MouseEvent& e) override;
    void  onMouseScroll(MouseEvent& e) override;

private:
    float value_ = 0.5f, min_ = 0.f, max_ = 1.f, default_ = 0.5f;
    float sensitivity_ = 120.f;
    std::string label_;
    bool  showValue_ = true;
    Color arcColor_   = Color(100, 180, 240, 255);
    Color trackColor_ = Color( 50,  55,  65, 255);
    Color bgColor_    = Color( 28,  30,  36, 255);
    bool  dragging_ = false;
    float dragStartY_ = 0.f, dragStartV_ = 0.f;

    static constexpr float kStartAngle = 225.0f * (3.14159265f / 180.0f);
    static constexpr float kSweep      = 270.0f * (3.14159265f / 180.0f);
};

// ═════════════════════════════════════════════════════════════════════════════
//  LinearKnob — Horizontal/vertical slider with thumb
// ═════════════════════════════════════════════════════════════════════════════

class LinearKnob : public Widget
{
public:
    LinearKnob();

    /// @brief Set the slider value.
    void  setValue(float v);
    /// @brief Get the current value.
    float value()      const { return value_; }
    /// @brief Get the value normalised to 0..1.
    float normalised() const { return (max_ - min_ > 0) ? (value_ - min_) / (max_ - min_) : 0.f; }

    /// @brief Set the min/max range.
    void  setRange(float mn, float mx) { min_ = mn; max_ = mx; setValue(value_); }
    /// @brief Set the default value for double-click reset.
    void  setDefault(float v)          { default_ = v; }
    /// @brief Set the display label.
    void  setLabel(const std::string& l) { label_ = l; markDirty(); }
    /// @brief Set vertical or horizontal orientation.
    void  setOrientation(bool vertical)  { vertical_ = vertical; markDirty(); }
    /// @brief Set the fill color.
    void  setFillColor(const Color& c)   { fillColor_  = c; markDirty(); }
    /// @brief Set the track background color.
    void  setTrackColor(const Color& c)  { trackColor_ = c; markDirty(); }
    /// @brief Set the thumb color.
    void  setThumbColor(const Color& c)  { thumbColor_ = c; markDirty(); }
    /// @brief Set whether to show the numeric value.
    void  setShowValue(bool s)           { showValue_  = s; markDirty(); }

    /// @brief Emitted when the value changes.
    Signal<float> onChanged;

    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onMouseMove(MouseEvent& e) override;
    void  onMouseRelease(MouseEvent& e) override;
    void  onMouseScroll(MouseEvent& e) override;

private:
    float value_ = 0.5f, min_ = 0.f, max_ = 1.f, default_ = 0.5f;
    bool  vertical_ = false;
    std::string label_;
    bool  showValue_ = true;
    Color fillColor_  = Color(100, 180, 240, 255);
    Color trackColor_ = Color( 40,  44,  52, 255);
    Color thumbColor_ = Color(220, 222, 228, 255);
    bool  dragging_ = false;
    float dragStart_ = 0.f, dragStartV_ = 0.f;
    Rect  trackRect() const;
};

// ═════════════════════════════════════════════════════════════════════════════
//  ADSRWidget — Attack / Decay / Sustain / Release envelope editor
// ═════════════════════════════════════════════════════════════════════════════

class ADSRWidget : public Widget
{
public:
    ADSRWidget();

    /// @brief Set the attack time in seconds.
    void setAttack(float s)  { a_ = max2(0.001f, s); markDirty(); }
    /// @brief Set the decay time in seconds.
    void setDecay(float s)   { d_ = max2(0.001f, s); markDirty(); }
    /// @brief Set the sustain level (0..1).
    void setSustain(float v) { s_ = clamp(v, 0.0f, 1.0f); markDirty(); }
    /// @brief Set the release time in seconds.
    void setRelease(float s) { r_ = max2(0.001f, s); markDirty(); }
    /// @brief Set all ADSR envelope parameters at once.
    void setADSR(float a, float d, float s, float r);

    /// @brief Get the attack time.
    float attack()  const { return a_; }
    /// @brief Get the decay time.
    float decay()   const { return d_; }
    /// @brief Get the sustain level.
    float sustain() const { return s_; }
    /// @brief Get the release time.
    float release() const { return r_; }

    /// @brief Set the envelope line color.
    void setLineColor(const Color& c)   { lineColor_ = c; markDirty(); }
    /// @brief Set the envelope fill color.
    void setFillColor(const Color& c)   { fillColor_ = c; markDirty(); }
    /// @brief Set the background color.
    void setBgColor(const Color& c)     { bgColor_ = c; markDirty(); }
    /// @brief Set the drag handle color.
    void setHandleColor(const Color& c) { handleColor_ = c; markDirty(); }
    /// @brief Set whether to show ADSR labels.
    void setShowLabels(bool s)          { showLabels_ = s; markDirty(); }

    /// @brief Emitted when any envelope parameter changes.
    Signal<float, float, float, float> onChanged;

    Vec2f sizeHint() const override { return {260, 120}; }
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;

private:
    struct Points { float x0,y0, x1,y1, x2,y2, x3,y3, x4,y4; };
    Points computePoints(const Rect& area) const;
    int    hitHandle(float mx, float my) const;

    float a_ = 0.05f, d_ = 0.10f, s_ = 0.70f, r_ = 0.20f;
    Color lineColor_   = Color(100, 200, 240, 255);
    Color fillColor_   = Color(100, 200, 240, 50);
    Color bgColor_     = Color( 20,  24,  30, 255);
    Color handleColor_ = Color(255, 255, 255, 220);
    bool  showLabels_  = true;
    int   dragging_ = -1;
    static constexpr float kHandleR = 5.0f;
};

// ═════════════════════════════════════════════════════════════════════════════
//  VUMeter — Multi-channel volume meter with peak hold
// ═════════════════════════════════════════════════════════════════════════════

class VUMeter : public Widget
{
public:
    enum class Orientation { Vertical, Horizontal };

    VUMeter();

    /// @brief Set the number of meter channels.
    void  setChannelCount(int n);
    /// @brief Get the number of channels.
    int   channelCount() const { return static_cast<int>(levels_.size()); }
    /// @brief Set the level for a channel (0..1).
    void  setLevel(int ch, float level);
    /// @brief Get the level for a channel.
    float level(int ch) const;
    /// @brief Update peak hold and decay animations.
    void  update(float dt);

    /// @brief Set vertical or horizontal orientation.
    void setOrientation(Orientation o) { orient_ = o; markDirty(); }
    /// @brief Get the current orientation.
    Orientation orientation() const    { return orient_; }
    /// @brief Set whether to show the dB scale.
    void setShowScale(bool s)          { showScale_ = s; markDirty(); }
    /// @brief Enable or disable peak hold indicators.
    void setPeakHold(bool p)           { peakHold_ = p; markDirty(); }
    /// @brief Set the peak decay rate.
    void setPeakDecayRate(float r)     { peakDecay_ = r; }
    /// @brief Set the spacing between channel bars.
    void setBarSpacing(int s)          { barSpacing_ = s; markDirty(); }

    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;

private:
    struct Channel { float level = 0.f, peak = 0.f, peakTimer = 0.f; };
    std::vector<Channel> levels_;
    Orientation orient_ = Orientation::Vertical;
    bool  showScale_  = true;
    bool  peakHold_   = true;
    float peakDecay_  = 0.5f;
    int   barSpacing_ = 4;

    void paintChannel(PaintContext& ctx, const Rect& bar, const Channel& ch) const;
};

// ═════════════════════════════════════════════════════════════════════════════
//  SpectrumAnalyzer — Frequency spectrum bar display
// ═════════════════════════════════════════════════════════════════════════════

class SpectrumAnalyzer : public Widget
{
public:
    SpectrumAnalyzer();

    /// @brief Set the number of frequency bins.
    void setBinCount(int n);
    /// @brief Get the number of frequency bins.
    int  binCount() const { return static_cast<int>(bins_.size()); }
    /// @brief Set bin magnitudes from raw data.
    void setMagnitudes(const float* magnitudes, int count);
    /// @brief Update peak hold and smoothing.
    void update(float dt);

    /// @brief Set custom frequency labels for the x-axis.
    void setFreqLabels(const float* freqs, int count);
    /// @brief Set the sample rate for frequency display.
    void setSampleRate(float sr) { sampleRate_ = sr; markDirty(); }

    /// @brief Enable logarithmic frequency scale.
    void setLogFrequency(bool l) { logFreq_    = l; markDirty(); }
    /// @brief Set whether to show frequency labels.
    void setShowLabels(bool s)   { showLabels_ = s; markDirty(); }
    /// @brief Enable or disable peak hold.
    void setPeakHold(bool p)     { peakHold_   = p; markDirty(); }
    /// @brief Set the peak decay rate.
    void setPeakDecay(float r)   { peakDecay_  = r; }
    /// @brief Set the magnitude smoothing factor.
    void setSmoothing(float s)   { smoothing_  = s; }
    /// @brief Set the low and high gradient bar colors.
    void setBarColor(const Color& lo, const Color& hi) { colorLow_ = lo; colorHigh_ = hi; markDirty(); }
    /// @brief Set the background color.
    void setBgColor(const Color& c) { bgColor_ = c; markDirty(); }
    /// @brief Set the spacing between bars.
    void setBarSpacing(int s)       { barSpacing_ = s; markDirty(); }

    Vec2f sizeHint() const override { return {300, 140}; }
    void  paint(PaintContext& ctx) override;

private:
    struct Bin { float value = 0.f, peak = 0.f, peakTimer = 0.f; };
    std::vector<Bin>   bins_;
    std::vector<float> freqLabels_;
    float sampleRate_ = 44100.f;
    bool  logFreq_    = false;
    bool  showLabels_ = true;
    bool  peakHold_   = true;
    float peakDecay_  = 1.2f;
    float smoothing_  = 0.7f;
    int   barSpacing_ = 2;
    Color colorLow_   = Color( 80, 200, 100, 255);
    Color colorHigh_  = Color(220,  60,  50, 255);
    Color bgColor_    = Color( 18,  20,  26, 255);
};

// ═════════════════════════════════════════════════════════════════════════════
//  WaveformView — Audio waveform display with zoom/pan/scrub
// ═════════════════════════════════════════════════════════════════════════════

class WaveformView : public Widget
{
public:
    WaveformView();

    /// @brief Load audio sample data for display.
    void setSamples(const float* data, int count, int channels = 1, float sampleRate = 44100.f);
    /// @brief Clear all loaded sample data.
    void clearSamples();
    /// @brief Get the total number of samples.
    int   sampleCount() const { return sampleCount_; }
    /// @brief Get the number of audio channels.
    int   channels()    const { return channels_; }
    /// @brief Get the sample rate in Hz.
    float sampleRate()  const { return sampleRate_; }

    /// @brief Set the playhead position (0..1).
    void  setPlayhead(float t) { playhead_ = t; markDirty(); }
    /// @brief Get the current playhead position.
    float playhead() const     { return playhead_; }

    /// @brief Set the loop region boundaries (0..1).
    void  setLoopRegion(float start, float end);
    /// @brief Clear the loop region.
    void  clearLoop() { hasLoop_ = false; markDirty(); }

    /// @brief Set the visible time range (0..1).
    void  setViewRange(float start, float end);
    /// @brief Get the view range start.
    float viewStart() const { return viewStart_; }
    /// @brief Get the view range end.
    float viewEnd()   const { return viewEnd_; }
    /// @brief Reset view to show the full waveform.
    void  resetView() { viewStart_ = 0.f; viewEnd_ = 1.f; markDirty(); }

    /// @brief Set the waveform color.
    void setWaveColor(const Color& c)    { waveColor_ = c; markDirty(); }
    /// @brief Set the background color.
    void setBgColor(const Color& c)      { bgColor_ = c; markDirty(); }
    /// @brief Set the playhead line color.
    void setPlayheadColor(const Color& c){ playheadColor_ = c; markDirty(); }
    /// @brief Set whether to show the centre line.
    void setShowCentreLine(bool s)       { showCentre_ = s; markDirty(); }

    /// @brief Emitted when the user scrubs the playhead.
    Signal<float> onScrub;

    Vec2f sizeHint() const override { return {300, 100}; }
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onMouseMove(MouseEvent& e) override;
    void  onMouseRelease(MouseEvent& e) override;
    void  onMouseScroll(MouseEvent& e) override;

private:
    struct EnvPair { float mn, mx; };
    std::vector<EnvPair> buildEnvelope(int ch, float vs, float ve, int pw) const;

    std::vector<float> samples_;
    int   sampleCount_ = 0, channels_ = 1;
    float sampleRate_ = 44100.f;
    float playhead_ = 0.f;
    bool  hasLoop_ = false;
    float loopStart_ = 0.f, loopEnd_ = 1.f;
    float viewStart_ = 0.f, viewEnd_ = 1.f;
    Color waveColor_     = Color(100, 200, 120, 255);
    Color bgColor_       = Color( 20,  24,  28, 255);
    Color playheadColor_ = Color(255, 200,  80, 255);
    bool  showCentre_ = true;
    bool  dragging_ = false, panning_ = false;
    float panStartX_ = 0.f, panVS_ = 0.f, panVE_ = 0.f;
};

// ═════════════════════════════════════════════════════════════════════════════
//  PianoRoll — Piano keyboard + note grid editor
// ═════════════════════════════════════════════════════════════════════════════

struct PianoNote {
    int   pitch;
    float start;
    float length;
};

class PianoRoll : public Widget
{
public:
    PianoRoll();

    /// @brief Add a note to the roll.
    void addNote(int pitch, float start, float length);
    /// @brief Remove a note by index.
    void removeNote(int idx);
    /// @brief Remove all notes.
    void clearNotes();
    /// @brief Get the list of notes.
    const std::vector<PianoNote>& notes() const { return notes_; }

    /// @brief Set the visible pitch range (MIDI note numbers).
    void setPitchRange(int lo, int hi);
    /// @brief Set the visible time range in beats.
    void setTimeRange(float startBeat, float endBeat);
    /// @brief Set beats per bar for grid lines.
    void setBeatsPerBar(int bpb) { bpb_ = bpb; markDirty(); }
    /// @brief Get the lowest visible pitch.
    int  pitchLo() const { return pitchLo_; }
    /// @brief Get the highest visible pitch.
    int  pitchHi() const { return pitchHi_; }

    /// @brief Set the playhead position in beats.
    void  setPlayhead(float beat) { playhead_ = beat; markDirty(); }
    /// @brief Get the current playhead position.
    float playhead() const        { return playhead_; }

    /// @brief Set the piano key column width in pixels.
    void setKeyWidth(float w)         { keyW_ = w; markDirty(); }
    /// @brief Set the note row height in pixels.
    void setRowHeight(float h)        { rowH_ = h; markDirty(); }
    /// @brief Set the note rectangle color.
    void setNoteColor(const Color& c) { noteColor_ = c; markDirty(); }
    /// @brief Set whether to show octave labels on keys.
    void setShowOctaveLabels(bool s)  { showOctave_ = s; markDirty(); }

    /// @brief Emitted when a note is added (pitch, start, length).
    Signal<int, float, float> onNoteAdded;
    /// @brief Emitted when a note is removed.
    Signal<int>               onNoteRemoved;
    /// @brief Emitted when a note is clicked.
    Signal<int>               onNoteClicked;
    /// @brief Emitted when the user scrubs the playhead.
    Signal<float>             onScrub;

    Vec2f sizeHint() const override { return {400, 200}; }
    void  layout() override;
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onMouseMove(MouseEvent& e) override;
    void  onMouseRelease(MouseEvent& e) override;
    void  onMouseScroll(MouseEvent& e) override;

private:
    Rect  gridArea() const;
    Rect  keyArea() const;
    float pitchToY(int pitch) const;
    int   yToPitch(float y) const;
    float beatToX(float beat) const;
    float xToBeat(float x) const;
    Rect  noteRect(const PianoNote& n) const;
    bool  isBlackKey(int pitch) const;
    void  paintKeys(PaintContext& ctx, const Rect& ka) const;
    void  paintGrid(PaintContext& ctx, const Rect& ga) const;
    void  paintNotes(PaintContext& ctx, const Rect& ga) const;
    void  paintPlayhead(PaintContext& ctx, const Rect& ga) const;

    std::vector<PianoNote> notes_;
    int   pitchLo_ = 48, pitchHi_ = 72;
    int   bpb_ = 4;
    float viewStart_ = 0.f, viewEnd_ = 8.f;
    float playhead_ = 0.f;
    float keyW_ = 36.f, rowH_ = 12.f;
    Color noteColor_    = Color(100, 160, 240, 220);
    Color noteSelColor_ = Color(240, 200,  60, 255);
    bool  showOctave_ = true;
    int   draggingNote_ = -1;
    float dragOffBeat_ = 0.f;
    int   dragOffPitch_ = 0;
    bool  panning_ = false;
    float panStartX_ = 0.f, panVS_ = 0.f, panVE_ = 0.f;
    int   newNotePitch_ = -1;
    float newNoteStart_ = 0.f;
};

} // namespace BuGUI
