#pragma once

#include <filesystem>
#include <string>
#include <expected.hpp>
#include "io.hpp"
#include "error.hpp"

const int SAMPLES   = 2048;
const int CHANNELS  = 2;
const int SAMPLES_SIZE = SAMPLES * CHANNELS * 2;

struct Metadata {
    int length;
    std::string system, game, song, author, copyright, comment, dumper;
};

using PlayResult = tl::expected<std::array<u8, SAMPLES_SIZE>, Error>;

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
