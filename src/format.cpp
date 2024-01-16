#include "format.hpp"

#include "io.hpp"
#include "tl/expected.hpp"
#include "fs.hpp"

namespace gmplayer {

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

} // namespace gmplayer
