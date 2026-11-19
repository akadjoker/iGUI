#pragma once

#include "Theme.hpp"
#include "Widget.hpp"
#include <igui/widgets/String.hpp>

namespace ig
{
namespace retained
{
// ═════════════════════════════════════════════════════════════════════════════
//  Slider — draggable value control (horizontal or vertical)
// ═════════════════════════════════════════════════════════════════════════════

class Slider : public Widget
{
public:
    IG_WIDGET_CLASS(Slider, Widget);

    Slider(float minVal = 0.0f, float maxVal = 1.0f, float value = 0.5f);

    /// @brief Get the current value.
    float value() const
    {
        return value_;
    }
    /// @brief Get the minimum value.
    float minValue() const
    {
        return min_;
    }
    /// @brief Get the maximum value.
    float maxValue() const
    {
        return max_;
    }
    /// @brief Set the current value (clamped to range).
    void setValue(float v);
    /// @brief Set the min/max range.
    void setRange(float minVal, float maxVal);

    /// @brief Set horizontal or vertical orientation.
    void setOrientation(LayoutDir dir)
    {
        orientation_ = dir;
        markDirty();
    }
    /// @brief Get the orientation.
    LayoutDir orientation() const
    {
        return orientation_;
    }

    /// @brief Show the value as text at the end of the slider: to the right of a horizontal one, under a
    ///        vertical one (the track is shorter by the room the text takes, which is that of the widest of
    ///        the texts of the minimum and the maximum, so the track does not move as the value changes,
    ///        and sizeHint() asks for the room). A value that changes every frame (a meter, a slider being
    ///        dragged) asks for no layout pass: the room does not depend on it. What does change the room (the
    ///        decimals, the suffix, the range, showing it) asks for a pass only if sizeHint() really is
    ///        another. A slider too short to keep a track of 24 pixels or so
    ///        beside the text shows no text. Default false.
    void setShowValue(bool on)
    {
        if (showValue_ == on) return;
        showValue_ = on;
        invalidateLayoutIfHintChanged(hint_); // (the hint only grows by the room of the text: one pass, once)
        markDirty();
    }
    bool showValue() const
    {
        return showValue_;
    }
    /// @brief Decimals of the text of the value (0 to 6); -1, the default, picks them from the range: none
    ///        from a range of 100, one from 10, two below.
    void setValueDecimals(int decimals)
    {
        decimals = decimals < -1 ? -1 : (decimals > 6 ? 6 : decimals);
        if (valueDecimals_ == decimals) return;
        valueDecimals_ = decimals;
        if (showValue_) invalidateLayoutIfHintChanged(hint_);
        markDirty();
    }
    int valueDecimals() const
    {
        return valueDecimals_;
    }
    /// @brief Text after the value, e.g. " dB" or "%".
    void setValueSuffix(const String& suffix)
    {
        if (valueSuffix_ == suffix) return;
        valueSuffix_ = suffix;
        if (showValue_) invalidateLayoutIfHintChanged(hint_);
        markDirty();
    }
    const String& valueSuffix() const
    {
        return valueSuffix_;
    }
    /// @brief The text of a value as the readout writes it (a value that rounds to zero is "0", not "-0").
    String valueText(float v) const;
    /// @brief The text of the current value.
    String valueText() const
    {
        return valueText(value_);
    }

    /// @brief Emitted when the value changes. The handler may take the slider out of its parent (removeChild): it touches
    ///        nothing of itself after that (see WidgetApp::hasGone).
    Signal<float> onValueChanged;

    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;

private:
    float min_, max_, value_;
    LayoutDir orientation_ = LayoutDir::Horizontal;
    bool dragging_ = false;
    bool showValue_ = false;
    int valueDecimals_ = -1;
    String valueSuffix_;
    mutable HintMemo hint_; // what sizeHint() was last time: the setters of the readout ask for a layout only when it changes

    // The room the readout takes at the end of the slider, along it (0 when it is not shown, or when the slider is
    // too short for it).
    float readoutExtent() const;
    void paintReadout(PaintContext& ctx, const Rect& full, float extent);

    // The thumb's centre travels this far in from each end, so the whole thumb
    // and a small gap stay inside the slider (0 when the slider is too short).
    // @p extent is the room the readout takes from the length (readoutExtent()).
    float thumbInset(float extent = 0.0f) const;
    // Radius of the round thumb: the theme's sliderThumbRadius, but never more than half of the
    // slider's thickness, so the thumb stays inside the slider's rectangle.
    float thumbRadius() const;
    void updateFromMouse(float localX, float localY);
};

// ═════════════════════════════════════════════════════════════════════════════
//  ProgressBar — non-interactive progress display with optional text label
// ═════════════════════════════════════════════════════════════════════════════

class ProgressBar : public Widget
{
public:
    IG_WIDGET_CLASS(ProgressBar, Widget);

