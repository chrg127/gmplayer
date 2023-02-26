#pragma once

#include <filesystem>
#include <system_error>
#include <string>
#include <expected.hpp>
#include "io.hpp"

const int SAMPLES   = 2048;
const int CHANNELS  = 2;
const int NUM_SAMPLES = SAMPLES * CHANNELS * 2;

struct Metadata {
    int length;
    std::string system, game, song, author, copyright, comment, dumper;
};

enum class ErrType {
    None, FileType, Header, Play, Seek, LoadFile, LoadTrack, LoadM3U,
};

struct Error {
    std::error_condition code;
    std::string_view details;
    operator bool() const { return static_cast<bool>(code); }
    Error() = default;
    Error(std::error_condition e) : code{e}, details{""} { }
    Error(ErrType e, std::string_view s);
};

using PlayResult = tl::expected<std::array<u8, NUM_SAMPLES>, Error>;

struct Interface {
    virtual ~Interface() = default;
    virtual      Error open(std::span<const u8> data, int frequency)       = 0;
    virtual      Error load_m3u(std::filesystem::path path)                = 0;
    virtual      Error start_track(int n)                                  = 0;
    virtual PlayResult play()                                              = 0;
    virtual      Error seek(int n)                                         = 0;
    virtual        int position()                                    const = 0;
    virtual        int track_count()                                 const = 0;
    virtual   Metadata track_metadata(int which)                     const = 0;
    virtual       bool track_ended()                                 const = 0;
    virtual       void set_fade(int from, int length)                      = 0;
    virtual       void set_tempo(double tempo)                             = 0;
    virtual       void ignore_silence(bool ignore)                         = 0;
};

class Music_Emu;

class GME : public Interface {
    Music_Emu *emu = nullptr;
    int fade_from, fade_len;
public:
    ~GME();
         Error open(std::span<const u8> data, int frequency)       override;
         Error load_m3u(std::filesystem::path path)                override;
         Error start_track(int n)                                  override;
    PlayResult play()                                              override;
         Error seek(int n)                                         override;
           int position()                                    const override;
           int track_count()                                 const override;
      Metadata track_metadata(int which)                     const override;
          bool track_ended()                                 const override;
          void set_fade(int from, int length)                      override;
          void set_tempo(double tempo)                             override;
          void ignore_silence(bool ignore)                         override;
};

auto read_file(const io::MappedFile &file, int frequency) -> tl::expected<std::unique_ptr<Interface>, Error>;
