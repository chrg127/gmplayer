#include "format.hpp"
#include <cstring>
#include "gme/gme.h"

namespace gmplayer {

namespace {

struct GMEErrorCategory : public std::error_category {
    ~GMEErrorCategory() {}
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

static GMEErrorCategory errcat;

} // namespace

Error::Error(ErrType e, std::string_view s)
    : code{static_cast<int>(e), errcat}, details{s}
{ }

GME::~GME()
{
    if (emu) {
        gme_delete(emu);
        emu = nullptr;
    }
}

Error GME::open(std::span<const u8> data, int frequency)
{
    auto type_str = gme_identify_header(data.data());
    if (strcmp(type_str, "") == 0)
        return Error(ErrType::Header, "invalid header");
    auto type = gme_identify_extension(type_str);
    emu = gme_new_emu_multi_channel(type, frequency);
    if (!emu)
        return Error(ErrType::LoadFile, "out of memory");
    auto err = gme_load_data(this->emu, data.data(), data.size());
    if (err)
        return Error(ErrType::LoadFile, err);
    return Error{};
}

Error GME::load_m3u(std::filesystem::path path)
{
    auto err = gme_load_m3u(emu, path.string().c_str());
    return err ? Error(ErrType::LoadM3U, err) : Error();
}

Error GME::start_track(int n)
{
    auto err = gme_start_track(emu, n);
    return err ? Error(ErrType::LoadTrack, err) : Error();
}

Error GME::play(std::span<i16> out)
{
    auto err = gme_play(emu, out.size(), out.data());
    return err ? Error(ErrType::Play, err) : Error{};
}

Error GME::seek(int n)
{
    auto err = gme_seek(emu, n);
    if (err)
        return Error(ErrType::Seek, err);
    // fade disappears on seek for some reason
    set_fade(fade_from, fade_len);
    return Error();
}

int GME::position() const
{
    return gme_tell(emu);
}

int GME::track_count() const
{
    return gme_track_count(emu);
}

Metadata GME::track_metadata(int which, int default_length)
{
    gme_info_t *info;
    gme_track_info(emu, &info, which);
    track_len = info->length      > 0 ? info->length
              : info->loop_length > 0 ? info->intro_length + info->loop_length * 2
              : default_length;
    auto data = Metadata {
        .length    = track_len,
        .info = {
            info->system,
            info->game,
            info->song[0] ? std::string(info->song)
                          : std::string("Track ") + std::to_string(which + 1),
            info->author,
            info->copyright,
            info->comment,
            info->dumper,
        }
    };
    gme_free_info(info);
    return data;
}

bool GME::track_ended() const
{
    // some songs don't have length information, hence the need for the second check.
    return gme_track_ended(emu) || gme_tell(emu) > track_len + fade_len;
}

int GME::channel_count() const
{
    return gme_voice_count(emu);
}

std::string GME::channel_name(int index) const
{
    return gme_voice_name(emu, index);
}

void GME::mute_channel(int index, bool mute)
{
    gme_mute_voice(emu, index, mute);
}

void GME::set_fade(int from, int length)
{
    fade_from = from;
    fade_len  = length;
    if (length != 0)
        gme_set_fade(emu, fade_from, fade_len);
}

void GME::set_tempo(double tempo)
{
    gme_set_tempo(emu, tempo);
}

bool GME::multi_channel() const
{
    return gme_multi_channel(emu);
}

auto read_file(const io::MappedFile &file, int frequency)
    -> tl::expected<std::unique_ptr<Interface>, Error>
{
    auto ptr = std::make_unique<GME>();
    if (auto err = ptr->open(file.bytes(), frequency); err)
        return tl::unexpected(err);
    return ptr;
}

} // namespace gmplayer