    ProgressBar(float minVal = 0.0f, float maxVal = 1.0f, float value = 0.0f);

    /// @brief Get the current progress value.
    float value() const
    {
        return value_;
    }
    /// @brief Get the minimum value.
    float minValue() const
    {
        return min_;
    }
    /// @brief Get the maximum value.
    float maxValue() const
    {
        return max_;
    }
    /// @brief Set the progress value.
    void setValue(float v);
    /// @brief Set the min/max range.
    void setRange(float minVal, float maxVal);

    /// @brief Set horizontal or vertical orientation.
    void setOrientation(LayoutDir dir)
    {
        orientation_ = dir;
        markDirty();
    }
    /// @brief Get the orientation.
    LayoutDir orientation() const
    {
        return orientation_;
    }

    /// @brief Set the overlay text.
    void setText(const String& t)
    {
        text_ = t;
        markDirty();
    }
    /// @brief Get the overlay text.
    const String& text() const
    {
        return text_;
    }

    /// @brief Set text alignment within the bar.
    void setTextAlign(TextAlign a)
    {
        textAlign_ = a;
        markDirty();
    }
    /// @brief Get the text alignment.
    TextAlign textAlign() const
    {
        return textAlign_;
    }

    /// @brief Set the filled bar color.
    void setBarColor(const Color& c)
    {
        barColor_ = c;
        markDirty();
    }
    /// @brief Get the filled bar color.
    Color barColor() const
    {
        return barColor_.a > 0 ? barColor_ : Theme::instance().progressFill;
    }
    /// @brief Set the text color.
    void setTextColor(const Color& c)
    {
        textColor_ = c;
        markDirty();
    }
    /// @brief Get the text color.
    const Color& textColor() const
    {
        return textColor_.a > 0 ? textColor_ : Theme::instance().app.progressText;
    }

    Signal<float> onValueChanged;

    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;

private:
    float min_, max_, value_;
    LayoutDir orientation_ = LayoutDir::Horizontal;
    String text_;
    TextAlign textAlign_ = TextAlign::CENTER;
    Color barColor_ = Color(0, 0, 0, 0); // alpha 0: follow the theme's progressFill
    Color textColor_ = Color(0, 0, 0, 0); // alpha 0: follow the theme's app.progressText
    ElideMemo textMemo_; // what paint worked out for a text wider than the bar (cut to its room)
};

// ═════════════════════════════════════════════════════════════════════════════
//  SpinBox — numeric input with +/- buttons and optional drag-to-adjust
//    auto* spin = parent->createChild<SpinBox>(0.0f, 100.0f, 50.0f);
//    spin->onValueChanged.connect([](float v){ ... });
// ═════════════════════════════════════════════════════════════════════════════

class SpinBox : public Widget
{
public:
    IG_WIDGET_CLASS(SpinBox, Widget);

    SpinBox(float minVal = 0.0f, float maxVal = 100.0f, float value = 0.0f, float step = 1.0f);

    /// @brief Get the current value.
    float value() const
    {
        return value_;
    }
    /// @brief Get the minimum value.
    float minValue() const
    {
        return min_;
    }
    /// @brief Get the maximum value.
    float maxValue() const
    {
        return max_;
    }
    /// @brief Get the step increment.
    float step() const
    {
        return step_;
    }
    /// @brief Set the current value.
    void setValue(float v);
    /// @brief Set the min/max range.
    void setRange(float minVal, float maxVal);
    /// @brief Set the step increment.
    void setStep(float s)
    {
        step_ = s;
    }

    /// @brief Set number of decimal places shown.
    void setDecimals(int d)
    {
        decimals_ = d;
        markDirty();
    }
    /// @brief Get the decimal precision.
    int decimals() const
    {
        return decimals_;
    }

    /// @brief Set a suffix string appended to the value.
    void setSuffix(const String& s)
    {
        suffix_ = s;
        markDirty();
    }
    /// @brief Get the suffix.
    const String& suffix() const
    {
        return suffix_;
    }

    /// @brief Set a prefix string prepended to the value.
    void setPrefix(const String& p)
    {
        prefix_ = p;
        markDirty();
    }
    /// @brief Get the prefix.
    const String& prefix() const
    {
        return prefix_;
    }

    /// @brief Set drag sensitivity for mouse-drag value adjustment.
    void setDragSensitivity(float s)
    {
        dragSens_ = s;
    }
    /// @brief Get the drag sensitivity.
    float dragSensitivity() const
    {
        return dragSens_;
    }

    Signal<float> onValueChanged;

    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseRelease(MouseEvent& e) override;
    void onMouseMove(MouseEvent& e) override;
    void onMouseScroll(MouseEvent& e) override;
    void onKeyPress(KeyEvent& e) override;

private:
    float min_, max_, value_, step_;
    int decimals_ = 1;
    String suffix_;
    String prefix_;

