#include "io.hpp"

#include <tuple>
#ifdef PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    #include <fcntl.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <unistd.h>
#else
    #error "io.hpp: platform not supported"
#endif

namespace io {

namespace detail {

#ifdef PLATFORM_WINDOWS

std::tuple<int, int, int, int> get_flags(Access access)
{
    switch (access) {
    case Access::Read:   return { GENERIC_READ,                 OPEN_EXISTING, PAGE_READONLY,  FILE_MAP_READ };
    case Access::Write:  return { GENERIC_WRITE,                CREATE_ALWAYS, PAGE_READWRITE, FILE_MAP_ALL_ACCESS };
    case Access::Modify: return { GENERIC_READ | GENERIC_WRITE, OPEN_EXISTING, PAGE_READWRITE, FILE_MAP_ALL_ACCESS };
    case Access::Append: return { GENERIC_READ | GENERIC_WRITE, CREATE_NEW,    PAGE_READWRITE, FILE_MAP_ALL_ACCESS };
    default: return {0, 0};
    }
}

std::pair<u8 *, std::size_t> open_mapped_file(std::filesystem::path path, Access access)
{
    auto [desired_access, creation_disposition, protection, map_access] = get_flags(access);
    int desired_access      = GENERIC_READ | GENERIC_WRITE;
    int creation_disposition = OPEN_EXISTING;
    int protection          = PAGE_READWRITE;
    int map_access           = FILE_MAP_ALL_ACCESS;
    HANDLE file = CreateFileW(path.c_str(), desiredAccess, FILE_SHARE_READ,
        nullptr, creation_disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return {nullptr, 0};
    HANDLE map = CreateFileMapping(file, nullptr, protection, 0, size, nullptr);
    if (map == INVALID_HANDLE_VALUE) {
        CloseHandle(file);
        return {nullptr, 0};
    }
    u8 *data = (u8 *) MapViewOfFile(map, map_access, 0, 0, size);
    CloseHandle(map);
    CloseHandle(file);
    return { data, size };
}

void close_mapped_file(u8 *ptr, std::size_t len)
{
    if (ptr)
        UnmapViewOfFile(ptr);
}

#else

std::pair<int, int> get_flags(Access access)
{
    switch (access) {
    case Access::Read:   return { O_RDONLY,         PROT_READ };
    case Access::Write:  return { O_RDWR | O_CREAT, PROT_WRITE };
    case Access::Modify: return { O_RDWR,           PROT_READ | PROT_WRITE };
    case Access::Append: return { O_RDWR | O_CREAT, PROT_READ | PROT_WRITE };
    default: return {0, 0};
    }
}

std::pair<u8 *, std::size_t> open_mapped_file(std::filesystem::path path, Access access)
{
    auto [open_flags, mmap_flags] = get_flags(access);
    int fd = ::open(path.c_str(), open_flags);
    if (fd < 0)
        return {nullptr, 0};
    struct stat statbuf;
    int err = fstat(fd, &statbuf);
    if (err < 0)
        return {nullptr, 0};
    auto *ptr = (u8 *) mmap(nullptr, statbuf.st_size, mmap_flags, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED)
        return {nullptr, 0};
    close(fd);
    return {ptr, static_cast<std::size_t>(statbuf.st_size)};
}

void close_mapped_file(u8 *ptr, std::size_t len)
{
    if (ptr) ::munmap(ptr, len);
}

} // namespace detail

} // namespace io

#endif
