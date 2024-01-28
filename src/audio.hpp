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

struct Fade {
    enum class Type { In, Out };

    static constexpr auto SHIFT = 8l;
    static constexpr auto BLOCK_SIZE = 512l;
    static constexpr auto UNIT_SHIFT = 14;
    static constexpr auto UNIT = 1 << UNIT_SHIFT;

private:
    int start = -1, step = 0, len = 0;
    Type type;

public:
    Fade() = default;
    Fade(Type type, int from, int length, int sample_rate, int num_channels);

    bool is_set()   const { return start != -1; }
    int length()    const { return len; }
    int get_start() const { return start; }
    void put_in(std::span<short> samples, long num_samples);
};

} // namespace gmplayer