    bool dragging_ = false;
    float dragStartY_ = 0;
    float dragStartV_ = 0;
    float dragSens_ = 0.1f;

    enum HitZone
    {
        None,
        Minus,
        Plus,
        Value
    };
    HitZone hitZone(float localX) const;
    const String& formatValue() const; // into valueText_, which keeps its room from one frame to the next
    float buttonWidth() const;
    mutable String valueText_;
    ElideMemo valueMemo_; // what paint worked out for a value wider than the room between the buttons (cut to it)
};

// ═════════════════════════════════════════════════════════════════════════════
//  DatePicker — click to open a calendar popup
//    auto* dp = parent->createChild<DatePicker>();
//    dp->onDateChanged.connect([](int y, int m, int d){ ... });
// ═════════════════════════════════════════════════════════════════════════════

struct Date
{
    int year = 2026;
    int month = 1; // 1–12
    int day = 1;   // 1–31
    bool operator==(const Date& o) const
    {
        return year == o.year && month == o.month && day == o.day;
    }
    bool operator!=(const Date& o) const
    {
        return !(*this == o);
    }
};

class DatePicker : public Widget
{
public:
    IG_WIDGET_CLASS(DatePicker, Widget);

    DatePicker();

    /// @brief Get the selected date.
    const Date& date() const
    {
        return date_;
    }
    /// @brief Set the date.
    void setDate(const Date& d);
    /// @brief Set the date by year, month, day.
    void setDate(int year, int month, int day);

    /// @brief Emitted when the date changes (year, month, day).
    Signal<int, int, int> onDateChanged;

    Vec2f sizeHint() const override;
    void  paint(PaintContext& ctx) override;
    // The year, month and day are segments: the one under the pointer is
    // highlighted, a click selects it, the wheel and Up/Down change it,
    // Left/Right move between them. A click on the icon opens the calendar.
    void  onMousePress(MouseEvent& e) override;
    void  onMouseMove(MouseEvent& e) override;
    void  onMouseLeave() override;
    void  onMouseScroll(MouseEvent& e) override;
    void  onKeyPress(KeyEvent& e) override;

    /// @brief Open the calendar popup.
    void openCalendar();
    /// @brief Close the calendar popup.
    void closeCalendar();
    /// @brief Check if the calendar is open.
    bool isOpen() const;

private:
    Date date_;
    int hoverSeg_ = 0; // 0 none, 1 year, 2 month, 3 day
    int activeSeg_ = 0;
    Rect segRect_[3] = {};
    int segmentAt(float x, float y) const;
    void adjust(int seg, int delta);

    String formatDate() const;
};

// ═════════════════════════════════════════════════════════════════════════════
//  TimePicker — hour : minute : second with spin buttons and scroll
//    auto* tp = parent->createChild<TimePicker>();
//    tp->onTimeChanged.connect([](int h, int m, int s){ ... });
// ═════════════════════════════════════════════════════════════════════════════

struct Time
{
    int hour = 0;   // 0–23
    int minute = 0; // 0–59
    int second = 0; // 0–59
    bool operator==(const Time& o) const
    {
        return hour == o.hour && minute == o.minute && second == o.second;
    }
    bool operator!=(const Time& o) const
    {
        return !(*this == o);
    }
};

class TimePicker : public Widget
{
public:
    IG_WIDGET_CLASS(TimePicker, Widget);

    TimePicker();

    /// @brief Get the current time.
    const Time& time() const
    {
        return time_;
    }
    /// @brief Set the time.
    void setTime(const Time& t);
    /// @brief Set the time by hours, minutes, seconds.
    void setTime(int h, int m, int s = 0);

    /// @brief Show or hide the seconds field.
    void setShowSeconds(bool s)
    {
        showSeconds_ = s;
        markDirty();
    }
    /// @brief Check if seconds are shown.
    bool showSeconds() const
    {
        return showSeconds_;
    }

    /// @brief Emitted when the time changes (h, m, s).
    Signal<int, int, int> onTimeChanged;

    Vec2f sizeHint() const override;
    void paint(PaintContext& ctx) override;
    void onMousePress(MouseEvent& e) override;
    void onMouseScroll(MouseEvent& e) override;
    void onKeyPress(KeyEvent& e) override;

private:
    Time time_;
    bool showSeconds_ = true;

    enum Field
    {
        Hour,
        Minute,
        Second
    };
    Field activeField_ = Hour;

    // Layout helpers
    float fieldWidth() const;
    float separatorWidth() const;
    Field hitField(float localX) const;
    void adjustField(Field f, int delta);
    String formatTime() const;
};

} // namespace retained
} // namespace ig
