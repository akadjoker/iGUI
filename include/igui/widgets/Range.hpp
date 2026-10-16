#pragma once

#include "Signal.hpp"
#include "Widget.hpp"
#include <igui/widgets/String.hpp>

namespace ig
{
namespace retained
{
// ═════════════════════════════════════════════════════════════════════════════
//  Range — a value inside [min, max - page], snapped to a step
//
//  The base of the widgets that edit a number: it keeps the value, the range
//  and the step, and tells when the value or the range change.
// ═════════════════════════════════════════════════════════════════════════════

class Range : public Widget
{
public:
    IG_WIDGET_CLASS(Range, Widget);

    /// @brief Set the value, snapped to the step and kept in range; emits valueChanged when it
    /// changes.
    void setValue(double value);
    /// @brief Set the value as setValue does, without emitting valueChanged.
    void setValueNoSignal(double value);
    /// @brief Set the minimum (the maximum is raised to it when lower).
    void setMin(double minimum);
    /// @brief Set the maximum (never below the minimum).
    void setMax(double maximum);
    /// @brief Set the step the value snaps to (0 for none).
    void setStep(double step);
    /// @brief Set the page: the value stays at most max - page.
    void setPage(double page);
    /// @brief Set the value from a ratio between 0 (min) and 1 (max).
    void setAsRatio(double ratio);

    /// @brief Get the value.
    double value() const
    {
        return value_;
    }
    /// @brief Get the minimum.
    double minValue() const
    {
        return min_;
    }
    /// @brief Get the maximum.
    double maxValue() const
    {
        return max_;
    }
    /// @brief Get the step.
    double step() const
    {
        return step_;
    }
    /// @brief Get the page.
    double page() const
    {
        return page_;
    }
    /// @brief Get where the value sits between min (0) and max (1).
    double asRatio() const;

    /// @brief Round the value to whole numbers.
    void setUseRoundedValues(bool enable)
    {
        rounded_ = enable;
    }
    /// @brief Check if the value is rounded to whole numbers.
    bool usingRoundedValues() const
    {
        return rounded_;
    }
    /// @brief Map the ratio exponentially (for a minimum of 0 or more).
    void setExpRatio(bool enable);
    /// @brief Check if the ratio is exponential.
    bool ratioExp() const
    {
        return expRatio_;
    }
    /// @brief Let the value go above the maximum.
    void setAllowGreater(bool allow)
    {
        allowGreater_ = allow;
    }
    /// @brief Check if the value may go above the maximum.
    bool greaterAllowed() const
    {
        return allowGreater_;
    }
    /// @brief Let the value go below the minimum.
    void setAllowLesser(bool allow)
    {
        allowLesser_ = allow;
    }
    /// @brief Check if the value may go below the minimum.
    bool lesserAllowed() const
    {
        return allowLesser_;
    }

    /// @brief Emitted when the value changes.
    Signal<double> valueChanged;
    /// @brief Emitted when the minimum, the maximum, the step or the page change.
    Signal<> changed;

    /// @brief A number as text with `decimals` digits after the point and the
    ///        trailing zeros dropped (one kept after the point); with a negative
    ///        count, 14 digits after the point less one per digit above 10.
    static String numberText(double value, int decimals = -1);
    /// @brief How many digits after the point a step shows (16 for a step of 0).
    static int rangeStepDecimals(double step);

protected:
    // The value snapped to the step in decimal, not binary: 0.1 + 0.2 snapped
    // to 0.1 gives 0.3 exactly.
    static double snappedR128(double value, double step);
    double calcValue(double value, double step) const;
    // Called when the value changes, with or without the signal.
    virtual void onValueChanged(double)
    {
    }
    // Emit valueChanged for the value as it is (after a setValueNoSignal).
    void notifyValueChanged();

private:
    double value_ = 0.0;
    double min_ = 0.0;
    double max_ = 100.0;
    double step_ = 1.0;
    double page_ = 0.0;
    bool expRatio_ = false;
    bool allowGreater_ = false;
    bool allowLesser_ = false;
    bool rounded_ = false;
};

} // namespace retained
} // namespace ig
