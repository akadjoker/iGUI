#pragma once

#include "Widget.hpp"
#include "Theme.hpp"
#include <string>
#include <vector>
#include <cmath>

// ═════════════════════════════════════════════════════════════════════════════
//  Automotive Widget Collection
//  BMW/Tesla-style instrument cluster widgets
// ═════════════════════════════════════════════════════════════════════════════

// ─────────────────────────────────────────────────────────────────────────────
//  RadialGauge — arc-style gauge (speedometer, RPM, fuel, temp)
//    Displays a value on a circular arc with optional tick marks, labels,
//    and a large center readout
// ─────────────────────────────────────────────────────────────────────────────

namespace BuGUI
{

class RadialGauge : public Widget
{
public:
    RadialGauge(float minVal = 0.0f, float maxVal = 260.0f, float value = 0.0f);

    /// @brief Get the current value.
    float value()    const { return value_; }
    /// @brief Get the minimum value.
    float minValue() const { return min_; }
    /// @brief Get the maximum value.
    float maxValue() const { return max_; }
    /// @brief Set the gauge value.
    void  setValue(float v);
    /// @brief Set the min/max range.
    void  setRange(float minVal, float maxVal);

    /// @brief Set the arc start angle in degrees.
    void  setStartAngle(float deg) { startAngle_ = deg; markDirty(); }
    /// @brief Set the arc sweep angle in degrees.
    void  setSweepAngle(float deg) { sweepAngle_ = deg; markDirty(); }

    /// @brief Set the unit label (e.g. "km/h").
    void setUnit(const std::string& u)  { unit_ = u; markDirty(); }
    /// @brief Set the display label text.
    void setLabel(const std::string& l)  { label_ = l; markDirty(); }
    /// @brief Set whether to show tick marks.
    void setShowTicks(bool s)            { showTicks_ = s; markDirty(); }
    /// @brief Set the interval between tick marks.
    void setTickInterval(float i)        { tickInterval_ = i; markDirty(); }
    /// @brief Set the number of decimal places for the readout.
    void setValueDecimals(int d)         { decimals_ = d; markDirty(); }

    /// @brief Set the active arc color.
    void setArcColor(const Color& c)     { arcColor_ = c; markDirty(); }
    /// @brief Set the arc background color.
    void setArcBgColor(const Color& c)   { arcBgColor_ = c; markDirty(); }
    /// @brief Set the glow effect color.
    void setGlowColor(const Color& c)    { glowColor_ = c; markDirty(); }
    /// @brief Set the center readout text color.
    void setValueColor(const Color& c)   { valueColor_ = c; markDirty(); }
    /// @brief Enable or disable the glow effect.
    void setShowGlow(bool s)             { showGlow_ = s; markDirty(); }

    /// @brief Set the arc thickness in pixels.
    void setArcWidth(float w) { arcWidth_ = w; markDirty(); }

    /// @brief Set whether to show the needle.
    void setShowNeedle(bool s)           { showNeedle_ = s; markDirty(); }
    /// @brief Set the needle color.
    void setNeedleColor(const Color& c)  { needleColor_ = c; markDirty(); }
    /// @brief Set the needle width in pixels.
    void setNeedleWidth(float w)         { needleWidth_ = w; markDirty(); }
    /// @brief Set needle length as fraction of radius (0..1).
    void setNeedleLength(float l)        { needleLen_ = l; markDirty(); }  // 0..1 fraction of radius

    /// @brief Enable filled arc mode (triangle fans).
    void setFilledArc(bool s)            { filledArc_ = s; markDirty(); }

    /// @brief Emitted when the value changes.
    Signal<float> onValueChanged;

    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;

private:
    float min_, max_, value_;
    float startAngle_ = 135.0f;  // degrees (0=right, 90=down)
    float sweepAngle_ = 270.0f;
    float arcWidth_   = 6.0f;

    std::string unit_;
    std::string label_;
    bool  showTicks_    = true;
    float tickInterval_ = 20.0f;
    int   decimals_     = 0;

    Color arcColor_   = Color(200, 160, 40, 255);   // amber
    Color arcBgColor_ = Color(60, 60, 65, 255);
    Color glowColor_  = Color(200, 160, 40, 80);
    Color valueColor_ = Color(240, 240, 240, 255);
    bool  showGlow_   = true;

    // Needle
    bool  showNeedle_  = false;
    Color needleColor_ = Color(220, 40, 40, 255);  // red
    float needleWidth_ = 3.0f;
    float needleLen_   = 0.85f;  // fraction of radius

    // Filled arc
    bool  filledArc_   = false;

    void drawArc(PaintContext& ctx, float cx, float cy, float r, float startRad, float endRad, int segs);
    void drawThickArc(PaintContext& ctx, float cx, float cy, float r, float width, float startRad, float endRad, int segs);
};

// ─────────────────────────────────────────────────────────────────────────────
//  PowerBar — horizontal center-zero bar gauge
//    Displays power/regen (e.g. -100 kW to +200 kW) with center at zero
// ─────────────────────────────────────────────────────────────────────────────

class PowerBar : public Widget
{
public:
    PowerBar(float minVal = -100.0f, float maxVal = 200.0f, float value = 0.0f);

    /// @brief Get the current value.
    float value()    const { return value_; }
    /// @brief Get the minimum value.
    float minValue() const { return min_; }
    /// @brief Get the maximum value.
    float maxValue() const { return max_; }
    /// @brief Set the bar value.
    void  setValue(float v);
    /// @brief Set the min/max range.
    void  setRange(float minVal, float maxVal);

