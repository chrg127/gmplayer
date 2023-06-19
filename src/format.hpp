#pragma once

#include <string>
#include <span>
#include <memory>
#include <expected.hpp>
#include "common.hpp"
#include "error.hpp"

namespace io { class MappedFile; }

class Music_Emu;

namespace gmplayer {

const int NUM_FRAMES    = 2048;
const int NUM_CHANNELS  = 2;
const int NUM_VOICES    = 8;
const int FRAME_SIZE    = NUM_VOICES * NUM_CHANNELS;

namespace literals {
    inline constexpr long long operator"" _sec(unsigned long long secs) { return secs * 1000ull; }
    inline constexpr long long operator"" _min(unsigned long long mins) { return mins * 60_sec; }
}

struct Metadata {
    enum { System = 0, Game, Song, Author, Copyright, Comment, Dumper };
    int length;
    std::array<std::string, 7> info;
};

struct FormatInterface {
    virtual ~FormatInterface() = default;
    virtual Error       start_track(int n)                       = 0;
    virtual Error       play(std::span<i16> out)                 = 0;
    virtual Error       seek(int n)                              = 0;
    virtual int         position()                         const = 0;
    virtual int         track_count()                      const = 0;
    virtual Metadata    track_metadata(int which)                = 0;
    virtual bool        track_ended()                      const = 0;
    virtual int         channel_count()                    const = 0;
    virtual std::string channel_name(int index)            const = 0;
    virtual void        mute_channel(int index, bool mute)       = 0;
    virtual void        set_fade(int from, int length)           = 0;
    virtual void        set_tempo(double tempo)                  = 0;
    virtual bool        is_multi_channel()                 const = 0;
};

struct Default : public FormatInterface {
    ~Default() { }
    Error       start_track(int n)                       override { return Error{}; }
    Error       play(std::span<i16> out)                 override { return Error{}; }
    Error       seek(int n)                              override { return Error{}; }
    int         position()                         const override { return 0; }
    int         track_count()                      const override { return 0; }
    Metadata    track_metadata(int which)                override { return Metadata{}; }
    bool        track_ended()                      const override { return true; }
    int         channel_count()                    const override { return 0; }
    std::string channel_name(int index)            const override { return ""; }
    void        mute_channel(int index, bool mute)       override { }
    void        set_fade(int from, int length)           override { }
    void        set_tempo(double tempo)                  override { }
    bool        is_multi_channel()                 const override { return false; }
};

class GME : public FormatInterface {
    Music_Emu *emu = nullptr;
    int fade_from = 0, fade_len = 0, default_length = 0, track_len = 0;
public:
    GME(Music_Emu *emu, int default_length) : emu{emu}, default_length{default_length} { }
    ~GME();
    Error       start_track(int n)                       override;
    Error       play(std::span<i16> out)                 override;
    Error       seek(int n)                              override;
    int         position()                         const override;
    int         track_count()                      const override;
    Metadata    track_metadata(int which)                override;
    bool        track_ended()                      const override;
    int         channel_count()                    const override;
    std::string channel_name(int index)            const override;
    void        mute_channel(int index, bool mute)       override;
    void        set_fade(int from, int length)           override;
    void        set_tempo(double tempo)                  override;
    bool        is_multi_channel()                 const override;
    static auto make(const io::MappedFile &file, int frequency, int default_length)
        -> tl::expected<std::unique_ptr<FormatInterface>, Error>;
};

inline std::unique_ptr<FormatInterface> make_default_format() { return std::make_unique<Default>(); }

auto read_file(const io::MappedFile &file, int frequency, int default_length)
    -> tl::expected<std::unique_ptr<FormatInterface>, Error>;

} // namespace gmplayer
