#include "pch.hpp"

#include "Range.hpp"

#include <r128.h>

#include <cmath>
#include <cstdio>

namespace
{
constexpr double kCmpEpsilon = 0.00001;

bool isEqualApprox(double left, double right)
{
    if (left == right) return true;
    double tolerance = kCmpEpsilon * std::fabs(left);
    if (tolerance < kCmpEpsilon) tolerance = kCmpEpsilon;
    return std::fabs(left - right) < tolerance;
}

double snapped(double value, double step)
{
    if (step != 0) value = std::floor(value / step + 0.5) * step;
    return value;
}

double maxOf(double a, double b)
{
    return a > b ? a : b;
}

double minOf(double a, double b)
{
    return a < b ? a : b;
}

double clampValue(double value, double low, double high)
{
    return value < low ? low : (value > high ? high : value);
}

int stepDecimals(double step)
{
    static const int maxn = 10;
    static const double sd[maxn] = { 0.9999, 0.09999, 0.009999, 0.0009999, 0.00009999, 0.000009999,
        0.0000009999, 0.00000009999, 0.000000009999, 0.0000000009999 };
    const double abs = std::fabs(step);
    const double decs = abs - static_cast<int>(abs);
    for (int i = 0; i < maxn; i++)
        if (decs >= sd[i]) return i;
    return 0;
}
} // namespace

String Range::numberText(double value, int decimals)
{
    if (std::isnan(value)) return "nan";
    if (std::isinf(value)) return std::signbit(value) ? "-inf" : "inf";

    if (decimals < 0)
    {
        decimals = 14;
        const double absValue = std::fabs(value);
        if (absValue > 10) decimals -= static_cast<int>(std::floor(std::log10(absValue)));
    }
    if (decimals > 32) decimals = 32;

    char buf[325];
    if (decimals < 0) std::snprintf(buf, sizeof(buf), "%lf", value);
    else
        std::snprintf(buf, sizeof(buf), "%.*lf", decimals, value);
    buf[324] = 0;

    bool period = false;
    int z = 0;
    while (buf[z])
    {
        if (buf[z] == '.') period = true;
        z++;
    }
    if (period)
    {
        z--;
        while (z > 0)
        {
            if (buf[z] == '0')
            {
                buf[z] = 0;
            }
            else if (buf[z] == '.')
            {
                buf[z + 1] = '0';
                break;
            }
            else
            {
                break;
            }
            z--;
        }
    }
    return buf;
}

int Range::rangeStepDecimals(double step)
{
    if (step < 0.0000000000001) return 16;
    return stepDecimals(step);
}

double Range::snappedR128(double value, double step)
{
    if (step == 0.0) return value;
    if (value > (1e18 * step) || value < -(1e18 * step)) return snapped(value, step);

    const int decimals = static_cast<int>(
            minOf(18.0, 14.0 - std::floor(std::log10(maxOf(std::fabs(value), step)))));
    const double scale = std::pow(10.0, decimals);
    value *= scale;
    step *= scale;

    const String stepText = numberText(step);
    const String valueText = numberText(value);
    R128 stepR128;
    R128 valueR128;
    const R128 half(0.5);
    r128FromString(&stepR128, stepText.c_str(), nullptr);
    r128FromString(&valueR128, valueText.c_str(), nullptr);
    r128Div(&valueR128, &valueR128, &stepR128);
    r128Add(&valueR128, &valueR128, &half);
    r128Floor(&valueR128, &valueR128);
    r128Mul(&valueR128, &valueR128, &stepR128);
    if (scale != 1.0)
    {
        const R128 scaleR128(scale);
        r128Div(&valueR128, &valueR128, &scaleR128);
    }
    return static_cast<double>(valueR128);
}

double Range::calcValue(double value, double step) const
{
    if (std::isnan(value)) return value;

    if (step > 0)
    {
        if (std::fabs(min_) > step * 1e14) value = snappedR128(value, step);
        else
            value = snappedR128(value - min_, step) + min_;
    }

    if (rounded_) value = std::round(value);
    if (!allowGreater_ && value > max_ - page_) value = max_ - page_;
    if (!allowLesser_ && value < min_) value = min_;
    return value;
}

void Range::setValue(double value)
{
    const double previous = value_;
    value_ = calcValue(value, step_);
    if (value_ != previous && !(std::isnan(value_) && std::isnan(previous))) notifyValueChanged();
}

void Range::setValueNoSignal(double value)
{
    const double previous = value_;
    value_ = calcValue(value, step_);
    if (value_ != previous)
    {
        onValueChanged(value_);
        markDirty();
    }
}

void Range::notifyValueChanged()
{
    onValueChanged(value_);
    valueChanged.emit(value_);
    markDirty();
}

void Range::setMin(double minimum)
{
    if (min_ == minimum) return;
    min_ = minimum;
    max_ = maxOf(max_, min_);
    page_ = clampValue(page_, 0, max_ - min_);
    setValue(value_);
    changed.emit();
    markDirty();
}

void Range::setMax(double maximum)
{
    const double validated = maxOf(maximum, min_);
    if (max_ == validated) return;
    max_ = validated;
    page_ = clampValue(page_, 0, max_ - min_);
    setValue(value_);
    changed.emit();
    markDirty();
}

void Range::setStep(double step)
{
    if (step_ == step) return;
    step_ = step;
    changed.emit();
    markDirty();
}

void Range::setPage(double page)
{
    const double validated = clampValue(page, 0, max_ - min_);
    if (page_ == validated) return;
    page_ = validated;
    setValue(value_);
    changed.emit();
    markDirty();
}

void Range::setAsRatio(double ratio)
{
    double v;
    if (expRatio_ && min_ >= 0)
    {
        const double expMin = min_ == 0 ? 0.0 : std::log(min_) / std::log(2.0);
        const double expMax = std::log(max_) / std::log(2.0);
        v = std::pow(2, expMin + (expMax - expMin) * ratio);
    }
    else
    {
        const double percent = (max_ - min_) * ratio;
        if (step_ > 0)
        {
            const double steps = std::round(percent / step_);
            v = steps * step_ + min_;
        }
        else
        {
            v = percent + min_;
        }
    }
    v = clampValue(v, min_, max_);
    setValue(v);
}

double Range::asRatio() const
{
    if (isEqualApprox(max_, min_)) return 1.0;

    if (expRatio_ && min_ >= 0)
    {
        const double expMin = min_ == 0 ? 0.0 : std::log(min_) / std::log(2.0);
        const double expMax = std::log(max_) / std::log(2.0);
        const double v = std::log(clampValue(value_, min_, max_)) / std::log(2.0);
        return clampValue((v - expMin) / (expMax - expMin), 0, 1);
    }
    const double v = clampValue(value_, min_, max_);
    return clampValue((v - min_) / (max_ - min_), 0, 1);
}

void Range::setExpRatio(bool enable)
{
    if (expRatio_ == enable) return;
    expRatio_ = enable;
    onValueChanged(value_);
    markDirty();
}
