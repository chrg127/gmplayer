#pragma once

#include <array>
#include <span>
#include "common.hpp"
#include "math.hpp"

namespace visualizer {

void plot(std::span<i16> data, i64 width, i64 height, int channel, int channel_size, int num_channels, auto &&draw)
{
    auto frame_size = num_channels * channel_size;
    auto num_frames = data.size() / frame_size;

    for (i64 f = 0; f < num_frames - 1; f++) {
        auto sample1 = math::avg(data.subspan((f+0)*frame_size + channel*channel_size, channel_size));
        auto sample2 = math::avg(data.subspan((f+1)*frame_size + channel*channel_size, channel_size));
        auto y1 = math::map<i64>(sample1, std::numeric_limits<i16>::min(), std::numeric_limits<i16>::max(), 0, height);
        auto y2 = math::map<i64>(sample2, std::numeric_limits<i16>::min(), std::numeric_limits<i16>::max(), 0, height);
        draw(std::array{f, y1}, std::array{f+1, y2});
    }
}

} // namespace visualizer
