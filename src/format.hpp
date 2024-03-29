#pragma once

#include <string>
#include <span>
#include <memory>
#include <tl/expected.hpp>
#include "common.hpp"
#include "audio.hpp"
#include "const.hpp"

namespace io { class MappedFile; }

class Music_Emu;
class GsfEmu;

namespace gmplayer {

struct FormatInterface {
    virtual ~FormatInterface() = default;
    virtual Error       start_track(int n)                       = 0;
    virtual Error       play(std::span<i16> out)                 = 0;
    virtual Error       seek(int n)                              = 0;
    virtual void        mute_channel(int index, bool mute)       = 0;
    virtual void        set_fade_out(int length)                 = 0;
    virtual void        set_fade_in(int length)                  = 0;
    virtual void        set_tempo(double tempo)                  = 0;
    virtual int         position()                         const = 0;
    virtual int         track_count()                      const = 0;
    virtual Metadata    track_metadata()                   const = 0;
    virtual Metadata    track_metadata(int which)          const = 0;
    virtual bool        track_ended()                      const = 0;
    virtual int         channel_count()                    const = 0;
    virtual std::string channel_name(int index)            const = 0;
    virtual bool        is_multi_channel()                 const = 0;
};

struct Default : public FormatInterface {
    ~Default() { }
    Error       start_track(int n)                       override { return Error{}; }
    Error       play(std::span<i16> out)                 override { return Error{}; }
    Error       seek(int n)                              override { return Error{}; }
    void        mute_channel(int index, bool mute)       override { }
    void        set_fade_out(int length)                 override { }
    void        set_fade_in(int length)                  override { }
    void        set_tempo(double tempo)                  override { }
    int         position()                         const override { return 0; }
    int         track_count()                      const override { return 0; }
    Metadata    track_metadata()                   const override { return Metadata{}; }
    Metadata    track_metadata(int which)          const override { return Metadata{}; }
    bool        track_ended()                      const override { return true; }
    int         channel_count()                    const override { return 0; }
    std::string channel_name(int index)            const override { return ""; }
    bool        is_multi_channel()                 const override { return false; }
};

class GME : public FormatInterface {
    Music_Emu *emu = nullptr;
    int fade_len = 0, default_length = 0;
    std::filesystem::path file_path = {};
    Metadata metadata;
    Fade fade_in;

public:
    GME(Music_Emu *emu, int default_length, std::filesystem::path file_path)
        : emu{emu}
        , default_length{default_length}
        , file_path{file_path}
    { }
    ~GME();
    Error       start_track(int n)                       override;
    Error       play(std::span<i16> out)                 override;
    Error       seek(int n)                              override;
    void        mute_channel(int index, bool mute)       override;
    void        set_fade_out(int length)                 override;
    void        set_fade_in(int length)                  override;
    void        set_tempo(double tempo)                  override;
    int         position()                         const override;
    int         track_count()                      const override;
    Metadata    track_metadata()                   const override;
    Metadata    track_metadata(int which)          const override;
    bool        track_ended()                      const override;
    int         channel_count()                    const override;
    std::string channel_name(int index)            const override;
    bool        is_multi_channel()                 const override;
    static auto make(const io::MappedFile &file, int frequency, int default_length)
        -> tl::expected<std::unique_ptr<FormatInterface>, const char *>;
};

class GSF : public FormatInterface {
    GsfEmu *emu = nullptr;
    Fade fade_out;
    Fade fade_in;

public:
    explicit GSF(GsfEmu *emu) : emu{emu} {}
    ~GSF();
    Error       start_track(int n)                       override;
    Error       play(std::span<i16> out)                 override;
    Error       seek(int n)                              override;
    void        mute_channel(int index, bool mute)       override;
    void        set_fade_out(int length)                 override;
    void        set_fade_in(int length)                  override;
    void        set_tempo(double tempo)                  override;
    int         position()                         const override;
    int         track_count()                      const override;
    Metadata    track_metadata()                   const override;
    Metadata    track_metadata(int which)          const override;
    bool        track_ended()                      const override;
    int         channel_count()                    const override;
    std::string channel_name(int index)            const override;
    bool        is_multi_channel()                 const override;
    static auto make(std::filesystem::path path, std::vector<io::MappedFile> &cache,
        int frequency, int default_length)
        -> tl::expected<std::unique_ptr<FormatInterface>, int>;
};

inline std::unique_ptr<FormatInterface> make_default_format() { return std::make_unique<Default>(); }

auto read_file(const io::MappedFile &file, std::vector<io::MappedFile> &cache, int frequency, int default_length)
    -> tl::expected<std::unique_ptr<FormatInterface>, Error>;

} // namespace gmplayer
