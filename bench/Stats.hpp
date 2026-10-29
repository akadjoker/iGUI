#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace bench
{

// Percentile of a copy of the samples; nearest-rank, so p95 of 300 frames is
// the 285th sample rather than an interpolation between two of them.
inline double percentile(std::vector<double> values, double fraction)
{
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const std::size_t rank =
            static_cast<std::size_t>(std::ceil(fraction * static_cast<double>(values.size())));
    const std::size_t index = rank == 0 ? 0 : std::min(rank - 1, values.size() - 1);
    return values[index];
}

inline double mean(const std::vector<double>& values)
{
    if (values.empty()) return 0.0;
    double total = 0.0;
    for (std::size_t i = 0; i < values.size(); ++i) total += values[i];
    return total / static_cast<double>(values.size());
}

} // namespace bench
