#include "format.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include "gme/gme.h"
#include "io.hpp"

namespace gmplayer {

namespace {
    int get_length(gme_info_t *info, int default_length)
    {
        return info->length      > 0 ? info->length
             : info->loop_length > 0 ? info->intro_length + info->loop_length * 2
             : default_length;
    }
} // namespace

auto read_file(const io::MappedFile &file, int frequency, int default_length)
    -> tl::expected<std::unique_ptr<FormatInterface>, Error>
{
    if (auto gme = GME::make(file, frequency, default_length); gme)
        return gme;
    return tl::unexpected(Error{ErrType::LoadFile, "no suitable interface found"});
}

GME::~GME()
{
    if (emu) {
        gme_delete(emu);
        emu = nullptr;
    }
}

auto GME::make(const io::MappedFile &file, int frequency, int default_length)
    -> tl::expected<std::unique_ptr<FormatInterface>, Error>
{
    auto data = file.bytes();
    auto type_str = gme_identify_header(data.data());
    if (strcmp(type_str, "") == 0)
        return tl::unexpected(Error(ErrType::Header, "invalid header"));
    auto emu = gme_new_emu_multi_channel(gme_identify_extension(type_str), frequency);
    if (!emu)
        return tl::unexpected(Error(ErrType::LoadFile, "out of memory"));
    if (auto err = gme_load_data(emu, data.data(), data.size()); err)
        return tl::unexpected(Error(ErrType::LoadFile, err));
    // load m3u file automatically. we don't care if it's found or not.
    if (auto err = gme_load_m3u(emu, file.path().replace_extension("m3u").c_str()); err) {
#ifdef DEBUG
        printf("GME: %s\n", err);
#endif
    }
    return std::make_unique<GME>(emu, default_length);
}

Error GME::start_track(int which)
{
    auto err = gme_start_track(emu, which);
    gme_info_t *info;
    gme_track_info(emu, &info, which);
    track_len = get_length(info, default_length);
    gme_free_info(info);
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

Metadata GME::track_metadata(int which)
{
    gme_info_t *info;
    gme_track_info(emu, &info, which);
    auto data = Metadata {
        .length = get_length(info, default_length),
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

bool GME::is_multi_channel() const
{
    return gme_multi_channel(emu);
}

} // namespace gmplayer
