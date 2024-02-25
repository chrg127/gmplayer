#include "format.hpp"

#include <memory>
#include <vector>
#include <fmt/core.h>
#include "gsf.h"
#include "io.hpp"
#include "tl/expected.hpp"
#include "audio.hpp"
#include "fs.hpp"

namespace gmplayer {

auto GSF::make(fs::path path, std::vector<io::MappedFile> &cache,
    int frequency, int default_length)
    -> tl::expected<std::unique_ptr<FormatInterface>, int>
{
    GsfEmu *emu;
    if (auto err = gsf_new(&emu, frequency, 0); err.code != 0)
        return tl::unexpected(err.code);
    auto reader = GsfReader {
        .read = [] (const char *pathname, void *userdata, const GsfAllocators *) -> GsfReadResult {
            std::vector<io::MappedFile> *cache = (std::vector<io::MappedFile> *) userdata;
            auto path = fs::path{pathname};
            auto it = std::find_if(cache->begin(), cache->end(), [&](const auto &f) {
                return (f.path() == path);
            });
            if (it != cache->end()) {
                return {
                    .buf  = it->bytes().data(),
                    .size = static_cast<long>(it->size()),
                    .err  = { .code = 0, .from = 0 }
                };
            }
            auto f = io::MappedFile::open(path, io::Access::Read);
            if (!f)
                return {
                    .buf  = nullptr,
                    .size = 0,
                    .err  = { .code = f.error().value(), .from = 0 }
                };
            cache->push_back(std::move(f.value()));
            return {
                .buf  = cache->back().bytes().data(),
                .size = static_cast<long>(cache->back().size()),
                .err  = { .code = 0, .from = 0 },
            };
        },
        .delete_data = [] (unsigned char *, long, void *, const GsfAllocators *) {},
        .userdata = static_cast<void *>(&cache),
    };
    if (auto err = gsf_load_file_with_reader(emu, path.string().c_str(), &reader); err.code != 0)
        return tl::unexpected(err.code);
    gsf_set_default_length(emu, default_length);
    gsf_set_infinite(emu, true);
    return std::make_unique<GSF>(emu);
}

GSF::~GSF()
{
    gsf_delete(emu);
}

Error GSF::start_track(int) { return Error{}; }

Error GSF::play(std::span<i16> out)
{
    gsf_play(emu, out.data(), out.size());
    auto num_samples = gsf_tell_samples(emu);
    if (fade_in.is_set() && num_samples <= fade_in.get_start() + fade_in.length())
        fade_in.put_in(out, num_samples);
    if (fade_out.is_set() && num_samples >= fade_out.get_start())
        fade_out.put_in(out, num_samples);
    return Error{};
}

Error GSF::seek(int n)
{
    gsf_seek(emu, n);
    return Error{};
}

void GSF::mute_channel(int index, bool mute)
{

}

void GSF::set_fade_out(int length)
{
    fade_out = Fade(Fade::Type::Out, gsf_length(emu), length, gsf_sample_rate(emu), gsf_num_channels(emu));
}

void GSF::set_fade_in(int length)
{
    fade_in = Fade(Fade::Type::In, 0, length, gsf_sample_rate(emu), gsf_num_channels(emu));
}

void GSF::set_tempo(double tempo)
{

}

int GSF::position() const
{
    return static_cast<int>(gsf_tell(emu));
}

int GSF::track_count() const
{
    return 1;
}

Metadata GSF::track_metadata() const
{
    GsfTags *tags;
    gsf_get_tags(emu, &tags);
    auto len = samples_to_millis(fade_out.length(), gsf_sample_rate(emu), gsf_num_channels(emu));
    Metadata metadata = {
        .length = static_cast<int>(gsf_length(emu) + len),
        .info = {
            "Game Boy Advance",
            tags->game,
            tags->title,
            tags->artist,
            tags->copyright,
            "",
            tags->gsfby,
        }
    };
    gsf_free_tags(tags);
    return metadata;
}

Metadata GSF::track_metadata(int which) const
{
    return track_metadata();
}

bool GSF::track_ended() const
{
    return gsf_tell(emu) > gsf_length(emu) + fade_out.length();
}

int GSF::channel_count() const
{
    return 1;
}

std::string GSF::channel_name(int index) const
{
    return "GBA";
}

bool GSF::is_multi_channel() const
{
    return false;
}

} // namespace gmplayer
