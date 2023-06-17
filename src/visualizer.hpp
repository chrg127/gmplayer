#pragma once

#include <array>
#include <span>
#include "common.hpp"
#include "math.hpp"

namespace visualizer {

void plot(std::span<i16> data, i64 width, i64 height, int voice, int num_channels, int num_voices, auto &&draw)
{
    auto m = [&](auto s) { return math::map<i64>(s, std::numeric_limits<i16>::min(), std::numeric_limits<i16>::max(), 0, height); };
    const auto frame_size = num_voices * num_channels;
    const auto num_frames = data.size() / frame_size;
    for (i64 f = 0; f < num_frames; f += 2) {
        auto y0 = m(math::avg(data.subspan((f+0)*frame_size + voice*num_channels*2 + 0, num_channels)));
        auto y1 = m(math::avg(data.subspan((f+0)*frame_size + voice*num_channels*2 + 2, num_channels)));
        auto y2 = m(math::avg(data.subspan((f+2)*frame_size + voice*num_channels*2 + 0, num_channels)));
        draw(std::array{f+0, y0}, std::array{f+1, y1});
        draw(std::array{f+1, y1}, std::array{f+2, y2});
    }
}

} // namespace visualizer
