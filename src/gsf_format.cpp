#include "format.hpp"

#include <memory>
#include <vector>
#include "gsf.h"
#include "io.hpp"
#include "tl/expected.hpp"
#include "types.hpp"
#include "fs.hpp"

namespace gmplayer {

auto GSF::make(fs::path path, std::vector<io::MappedFile> &cache,
    int frequency, int default_length)
    -> tl::expected<std::unique_ptr<FormatInterface>, int>
{
    auto read_file = [] (void *userdata, const char *pathname, unsigned char **buf, long *size) {
        std::vector<io::MappedFile> *cache = (std::vector<io::MappedFile> *) userdata;
        auto path = fs::path{pathname};
        auto it = std::find_if(cache->begin(), cache->end(), [&](const auto &f) {
            return (f.path() == path);
        });
        if (it != cache->end()) {
            *buf = it->bytes().data();
            *size = it->size();
            return 0;
        }
        auto f = io::MappedFile::open(path, io::Access::Read);
        if (!f)
            return 1;
        cache->push_back(std::move(f.value()));
        *buf = cache->back().bytes().data();
        *size = cache->back().size();
        return 0;
    };
    auto delete_file = [] (unsigned char *buf) {};

    GsfEmu *emu;
    if (auto new_err = gsf_new(&emu, frequency, 0); new_err != 0)
        return tl::unexpected(new_err);
    gsf_set_default_length(emu, default_length);
    if (auto load_err = gsf_load_file_custom(emu, path.string().c_str(), &cache, read_file, delete_file);
        load_err != 0)
        return tl::unexpected(load_err);
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

void GSF::set_fade(int from, int length)
{

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
    Metadata metadata = {
        .length = static_cast<int>(gsf_length(emu)),
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
    return gsf_ended(emu);
}

int GSF::channel_count() const
{
    return 1;
}

std::string GSF::channel_name(int index) const
{
    return "";
}

bool GSF::is_multi_channel() const
{
    return false;
}

} // namespace gmplayer const
