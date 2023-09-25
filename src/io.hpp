/*
 * This is a library that provides File classes. It is meant to be a thin
 * wrapper over stdio.h, as well replacing iostreams.
 *
 * There are two file classes provided: a File class, which is a simple wrapper
 * over a FILE *, and a MappedFile class, which is a wrapper over an memory
 * mapped file.
 *
 * There is also some additional stuff, such as a function that directly reads
 * a file and returns a string, and functions for getting standard paths.
 *
 * More specific documentation can be found below.
 */

#pragma once

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tl/expected.hpp>
#include "common.hpp"

namespace io {

/* A file can be opened in one of four ways: */
enum class Access { Read, Write, Modify, Append, };

/* A type returned when opening a file. */
template <typename T>
using Result = tl::expected<T, std::error_code>;

namespace detail {
    inline void file_deleter(FILE *fp)
    {
        if (fp && fp != stdin && fp != stdout && fp != stderr)
            std::fclose(fp);
    }

    inline std::error_code make_error(int ec = errno)
    {
        return std::error_code(ec, std::system_category());
    }

    Result<std::pair<u8 *, std::size_t>> open_mapped_file(std::filesystem::path path, Access access);
    int close_mapped_file(u8 *ptr, std::size_t len);
} // namespace detail

/*
 * This is a RAII-style thin wrapper over a FILE *. It will automatically close
 * the file when destroyed, unless it is one of stdin, stdout, stderr.
 *
 * @open: a static method for opening Files;
 * @assoc: used to associate an existing FILE * with a File object. Mostly
 *         useful for stdin, stdout and stderr. Using assoc(), the filename
 *         will always be set to '/';
 * @get_word: gets a single word from the File and returns it as a string. A
 *            word is delimited by any kind of space;
 * @get_line: same as above, but for lines. The line delimiter is a single
 *            newline;
 * @filename: returns the file's name, which is NOT the entire file path;
 * @file_path: returns the file's entire path as a path object;
 * @data: returns the underlying FILE *;
 * @getc: corresponds to std::fgetc;
 * @ungetc: corresponds to std::ungetc;
 * @close: manually closes the file. To be used if you care about the return value;
 */
class File {
    std::unique_ptr<FILE, void (*)(FILE *)> file_ptr = { nullptr, detail::file_deleter };
    std::filesystem::path filepath;

    File(FILE *f, std::filesystem::path p)
        : file_ptr{f, detail::file_deleter}, filepath{std::move(p)}
    { }

public:
    static Result<File> open(std::filesystem::path pathname, Access access)
    {
        FILE *fp = [&](const std::string &name) -> FILE * {
            switch (access) {
            case Access::Read:   return fopen(name.c_str(), "rb"); break;
            case Access::Write:  return fopen(name.c_str(), "wb"); break;
            case Access::Modify: return fopen(name.c_str(), "rb+"); break;
            case Access::Append: return fopen(name.c_str(), "ab"); break;
            default:             return nullptr;
            }
        }(pathname.string());
        if (!fp)
            return tl::unexpected{detail::make_error()};
        return File{fp, pathname};
    }

    static File assoc(FILE *fp) { return {fp, std::filesystem::path("/")}; }

    bool get_word(std::string &str)
    {
        auto is_delim = [](int c) { return c == ' '  || c == '\t' || c == '\r' || c == '\n'; };
        auto is_space = [](int c) { return c == ' '  || c == '\t' || c == '\r'; };
        str.erase();
        int c;
        while (c = getc(), is_space(c) && c != EOF)
            ;
        ungetc(c);
        while (c = getc(), !is_delim(c) && c != EOF)
            str += c;
        ungetc(c);
        return !(c == EOF);
    }

    bool get_line(std::string &str, int delim = '\n')
    {
        str.erase();
        int c;
        while (c = getc(), c != delim && c != EOF)
            str += c;
        return !(c == EOF);
    }

