#include "format.hpp"
#include <cstring>
#include "gme/gme.h"

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
    auto err = gme_open_data(data.data(), data.size(), &emu, frequency);
    return err ? Error(ErrType::LoadFile, err) : Error();
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

PlayResult GME::play()
{
    std::array<short, SAMPLES * CHANNELS> buf;
    auto err = gme_play(emu, SAMPLES * CHANNELS, buf.data());
    if (err)
        return tl::unexpected(Error(ErrType::Play, err));
    std::array<u8, NUM_SAMPLES> bytes;
    std::memcpy(bytes.data(), buf.data(), bytes.size());
    return bytes;
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

Metadata GME::track_metadata(int which) const
{
    gme_info_t *info;
    gme_track_info(emu, &info, which);
    auto data = Metadata {
        .length    = info->length      > 0 ? info->length
                   : info->loop_length > 0 ? info->intro_length + info->loop_length * 2
                   : -1,
        .system    = info->system,
        .game      = info->game,
        .song      = info->song,
        .author    = info->author,
        .copyright = info->copyright,
        .comment   = info->comment,
        .dumper    = info->dumper,
    };
    gme_free_info(info);
    return data;
}

bool GME::track_ended() const
{
    return gme_track_ended(emu);
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

void GME::ignore_silence(bool ignore)
{
    gme_ignore_silence(emu, ignore);
}

auto read_file(const io::MappedFile &file, int frequency)
    -> tl::expected<std::unique_ptr<Interface>, Error>
{
    gme_type_t type;
    if (auto err = gme_identify_file(file.filename().c_str(), &type); type == nullptr)
        return tl::unexpected(Error(ErrType::FileType, err));
    if (auto header = gme_identify_header(file.data()); header[0] == '\0')
        return tl::unexpected(Error(ErrType::Header, ""));
    auto ptr = std::make_unique<GME>();
    ptr->open(file.bytes(), frequency);
    return ptr;
}
