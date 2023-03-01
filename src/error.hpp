#pragma once

#include <system_error>

namespace gmplayer {

enum class ErrType {
    None, FileType, Header, Play, Seek, LoadFile, LoadTrack, LoadM3U,
};

struct Error {
    std::error_condition code;
    std::string details;
    operator bool() const { return static_cast<bool>(code); }
    Error() = default;
    Error(std::error_condition e, std::string_view s) : code{e}, details{s} { }
    Error(ErrType e, std::string_view s);
};

} // namespace gmplayer