    std::string           name() const noexcept { return filepath.filename().string(); }
    std::filesystem::path path() const noexcept { return filepath; }
    FILE *                data() const noexcept { return file_ptr.get(); }
    int                   getc()                { return std::fgetc(file_ptr.get()); }
    int                   ungetc(int c)         { return std::ungetc(c, file_ptr.get()); }
    int                   close()               { return std::fclose(file_ptr.release()); }
};

/*
 * This is a RAII-style wrapper over a memory mapped file. It is worth
 * mentioning that, just like an std::unique_ptr, a MappedFile object is only
 * movable.
 * Because the data underneath is essentially an array, the class also provides
 * a small iterator inteface. Only the methods not belonging to this interface
 * will be described.
 *
 * @open: a static method for opening MappedFiles, same as File::open;
 * @slice: returns a slice, i.e. a part of the file's contents;
 * @filename and @file_path: return, respectively, the file's name and file's
 *                           path, just like in File;
 */
class MappedFile {
    u8 *ptr = nullptr;
    std::size_t len = 0;
    std::filesystem::path filepath;

    MappedFile(u8 *p, std::size_t s, std::filesystem::path pa)
        : ptr{p}, len{s}, filepath{pa}
    { }

public:
    ~MappedFile() { detail::close_mapped_file(ptr, len); }

    MappedFile(const MappedFile &) = delete;
    MappedFile & operator=(const MappedFile &) = delete;
    MappedFile(MappedFile &&m) noexcept { operator=(std::move(m)); }
    MappedFile & operator=(MappedFile &&m) noexcept
    {
        std::swap(ptr, m.ptr);
        std::swap(len, m.len);
        std::swap(filepath, m.filepath);
        return *this;
    }

    static Result<MappedFile> open(std::filesystem::path path, Access access)
    {
        auto v = detail::open_mapped_file(path, access);
        if (!v)
            return tl::unexpected(v.error());
        return MappedFile(v.value().first, v.value().second, path);
    }

    int close() { auto r = detail::close_mapped_file(ptr, len); ptr = nullptr; len = 0; return r; }

    using value_type      = u8;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference       =       value_type &;
    using const_reference = const value_type &;
    using pointer         =       value_type *;
    using const_pointer   = const value_type *;
    using iterator        =       value_type *;
    using const_iterator  = const value_type *;

    reference               operator[](size_type pos)                               { return ptr[pos]; }
    const_reference         operator[](size_type pos)                const          { return ptr[pos]; }
    u8 *                    data()                                         noexcept { return ptr; }
    const u8 *              data()                                   const noexcept { return ptr; }
    iterator                begin()                                        noexcept { return ptr; }
    const_iterator          begin()                                  const noexcept { return ptr; }
    iterator                end()                                          noexcept { return ptr + len; }
    const_iterator          end()                                    const noexcept { return ptr + len; }
    size_type               size()                                   const noexcept { return len; }
    std::span<u8>           slice(size_type start, size_type length)                { return { ptr + start, length}; }
    std::span<const u8>     slice(size_type start, size_type length) const          { return { ptr + start, length}; }
    std::span<u8>           bytes()                                                 { return { ptr, len }; }
    std::span<const u8>     bytes()                                  const          { return { ptr, len }; }
    std::string             name()                                   const noexcept { return filepath.filename().string(); }
    std::filesystem::path   path()                                   const noexcept { return filepath; }
};

/*
 * Reads an entire file into an std::string. Useful if you just want to read
 * all of a file's contents at once, without needing to create a File object.
 */
inline Result<std::string> read_file(std::filesystem::path path)
{
    FILE *file = fopen(path.string().c_str(), "rb");
    if (!file)
        return tl::unexpected(detail::make_error());
    fseek(file, 0l, SEEK_END);
    long size = ftell(file);
    rewind(file);
    std::string buf(size, ' ');
    size_t bytes_read = fread(buf.data(), sizeof(char), size, file);
    if (bytes_read < std::size_t(size))
        return tl::unexpected(detail::make_error());
    fclose(file);
    return buf;
}

namespace directory {

std::filesystem::path home();
std::filesystem::path config();
std::filesystem::path data();
std::filesystem::path applications();

} // namespace directory

} // namespace io
