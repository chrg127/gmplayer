#pragma once

#include <array>
#include <span>
#include "common.hpp"
#include "math.hpp"

namespace visualizer {

void plot(std::span<i16> data, i64 width, i64 height, auto &&draw)
{
    for (i64 i = 0; i < data.size(); i += 2) {
        auto sample1 = math::avg(data[i+0], data[i+1]);
        auto sample2 = math::avg(data[i+2], data[i+3]);
        auto x1 = i / 2;
        auto x2 = i / 2 + 1;
        auto y1 = math::map<i64>(sample1, std::numeric_limits<i16>::min(), std::numeric_limits<i16>::max(), 0, height);
        auto y2 = math::map<i64>(sample2, std::numeric_limits<i16>::min(), std::numeric_limits<i16>::max(), 0, height);
        draw(std::array{x1, y1}, std::array{x2, y2});
    }
}

} // namespace visualizer
