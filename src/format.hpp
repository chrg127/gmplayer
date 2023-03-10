#pragma once

#include <filesystem>
#include <string>
#include <expected.hpp>
#include "io.hpp"
#include "error.hpp"

class Music_Emu;

namespace gmplayer {

const int SAMPLES   = 2048;
const int CHANNELS  = 2;
const int SAMPLES_SIZE = SAMPLES * CHANNELS * 2;

struct Metadata {
    enum {
        System = 0, Game, Song, Author, Copyright, Comment, Dumper
    };
    int length;
    std::array<std::string, 7> info;
};

using PlayResult = tl::expected<std::array<u8, SAMPLES_SIZE>, Error>;

struct Interface {
    virtual ~Interface() = default;
    virtual Error       open(std::span<const u8> data, int frequency)       = 0;
    virtual Error       load_m3u(std::filesystem::path path)                = 0;
    virtual Error       start_track(int n)                                  = 0;
    virtual PlayResult  play()                                              = 0;
    virtual Error       seek(int n)                                         = 0;
    virtual int         position()                                    const = 0;
    virtual int         track_count()                                 const = 0;
    virtual Metadata    track_metadata(int which, int default_length)       = 0;
    virtual bool        track_ended()                                 const = 0;
    virtual int         channel_count()                               const = 0;
    virtual std::string channel_name(int index)                       const = 0;
    virtual void        mute_channel(int index, bool mute)                  = 0;
    virtual void        set_fade(int from, int length)                      = 0;
    virtual void        set_tempo(double tempo)                             = 0;
};

struct Default : public Interface {
               ~Default()                                                    { }
    Error       open(std::span<const u8> data, int frequency)       override { return Error{}; }
    Error       load_m3u(std::filesystem::path path)                override { return Error{}; }
    Error       start_track(int n)                                  override { return Error{}; }
    PlayResult  play()                                              override { return PlayResult{}; }
    Error       seek(int n)                                         override { return Error{}; }
    int         position()                                    const override { return 0; }
    int         track_count()                                 const override { return 0; }
    Metadata    track_metadata(int which, int default_length)       override { return Metadata{}; }
    bool        track_ended()                                 const override { return true; }
    int         channel_count()                               const override { return 0; }
    std::string channel_name(int index)                       const override { return ""; }
    void        mute_channel(int index, bool mute)                  override { }
    void        set_fade(int from, int length)                      override { }
    void        set_tempo(double tempo)                             override { }
};

class GME : public Interface {
    Music_Emu *emu = nullptr;
    int fade_from, fade_len, track_len;
public:
               ~GME();
    Error       open(std::span<const u8> data, int frequency)       override;
    Error       load_m3u(std::filesystem::path path)                override;
    Error       start_track(int n)                                  override;
    PlayResult  play()                                              override;
    Error       seek(int n)                                         override;
    int         position()                                    const override;
    int         track_count()                                 const override;
    Metadata    track_metadata(int which, int default_length)       override;
    bool        track_ended()                                 const override;
    int         channel_count()                               const override;
    std::string channel_name(int index)                       const override;
    void        mute_channel(int index, bool mute)                  override;
    void        set_fade(int from, int length)                      override;
    void        set_tempo(double tempo)                             override;
};

auto read_file(const io::MappedFile &file, int frequency) -> tl::expected<std::unique_ptr<Interface>, Error>;

} // namespace gmplayer
