#pragma once

#include <chrono>
#include <system_error>
#include <filesystem>
#include <array>
#include "math.hpp"
#include "concepts.hpp"

namespace gmplayer {

namespace literals {
    inline constexpr long long operator"" _sec(unsigned long long secs) { return secs * 1000ull; }
    inline constexpr long long operator"" _min(unsigned long long mins) { return mins * 60_sec; }
}

struct Error {
    enum class Type { None, Play, Seek, LoadFile, LoadTrack };
    Type code                       = {};
    std::string details             = {};
    std::filesystem::path file_path = {};
    std::string track_name          = {};
    operator bool() const { return static_cast<bool>(code); }
    Type type() const { return code; }
};

struct Metadata {
    enum Field { System = 0, Game, Song, Author, Copyright, Comment, Dumper };
    int length;
    std::array<std::string, 7> info;
};

inline int tempo_to_int(double value) { return math::map(std::log2(value), -2.0, 2.0, 0.0, 100.0); }
inline double int_to_tempo(int value) { return std::exp2(math::map(double(value), 0.0, 100.0, -2.0, 2.0)); }

// void add_fade(short *out, long size, long num_samples, long fade_start, long fade_step);

template <Number T>
constexpr T samples_to_millis(T samples, int sample_rate, int channels)
{
    auto rate = sample_rate * channels;
    auto secs = samples / rate;
    auto frac = samples - secs * rate; // samples % rate;
    return secs * 1000 + frac * 1000 / rate;
}

// equivalent to sample_rate * (millis / 1000) * channels
template <Number T>
constexpr T millis_to_samples(T millis, int sample_rate, int channels)
{
    auto secs = millis / 1000;
    auto frac = millis - secs * 1000; // millis % 1000;
    return (secs * sample_rate + frac * sample_rate / 1000) * channels;
}

// the 1/2**x function, scaled by `unit`
template <Number T>
inline T unit_div_pow2(T x, int step, int unit)
{
    return T(unit / std::pow(2.0, double(x) / step));
    // int shift    = x / step;
    // int fraction = (x - shift * step) * unit / step;
    // return ((unit - fraction) + (fraction >> 1)) >> shift;
}

class Fade {
    int start = -1, step = 0, len = 0;

public:
    static constexpr auto SHIFT = 8l;
    static constexpr auto BLOCK_SIZE = 512l;

    bool is_set() const { return start != -1; }
    int length() const { return len; }
    int get_start() const { return start; }

    // gain = unit_div_pow2(x, step, unit)
    // gain < 64 when x = 5 * sample_rate * 2 / 512
    //
    // unit / 2**(secs * sample_rate * channels / block_size / step) = 64
    // unit = 64 * 2**(secs * sample_rate * channels / block_size / step) = 64
    // 2**shift = 2**fade_shift * 2**(secs * sample_rate * channels / block_size / step) = 64
    //
    // shift = fade_shift + (secs * sample_rate * channels / block_size / step)
    // shift - fade_shift = secs * sample_rate * channels / block_size / step
    // (shift - fade_shift) * step = secs * sample_rate * channels / block_size
    // step = secs * sample_rate * channels / block_size * (shift - fade_shift)
    void set(int from, int length, int sample_rate, int num_channels)
    {
        step = (length / 1000) * sample_rate * num_channels / (BLOCK_SIZE * SHIFT);
        start  = millis_to_samples(from, sample_rate, num_channels);
        len = length;
    }

    void put_in(std::span<short> samples, long num_samples)
    {
        constexpr auto UNIT = 1 << 14;
        for (auto i = 0; i < samples.size(); i += BLOCK_SIZE) {
            auto gain = unit_div_pow2((num_samples + i - start) / BLOCK_SIZE, step, UNIT);
            // if (gain < (UNIT >> fade_shift)) // when close to 0
            //     return true;
            // fmt::print("tell_s = {}, start = {}, i = {}, ", num_samples, start, i);
            // fmt::print("one_div_x({}, {}, {}) gain = {}\n",
                // (num_samples + i - start) / BLOCK_SIZE, step, UNIT, gain);
            for (auto j = 0; j < std::min<long>(BLOCK_SIZE, samples.size() - i); j++)
                samples[i+j] = short(samples[i+j] * gain / UNIT);
        }
    }
};

} // namespace gmplayer
