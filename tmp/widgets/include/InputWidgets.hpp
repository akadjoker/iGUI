#pragma once

#include "Widget.hpp"
#include "Theme.hpp"
#include <string>

namespace BuGUI
{
// ═════════════════════════════════════════════════════════════════════════════
//  Slider — draggable value control (horizontal or vertical)
// ═════════════════════════════════════════════════════════════════════════════

class Slider : public Widget
{
public:
    Slider(float minVal = 0.0f, float maxVal = 1.0f, float value = 0.5f);

    /// @brief Get the current value.
    float value()    const { return value_; }
    /// @brief Get the minimum value.
    float minValue() const { return min_; }
    /// @brief Get the maximum value.
    float maxValue() const { return max_; }
    /// @brief Set the current value (clamped to range).
    void  setValue(float v);
    /// @brief Set the min/max range.
    void  setRange(float minVal, float maxVal);

    /// @brief Set horizontal or vertical orientation.
    void      setOrientation(LayoutDir dir) { orientation_ = dir; markDirty(); }
    /// @brief Get the orientation.
    LayoutDir orientation() const           { return orientation_; }

    /// @brief Emitted when the value changes.
    Signal<float> onValueChanged;

    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onMouseRelease(MouseEvent& e) override;
    void  onMouseMove(MouseEvent& e) override;

private:
    float     min_, max_, value_;
    LayoutDir orientation_ = LayoutDir::Horizontal;
    bool      dragging_    = false;

    void updateFromMouse(float localX, float localY);
};

// ═════════════════════════════════════════════════════════════════════════════
//  ProgressBar — non-interactive progress display with optional text label
// ═════════════════════════════════════════════════════════════════════════════

class ProgressBar : public Widget
{
public:
    ProgressBar(float minVal = 0.0f, float maxVal = 1.0f, float value = 0.0f);

    /// @brief Get the current progress value.
    float value()    const { return value_; }
    /// @brief Get the minimum value.
    float minValue() const { return min_; }
    /// @brief Get the maximum value.
    float maxValue() const { return max_; }
    /// @brief Set the progress value.
    void  setValue(float v);
    /// @brief Set the min/max range.
    void  setRange(float minVal, float maxVal);

    /// @brief Set horizontal or vertical orientation.
    void      setOrientation(LayoutDir dir) { orientation_ = dir; markDirty(); }
    /// @brief Get the orientation.
    LayoutDir orientation() const           { return orientation_; }

    /// @brief Set the overlay text.
    void               setText(const std::string& t) { text_ = t; markDirty(); }
    /// @brief Get the overlay text.
    const std::string& text() const                  { return text_; }

    /// @brief Set text alignment within the bar.
    void      setTextAlign(TextAlign a) { textAlign_ = a; markDirty(); }
    /// @brief Get the text alignment.
    TextAlign textAlign() const         { return textAlign_; }

    /// @brief Set the filled bar color.
    void         setBarColor(const Color& c)  { barColor_ = c; markDirty(); }
    /// @brief Get the filled bar color.
    const Color& barColor() const             { return barColor_; }
    /// @brief Set the text color.
    void         setTextColor(const Color& c) { textColor_ = c; markDirty(); }
    /// @brief Get the text color.
    const Color& textColor() const            { return textColor_; }

    Signal<float> onValueChanged;

    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;

private:
    float     min_, max_, value_;
    LayoutDir orientation_ = LayoutDir::Horizontal;
    std::string text_;
    TextAlign   textAlign_ = TextAlign::CENTER;
    Color barColor_  = Color(140, 140, 145, 255);
    Color textColor_ = Color(220, 220, 220, 255);
};

// ═════════════════════════════════════════════════════════════════════════════
//  SpinBox — numeric input with +/- buttons and optional drag-to-adjust
//    auto* spin = parent->createChild<SpinBox>(0.0f, 100.0f, 50.0f);
//    spin->onValueChanged.connect([](float v){ ... });
// ═════════════════════════════════════════════════════════════════════════════

class SpinBox : public Widget
{
public:
    SpinBox(float minVal = 0.0f, float maxVal = 100.0f, float value = 0.0f, float step = 1.0f);

    /// @brief Get the current value.
    float value()    const { return value_; }
    /// @brief Get the minimum value.
    float minValue() const { return min_; }
    /// @brief Get the maximum value.
    float maxValue() const { return max_; }
    /// @brief Get the step increment.
    float step()     const { return step_; }
    /// @brief Set the current value.
    void  setValue(float v);
    /// @brief Set the min/max range.
    void  setRange(float minVal, float maxVal);
    /// @brief Set the step increment.
    void  setStep(float s) { step_ = s; }

