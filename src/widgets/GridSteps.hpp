#pragma once
// ═════════════════════════════════════════════════════════════════════════════
//  GridSteps.hpp — how many steps a loop over a float range takes, worked out before the loop.
//
//  `for (float x = a; x < b; x += step)` does not end when step is 0, negative or NaN, or when x is so
//  big that x + step == x (above 2^24 a float cannot add one), and with a huge range it draws a line for every
//  step and takes the memory of the machine. The loops that draw grid lines, ticks and checkerboards count
//  their steps here first, as an integer that is never more than what fits in the pixels they paint, and run
//  that many (a loop that adds `step` to a float can keep doing so: the values it draws are the same as
//  before, only the count of turns is bounded).
// ═════════════════════════════════════════════════════════════════════════════

#include "RetainedBase.hpp"


namespace ig
{
namespace retained
{

/// The most turns a loop gets for a stretch of `px` pixels: one for each pixel and two more for the ends (a grid
/// line or a tick cannot show more than one for each pixel), and never more than a very wide window needs. Zero
/// for a width that is not a positive number.
inline int pixelCap(float px)
{
    if (!(px > 0.0f)) return 0;
    return static_cast<int>(px < 16384.0f ? px : 16384.0f) + 2;
}

/// A bound for the turns of a loop that goes `first`, `first + step`, ... while the value is not past `last`
/// and keeps its own test of that: the steps there are, and one more (a float that is added to again and again
/// can land a step either side of first + k * step), but never more than maxCount. Zero when a number is NaN or
/// infinite, `step` is not positive or `last` is before `first`. The count is worked out in double, so a range as
/// wide as the floats are does not overflow it.
inline int stepLimit(float first, float last, float step, int maxCount)
{
    if (maxCount <= 0 || !(step > 0.0f) || !(last >= first)) return 0;
    if (!Math::isFinite(first) || !Math::isFinite(last) || !Math::isFinite(step)) return 0;
    const double n = floor((static_cast<double>(last) - static_cast<double>(first)) / static_cast<double>(step)) + 2.0;
    if (!(n < static_cast<double>(maxCount))) return maxCount;
    return static_cast<int>(n);
}

/// The marks of a grid that a view of `px` pixels can show, as a first mark, the step between marks and a
/// count: the marks are `first` and every `unit` after it, up to `last`. When there are more than one for
/// each pixel (a view zoomed far out, or a huge range) the step becomes a whole number of units, so that there is at
/// most one mark for a pixel, and the first mark moves to the first multiple of the step; the count never goes
/// over the pixels, so the loop that draws them always ends. Nothing (count 0) when a number is NaN or
/// infinite, `unit` is not positive, or the range or the view is empty.
struct GridMarks
{
    float first = 0.0f;
    float step = 1.0f;
    int count = 0;
};

inline GridMarks gridMarks(float first, float last, float unit, float px)
{
    GridMarks run;
    run.first = first;
    run.step = unit;
    if (!(px > 0.0f) || !(unit > 0.0f) || !(last >= first)) return run;
    if (!Math::isFinite(first) || !Math::isFinite(last) || !Math::isFinite(unit) || !Math::isFinite(px)) return run;
    // one mark for each pixel, and the one just before and the one just after; never more than a very wide
    // view of a screen can hold
    const double room = (px < 16384.0f ? static_cast<double>(px) : 16384.0) + 2.0;
    const double span = static_cast<double>(last) - static_cast<double>(first);
    const double marks = floor(span / static_cast<double>(unit)) + 1.0;
    if (marks <= room)
    {
        run.count = static_cast<int>(marks);
        return run;
    }
    const double step = static_cast<double>(unit) * ceil(marks / room);
    if (!(step < 3.0e38)) return run;
    run.step = static_cast<float>(step);
    run.first = static_cast<float>(ceil(static_cast<double>(first) / step) * step);
    const double left = floor((static_cast<double>(last) - static_cast<double>(run.first)) / step) + 1.0;
    if (!(left >= 1.0)) return run;
    run.count = static_cast<int>(left < room ? left : room);
    return run;
}

} // namespace retained
} // namespace ig