    /// @brief Set the unit label (e.g. "kW").
    void setUnit(const std::string& u)   { unit_ = u; markDirty(); }
    /// @brief Set whether to show tick marks.
    void setShowTicks(bool s)            { showTicks_ = s; markDirty(); }
    /// @brief Set the number of tick marks.
    void setTickCount(int n)             { tickCount_ = n; markDirty(); }

    /// @brief Set the positive-value bar color.
    void setPositiveColor(const Color& c) { posColor_ = c; markDirty(); }
    /// @brief Set the negative-value (regen) bar color.
    void setNegativeColor(const Color& c) { negColor_ = c; markDirty(); }
    /// @brief Set the bar background color.
    void setBarBgColor(const Color& c)    { barBgColor_ = c; markDirty(); }

    /// @brief Emitted when the value changes.
    Signal<float> onValueChanged;

    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;

private:
    float min_, max_, value_;
    std::string unit_ = "kW";
    bool  showTicks_ = true;
    int   tickCount_ = 15;

    Color posColor_  = Color(200, 160, 40, 255);
    Color negColor_  = Color(60, 180, 80, 255);
    Color barBgColor_ = Color(50, 50, 55, 255);
};

// ─────────────────────────────────────────────────────────────────────────────
//  InfoTile — icon + value + label tile for dashboard status bars
//    e.g.  🔋 100%   📍 200 km   🌡 24°
// ─────────────────────────────────────────────────────────────────────────────

class InfoTile : public Widget
{
public:
    InfoTile(const std::string& icon = "", const std::string& value = "",
             const std::string& suffix = "");

    /// @brief Set the icon text (e.g. emoji or symbol).
    void setIcon(const std::string& i)   { icon_ = i; markDirty(); }
    /// @brief Set the displayed value text.
    void setValue(const std::string& v)   { value_ = v; markDirty(); }
    /// @brief Set the suffix text (e.g. "km", "%").
    void setSuffix(const std::string& s)  { suffix_ = s; markDirty(); }

    /// @brief Set the icon color.
    void setIconColor(const Color& c)     { iconColor_ = c; markDirty(); }
    /// @brief Set the value text color.
    void setValueColor(const Color& c)    { valueColor_ = c; markDirty(); }

    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;

private:
    std::string icon_;
    std::string value_;
    std::string suffix_;
    Color iconColor_  = Color(140, 140, 140, 255);
    Color valueColor_ = Color(220, 220, 220, 255);
};

// ─────────────────────────────────────────────────────────────────────────────
//  DriveMode — mode selector (NORMAL / SPORT / ECO / COMFORT)
// ─────────────────────────────────────────────────────────────────────────────

class DriveMode : public Widget
{
public:
    DriveMode();

    /// @brief Add a drive mode and return its index.
    int  addMode(const std::string& name, const Color& color);
    /// @brief Set the active mode by index.
    void setMode(int index);
    /// @brief Get the current mode index.
    int  mode() const { return current_; }
    /// @brief Get the current mode name.
    const std::string& modeName() const;

    /// @brief Emitted when the active mode changes.
    Signal<int> onModeChanged;

    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onMouseScroll(MouseEvent& e) override;
    void  onKeyPress(KeyEvent& e) override;

private:
    struct ModeEntry { std::string name; Color color; };
    std::vector<ModeEntry> modes_;
    int current_ = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
//  DigitalSpeed — big numeric speed display with unit
//    Like the BMW cluster center: large "56" + "km/h" above
// ─────────────────────────────────────────────────────────────────────────────

class DigitalSpeed : public Widget
{
public:
    DigitalSpeed(float value = 0.0f);

    /// @brief Get the current speed value.
    float value() const { return value_; }
    /// @brief Set the speed value.
    void  setValue(float v);

    /// @brief Set the unit label (e.g. "km/h").
    void setUnit(const std::string& u)   { unit_ = u; markDirty(); }
    /// @brief Set the value text color.
    void setValueColor(const Color& c)   { valueColor_ = c; markDirty(); }
    /// @brief Set the unit text color.
    void setUnitColor(const Color& c)    { unitColor_ = c; markDirty(); }

    /// @brief Set the font scale multiplier for the big number.
    void setScale(float s)               { scale_ = s; markDirty(); }

    /// @brief Emitted when the value changes.
    Signal<float> onValueChanged;

    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;

private:
    float value_;
    float scale_ = 4.0f;
    std::string unit_ = "km/h";
    Color valueColor_ = Color(240, 240, 240, 255);
    Color unitColor_  = Color(160, 160, 165, 255);
};

// ─────────────────────────────────────────────────────────────────────────────
//  BatteryGauge — horizontal battery bar with percentage
// ─────────────────────────────────────────────────────────────────────────────

class BatteryGauge : public Widget
{
public:
    BatteryGauge(float percent = 100.0f);

    /// @brief Get the current charge percentage.
    float percent() const { return percent_; }
    /// @brief Set the charge percentage (0..100).
    void  setPercent(float p);

    /// @brief Set whether the battery is currently charging.
    void setChargingState(bool charging) { charging_ = charging; markDirty(); }
    /// @brief Get whether the battery is charging.
    bool isCharging() const              { return charging_; }

    /// @brief Emitted when the percentage changes.
    Signal<float> onPercentChanged;

    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;

private:
    float percent_;
    bool  charging_ = false;
};

} // namespace BuGUI
