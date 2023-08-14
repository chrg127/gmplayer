#pragma once

#include <system_error>
#include <filesystem>

namespace gmplayer {

struct Error {
    enum class Type { None, Play, Seek, LoadFile, LoadTrack };
    std::error_condition code       = {};
    std::string details             = {};
    std::filesystem::path file_path = {};
    std::string track_name          = {};
    operator bool() const { return static_cast<bool>(code); }
    auto message() const { return code.message(); }
    Type type() const { return static_cast<Type>(code.value()); }
};

struct ErrorCategory : public std::error_category {
    ~ErrorCategory() {}
    const char *name() const noexcept { return "gme error"; }
    std::string message(int n) const
    {
        switch (static_cast<Error::Type>(n)) {
        case Error::Type::None:      return "Success";
        case Error::Type::Play:      return "Found an error while playing";
        case Error::Type::Seek:      return "Seek error";
        case Error::Type::LoadFile:  return "Couldn't load file";
        case Error::Type::LoadTrack: return "Couldn't load track";
        default:                     return "Unknown error";
        }
    }
};

inline ErrorCategory error_category;

inline std::error_condition make_errcond(Error::Type type) { return { static_cast<int>(type), error_category }; }

} // namespace gmplayer
