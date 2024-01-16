#pragma once

#include <chrono>
#include <system_error>
#include <filesystem>
#include <array>
#include "math.hpp"

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

} // namespace gmplayer
