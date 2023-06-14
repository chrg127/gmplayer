#pragma once

#include <system_error>

namespace gmplayer {

enum class ErrType {
    None, FileType, Header, Play, Seek, LoadFile, LoadTrack, LoadM3U,
};

struct ErrorCategory : public std::error_category {
    ~ErrorCategory() {}
    const char *name() const noexcept { return "gme error"; }
    std::string message(int n) const
    {
        switch (static_cast<ErrType>(n)) {
        case ErrType::None:           return "Success";
        case ErrType::FileType:       return "Invalid music file type";
        case ErrType::Header:         return "Invalid music file header";
        case ErrType::Play:           return "Found an error while playing";
        case ErrType::Seek:           return "Seek error";
        case ErrType::LoadFile:       return "Couldn't load file";
        case ErrType::LoadTrack:      return "Couldn't load track";
        case ErrType::LoadM3U:        return "Couldn't load m3u file";
        default:                      return "Unknown error";
        }
    }
};

inline ErrorCategory error_category;

struct Error {
    std::error_condition code;
    std::string details;
    Error() = default;
    Error(std::error_condition e, std::string_view s) : code{e}, details{s} { }
    Error(ErrType e, std::string_view s) : code{static_cast<int>(e), error_category}, details{s} { }
    operator bool() const { return static_cast<bool>(code); }
    auto message() const { return code.message(); }
};

} // namespace gmplayer
