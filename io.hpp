#pragma once

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <optional>
#include "common.hpp"

#if defined(PLATFORM_LINUX)
#   include <fcntl.h>
#   include <sys/mman.h>
#   include <sys/stat.h>
#   include <unistd.h>
#else
#   warning "platform not supported"
#endif

namespace io {

enum class Access { Read, Write, Modify, Append, };

class File {
    FILE *fp = nullptr;
    std::string name;

    File(FILE *f, std::string &&s) : fp{f}, name{std::move(s)} {}

public:
    ~File()
    {
        if (!fp || fp == stdin || fp == stdout || fp == stderr)
            return;
        std::fclose(fp);
        fp = nullptr;
        name.erase();
    }

    File(File &&f) noexcept { operator=(std::move(f)); }
    File & operator=(File &&f) noexcept
    {
        std::swap(fp, f.fp);
        std::swap(name, f.name);
        return *this;
    }

    static std::optional<File> open(std::string_view pathname, Access access)
    {
        FILE *fp = nullptr;
        switch (access) {
        case Access::Read:   fp = fopen(pathname.data(), "rb"); break;
        case Access::Write:  fp = fopen(pathname.data(), "wb"); break;
        case Access::Modify: fp = fopen(pathname.data(), "rb+"); break;
        case Access::Append: fp = fopen(pathname.data(), "ab"); break;
        }
        if (!fp)
            return std::nullopt;
        return File{ fp, std::string(pathname) };
    }

    static File assoc(FILE *fp) { return { fp, "" }; }

    bool get_word(std::string &str)
    {
        auto is_delim = [](int c) { return c == '\n' || c == ' '  || c == '\t'; };
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

    std::string filename() const noexcept { return name; }
    FILE *data() const noexcept           { return fp; }
    int getc()                            { return std::fgetc(fp); }
    int ungetc(int c)                     { return std::ungetc(c, fp); }
};

class MappedFile {
    u8 *ptr = nullptr;
    std::size_t len = 0;
    std::string name;

    MappedFile(u8 *p, std::size_t s, std::string_view n)
        : ptr(p), len(s), name(n)
    { }

public:
    ~MappedFile();

    MappedFile(const MappedFile &) = delete;
    MappedFile & operator=(const MappedFile &) = delete;
    MappedFile(MappedFile &&m) noexcept { operator=(std::move(m)); }
    MappedFile & operator=(MappedFile &&m) noexcept
    {
        std::swap(ptr, m.ptr);
        std::swap(len, m.len);
        std::swap(name, m.name);
        return *this;
    }

    static std::optional<MappedFile> open(std::string_view pathname);

    u8 operator[](std::size_t index) { return ptr[index]; }
    u8 *begin() const                { return ptr; }
    u8 *end() const                  { return ptr + len; }
    u8 *data() const                 { return ptr; }
    std::size_t size() const            { return len; }
    std::string filename() const        { return name; }
    std::span<u8> slice(std::size_t start, std::size_t length) { return { ptr + start, length}; }
};

} // namespace io



namespace io {

#ifdef PLATFORM_LINUX

inline std::optional<MappedFile> MappedFile::open(std::string_view pathname)
{
    int fd = ::open(pathname.data(), O_RDWR);
    if (fd < 0)
        return std::nullopt;
    struct stat statbuf;
    int err = fstat(fd, &statbuf);
    if (err < 0)
        return std::nullopt;
    auto *ptr = (u8 *) mmap(nullptr, statbuf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED)
        return std::nullopt;
    close(fd);
    return MappedFile{ptr, static_cast<std::size_t>(statbuf.st_size), pathname};
}

inline MappedFile::~MappedFile()
{
    if (ptr) ::munmap(ptr, len);
}

#endif

// reads an entire file into a buffer, skipping any file object construction.
inline std::optional<std::unique_ptr<char[]>> read_binary_file(std::string_view path)
{
    FILE *file = fopen(path.data(), "rb");
    if (!file)
        return std::nullopt;
    fseek(file, 0l, SEEK_END);
    long size = ftell(file);
    rewind(file);
    auto buf = std::make_unique<char[]>(size);
    size_t bytes_read = fread(buf.get(), sizeof(char), size, file);
    if (bytes_read < std::size_t(size))
        return std::nullopt;
    fclose(file);
    return buf;
}

// same as above, but returns it as a string
inline std::optional<std::string> read_file(std::string_view path)
{
    FILE *file = fopen(path.data(), "rb");
    if (!file)
        return std::nullopt;
    fseek(file, 0l, SEEK_END);
    long size = ftell(file);
    rewind(file);
    std::string buf(size, ' ');
    size_t bytes_read = fread(buf.data(), sizeof(char), size, file);
    if (bytes_read < std::size_t(size))
        return std::nullopt;
    fclose(file);
    return buf;
}

inline std::string user_home()
{
#ifdef PLATFORM_LINUX
    return getenv("HOME");
#endif
}

} // namespace io
