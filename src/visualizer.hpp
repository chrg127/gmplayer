#pragma once

#include <array>
#include <span>
#include "common.hpp"
#include "math.hpp"

namespace visualizer {

void plot(std::span<i16> data, i64 width, i64 height, int channel, int channel_size, int num_channels, auto &&draw)
{
    auto stride = num_channels * channel_size;
    for (i64 i = 0; i < data.size() - stride; i += stride) {
        auto si_cur  = i        + channel*channel_size;
        auto si_next = i+stride + channel*channel_size;
        auto sample1 = math::avg(data.subspan(si_cur,  channel_size));
        auto sample2 = math::avg(data.subspan(si_next, channel_size));
        auto x1 = i / stride;
        auto x2 = i / stride + 1;
        auto y1 = math::map<i64>(sample1, std::numeric_limits<i16>::min(), std::numeric_limits<i16>::max(), 0, height);
        auto y2 = math::map<i64>(sample2, std::numeric_limits<i16>::min(), std::numeric_limits<i16>::max(), 0, height);
        draw(std::array{x1, y1}, std::array{x2, y2});
    }
}

} // namespace visualizer
