#pragma once
// The searching and reducing a PlotChart does on its series, header only (also usable on its own, and what the tests of the chart check
// numerically): the first sample at or after an x (galloping from a hint), and a pyramid of min/max blocks that answers the minimum and the
// maximum of any range of samples in O(log n) after being built once. NaN samples are gaps: they are skipped by the minimum and the maximum
// and flagged.
#include "Retained.hpp" // (ct::Vector, size_t, uint8_t)

namespace ig
{
namespace retained
{
namespace plot
{

/// The index of the first sample of the ascending array x[0, n) that is not less than `v` (n when every one is), searched from `hint` by
/// doubling steps: a column of a chart asks for the boundary next to the one it asked for before, which is a few samples away.
inline size_t lowerBoundFrom(const double* x, size_t n, size_t hint, double v)
{
    if (n == 0) return 0;
    if (hint > n) hint = n;
    size_t lo, hi;
    if (hint < n && x[hint] < v)
    {
        size_t i = hint, step = 1;
        while (i + step < n && x[i + step] < v)
        {
            i += step;
            step <<= 1;
        }
        lo = i + 1;
        hi = i + step < n ? i + step : n;
    }
    else
    {
        size_t i = hint, step = 1;
        while (i >= step && x[i - step] >= v)
        {
            i -= step;
            step <<= 1;
        }
        hi = i;
        lo = i >= step ? i - step + 1 : 0;
    }
    while (lo < hi)
    {
        const size_t mid = lo + (hi - lo) / 2;
        if (x[mid] < v) lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

inline size_t lowerBound(const double* x, size_t n, double v)
{
    return lowerBoundFrom(x, n, n / 2, v);
}

/// The minimum and the maximum of some samples, and whether there was a gap (a NaN) among them; `any` is false when no sample was a number.
struct MinMax
{
    double lo = 0.0;
    double hi = 0.0;
    bool any = false;
    bool gap = false;

    void add(double v)
    {
        if (v != v)
        {
            gap = true;
            return;
        }
        if (!any)
        {
            lo = hi = v;
            any = true;
        }
        else
        {
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
    }
    void merge(const MinMax& o)
    {
        gap = gap || o.gap;
        if (!o.any) return;
        if (!any)
        {
            lo = o.lo;
            hi = o.hi;
            any = true;
        }
        else
        {
            if (o.lo < lo) lo = o.lo;
            if (o.hi > hi) hi = o.hi;
        }
    }
};

/// A pyramid of minimum and maximum over blocks of 32 samples and, above them, of pairs of blocks, to the top. Built once for a revision of the
/// data (build() takes a budget of samples and says whether it finished: a chart builds a big series a few frames at a time) and answers query()
/// for any range in O(log n); ranges that are short, and the edges of the others, are read from the samples.
class Pyramid
{
public:
    enum
    {
        kBlock = 32,
        kLinearBlocks = 64, // a range of at most this many blocks is read block after block: the climb costs more
        kMaxLevels = 28
    };

    void reset()
    {
        n_ = 0;
        builtBlocks_ = 0;
        ready_ = false;
        for (int l = 0; l < kMaxLevels; ++l)
        {
            lo_[l].clear();
            hi_[l].clear();
            gap_[l].clear();
        }
    }
    bool ready() const
    {
        return ready_;
    }
    size_t samples() const
    {
        return n_;
    }
    /// 0..1: the blocks of the lowest level built so far.
    float progress() const
    {
        const size_t blocks = (n_ + kBlock - 1) / kBlock;
        return ready_ || blocks == 0 ? 1.0f : static_cast<float>(builtBlocks_) / static_cast<float>(blocks);
    }
    /// Starts over for `n` samples (the data changed).
    void begin(size_t n)
    {
        reset();
        n_ = n;
        const size_t blocks = (n + kBlock - 1) / kBlock;
        lo_[0].resize(blocks);
        hi_[0].resize(blocks);
        gap_[0].resize(blocks);
        if (n < static_cast<size_t>(kBlock) * 2) ready_ = true; // (too short to be worth a pyramid: queries read the samples)
    }
    /// Reduces up to `budget` more samples of `y` (the same array and count as begin()); true when the pyramid is complete.
    bool build(const double* y, size_t budget)
    {
        if (ready_) return true;
        const size_t blocks = (n_ + kBlock - 1) / kBlock;
        size_t maxBlocks = budget / kBlock + 1;
        while (builtBlocks_ < blocks && maxBlocks > 0)
        {
            const size_t a = builtBlocks_ * kBlock;
            const size_t b = a + kBlock < n_ ? a + kBlock : n_;
            Acc acc;
            acc.samples(y, a, b);
            // a block with nothing but gaps keeps lo > hi (+inf, -inf): it adds nothing; the flag says there were gaps
            lo_[0][builtBlocks_] = acc.lo;
            hi_[0][builtBlocks_] = acc.hi;
            gap_[0][builtBlocks_] = acc.gap ? 1 : 0;
            ++builtBlocks_;
            --maxBlocks;
        }
        if (builtBlocks_ < blocks) return false;
        // the levels above
        size_t count = blocks;
        for (int l = 1; l < kMaxLevels && count > 1; ++l)
        {
            const size_t up = (count + 1) / 2;
            lo_[l].resize(up);
            hi_[l].resize(up);
            gap_[l].resize(up);
            for (size_t i = 0; i < up; ++i)
            {
                const size_t a = 2 * i, b = a + 1 < count ? a + 2 : a + 1;
                Acc acc;
                acc.blocks(*this, l - 1, a, b);
                lo_[l][i] = acc.lo;
                hi_[l][i] = acc.hi;
                gap_[l][i] = acc.gap ? 1 : 0;
            }
            count = up;
        }
        ready_ = true;
        return true;
    }
    /// The minimum, the maximum and the gaps among y[a, b); the samples of `y` are read at the edges and where the pyramid is not built.
    MinMax query(const double* y, size_t a, size_t b) const
    {
        MinMax m;
        if (b > n_) b = n_;
        if (a >= b) return m;
        Acc acc;
        if (!ready_ || b - a < static_cast<size_t>(kBlock) * 2)
            acc.samples(y, a, b);
        else
        {
            const size_t bA = (a + kBlock - 1) / kBlock, bB = b / kBlock;
            acc.samples(y, a, bA * kBlock);
            acc.samples(y, bB * kBlock, b);
            if (bB - bA <= static_cast<size_t>(kLinearBlocks))
                acc.blocks(*this, 0, bA, bB);
            else
            {
                size_t lo = bA, hi = bB;
                int level = 0;
                while (lo < hi)
                {
                    if (lo & 1)
                    {
                        acc.blocks(*this, level, lo, lo + 1);
                        ++lo;
                    }
                    if (hi & 1)
                    {
                        --hi;
                        acc.blocks(*this, level, hi, hi + 1);
                    }
                    lo >>= 1;
                    hi >>= 1;
                    ++level;
                }
            }
        }
        m.any = acc.lo <= acc.hi;
        m.lo = m.any ? acc.lo : 0.0;
        m.hi = m.any ? acc.hi : 0.0;
        m.gap = acc.gap;
        return m;
    }

private:
    /// A running minimum and maximum that starts empty (lo > hi), so that no sample, block or gap needs a branch of its own: a NaN is never less or greater
    /// than anything, so the comparisons skip it, and `gap` collects whether there was one.
    struct Acc
    {
        double lo = __builtin_huge_val();
        double hi = -__builtin_huge_val();
        bool gap = false;

        // four chains at once: a minimum is a dependent chain, and a column of a chart reads some dozens of values
        static double lessOf(double a, double b)
        {
            return a < b ? a : b;
        }
        static double moreOf(double a, double b)
        {
            return a > b ? a : b;
        }
        void samples(const double* y, size_t a, size_t b)
        {
            double l0 = lo, l1 = lo, l2 = lo, l3 = lo;
            double h0 = hi, h1 = hi, h2 = hi, h3 = hi;
            bool g = false;
            size_t i = a;
            for (; i + 4 <= b; i += 4)
            {
                const double v0 = y[i], v1 = y[i + 1], v2 = y[i + 2], v3 = y[i + 3];
                g |= (v0 != v0) | (v1 != v1) | (v2 != v2) | (v3 != v3);
                l0 = lessOf(v0, l0);
                l1 = lessOf(v1, l1);
                l2 = lessOf(v2, l2);
                l3 = lessOf(v3, l3);
                h0 = moreOf(v0, h0);
                h1 = moreOf(v1, h1);
                h2 = moreOf(v2, h2);
                h3 = moreOf(v3, h3);
            }
            for (; i < b; ++i)
            {
                const double v = y[i];
                g |= v != v;
                l0 = lessOf(v, l0);
                h0 = moreOf(v, h0);
            }
            lo = lessOf(lessOf(l0, l1), lessOf(l2, l3));
            hi = moreOf(moreOf(h0, h1), moreOf(h2, h3));
            gap = gap || g;
        }
        void blocks(const Pyramid& p, int level, size_t a, size_t b)
        {
            const double* pl = p.lo_[level].data();
            const double* ph = p.hi_[level].data();
            const uint8_t* pg = p.gap_[level].data();
            double l0 = lo, l1 = lo, l2 = lo, l3 = lo;
            double h0 = hi, h1 = hi, h2 = hi, h3 = hi;
            uint8_t g = 0;
            size_t k = a;
            for (; k + 4 <= b; k += 4)
            {
                l0 = lessOf(pl[k], l0);
                l1 = lessOf(pl[k + 1], l1);
                l2 = lessOf(pl[k + 2], l2);
                l3 = lessOf(pl[k + 3], l3);
                h0 = moreOf(ph[k], h0);
                h1 = moreOf(ph[k + 1], h1);
                h2 = moreOf(ph[k + 2], h2);
                h3 = moreOf(ph[k + 3], h3);
                g |= pg[k] | pg[k + 1] | pg[k + 2] | pg[k + 3];
            }
            for (; k < b; ++k)
            {
                l0 = lessOf(pl[k], l0);
                h0 = moreOf(ph[k], h0);
                g |= pg[k];
            }
            lo = lessOf(lessOf(l0, l1), lessOf(l2, l3));
            hi = moreOf(moreOf(h0, h1), moreOf(h2, h3));
            gap = gap || g != 0;
        }
    };

    size_t n_ = 0;
    size_t builtBlocks_ = 0;
    bool ready_ = false;
    ct::Vector<double> lo_[kMaxLevels];
    ct::Vector<double> hi_[kMaxLevels];
    ct::Vector<uint8_t> gap_[kMaxLevels];
};

} // namespace plot
} // namespace retained
} // namespace ig