    /// @brief Set number of decimal places shown.
    void setDecimals(int d) { decimals_ = d; markDirty(); }
    /// @brief Get the decimal precision.
    int  decimals() const   { return decimals_; }

    /// @brief Set a suffix string appended to the value.
    void setSuffix(const std::string& s)  { suffix_ = s; markDirty(); }
    /// @brief Get the suffix.
    const std::string& suffix() const     { return suffix_; }

    /// @brief Set a prefix string prepended to the value.
    void setPrefix(const std::string& p)  { prefix_ = p; markDirty(); }
    /// @brief Get the prefix.
    const std::string& prefix() const     { return prefix_; }

    /// @brief Set drag sensitivity for mouse-drag value adjustment.
    void  setDragSensitivity(float s) { dragSens_ = s; }
    /// @brief Get the drag sensitivity.
    float dragSensitivity() const     { return dragSens_; }

    Signal<float> onValueChanged;

    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onMouseRelease(MouseEvent& e) override;
    void  onMouseMove(MouseEvent& e) override;
    void  onMouseScroll(MouseEvent& e) override;
    void  onKeyPress(KeyEvent& e) override;

private:
    float min_, max_, value_, step_;
    int   decimals_ = 1;
    std::string suffix_;
    std::string prefix_;

    bool  dragging_   = false;
    float dragStartY_ = 0;
    float dragStartV_ = 0;
    float dragSens_   = 0.1f;

    enum HitZone { None, Minus, Plus, Value };
    HitZone hitZone(float localX) const;
    std::string formatValue() const;
    float buttonWidth() const;
};

// ═════════════════════════════════════════════════════════════════════════════
//  DatePicker — click to open a calendar popup
//    auto* dp = parent->createChild<DatePicker>();
//    dp->onDateChanged.connect([](int y, int m, int d){ ... });
// ═════════════════════════════════════════════════════════════════════════════

struct Date {
    int year  = 2026;
    int month = 1;    // 1–12
    int day   = 1;    // 1–31
    bool operator==(const Date& o) const { return year == o.year && month == o.month && day == o.day; }
    bool operator!=(const Date& o) const { return !(*this == o); }
};

class DatePicker : public Widget
{
public:
    DatePicker();

    /// @brief Get the selected date.
    const Date& date() const { return date_; }
    /// @brief Set the date.
    void setDate(const Date& d);
    /// @brief Set the date by year, month, day.
    void setDate(int year, int month, int day);

    /// @brief Emitted when the date changes (year, month, day).
    Signal<int, int, int> onDateChanged;

    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;

    /// @brief Open the calendar popup.
    void openCalendar();
    /// @brief Close the calendar popup.
    void closeCalendar();
    /// @brief Check if the calendar is open.
    bool isOpen() const { return open_; }

private:
    Date  date_;
    bool  open_ = false;

    std::string formatDate() const;
};

// ═════════════════════════════════════════════════════════════════════════════
//  TimePicker — hour : minute : second with spin buttons and scroll
//    auto* tp = parent->createChild<TimePicker>();
//    tp->onTimeChanged.connect([](int h, int m, int s){ ... });
// ═════════════════════════════════════════════════════════════════════════════

struct Time {
    int hour   = 0;  // 0–23
    int minute = 0;  // 0–59
    int second = 0;  // 0–59
    bool operator==(const Time& o) const { return hour == o.hour && minute == o.minute && second == o.second; }
    bool operator!=(const Time& o) const { return !(*this == o); }
};

class TimePicker : public Widget
{
public:
    TimePicker();

    /// @brief Get the current time.
    const Time& time() const       { return time_; }
    /// @brief Set the time.
    void setTime(const Time& t);
    /// @brief Set the time by hours, minutes, seconds.
    void setTime(int h, int m, int s = 0);

    /// @brief Show or hide the seconds field.
    void setShowSeconds(bool s)    { showSeconds_ = s; markDirty(); }
    /// @brief Check if seconds are shown.
    bool showSeconds() const       { return showSeconds_; }

    /// @brief Emitted when the time changes (h, m, s).
    Signal<int, int, int> onTimeChanged;

    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;
    void  onMousePress(MouseEvent& e) override;
    void  onMouseScroll(MouseEvent& e) override;
    void  onKeyPress(KeyEvent& e) override;

private:
    Time  time_;
    bool  showSeconds_ = true;

    enum Field { Hour, Minute, Second };
    Field  activeField_ = Hour;

    // Layout helpers
    float fieldWidth() const;
    float separatorWidth() const;
    Field hitField(float localX) const;
    void  adjustField(Field f, int delta);
    std::string formatTime() const;
};

} // namespace BuGUI
