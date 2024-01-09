#include "format.hpp"

#include "io.hpp"

namespace gmplayer {

// io::MappedFile &get_from_cache(const std::vector<io::MappedFile> &cache)
// {
//     auto it = std::find_if(cache.begin(), cache.end(), [&](const auto &f) {
//         return (f.path() == path);
//     });
//     if (it != cache->end())
//         return *it;
//     auto f = io::MappedFile::open(path, io::Access::Read);
//     cache.push_back(f.value());
//     return cache.back();
// }

auto read_file(const io::MappedFile &file, std::vector<io::MappedFile> &cache, int frequency, int default_length)
    -> tl::expected<std::unique_ptr<FormatInterface>, Error>
{
    if (auto gsf = GSF::make(file.path(), cache, frequency, default_length); gsf)
        return std::move(gsf.value());
    if (auto gme = GME::make(file, frequency, default_length); gme)
        return std::move(gme.value());
    return tl::unexpected<Error>({
        .code = Error::Type::LoadFile,
        .details = "no suitable interface found",
        .file_path = file.path(),
        .track_name = "",
    });
}

// auto read_file(const io::MappedFile &file, int frequency, int default_length)
//     -> tl::expected<std::unique_ptr<FormatInterface>, Error>
// {
//     if (auto gme = GME::make(file, frequency, default_length); gme)
//         return std::move(gme.value());
//     return tl::unexpected<Error>({
//         .code = Error::Type::LoadFile,
//         .details = "no suitable interface found",
//         .file_path = file.path(),
//         .track_name = "",
//     });
// }

} // namespace gmplayer
