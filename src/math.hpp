#pragma once

#include <cmath>

namespace math {

template <typename T>
constexpr T map(T x, T in_min, T in_max, T out_min, T out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

template <typename T>
T avg(T a, T b)
{
    auto h = std::max(a, b);
    auto l = std::min(a, b);
    return l + (h - l) / 2;
};

} // namespace math
