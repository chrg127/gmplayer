#include "io.hpp"

#include <tuple>
#ifdef PLATFORM_WINDOWS
    #define NO_MIN_MAX
    #include <initguid.h>
    #include <cguid.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
    #include <wchar.h>
    #include <shlobj.h>
    #include <shellapi.h>
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    #include <fcntl.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <unistd.h>
    #include <pwd.h>
#else
    #error "io.hpp: platform not supported"
#endif

namespace fs = std::filesystem;

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
    default: return {0, 0, 0, 0};
    }
}

Result<std::pair<u8 *, std::size_t>> open_mapped_file(std::filesystem::path path, Access access)
{
    auto [desired_access, creation_disposition, protection, map_access] = get_flags(access);
    HANDLE file = CreateFileW(path.c_str(), desired_access, FILE_SHARE_READ,
        nullptr, creation_disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return tl::unexpected(make_error(GetLastError()));
    auto size = GetFileSize(file, nullptr);
    HANDLE map = CreateFileMapping(file, nullptr, protection, 0, size, nullptr);
    if (map == INVALID_HANDLE_VALUE) {
        CloseHandle(file);
        return tl::unexpected(make_error(GetLastError()));
    }
    u8 *data = (u8 *) MapViewOfFile(map, map_access, 0, 0, size);
    CloseHandle(map);
    CloseHandle(file);
    return std::make_pair(data, size);
}

int close_mapped_file(u8 *ptr, std::size_t len)
{
    return UnmapViewOfFile(ptr);
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

Result<std::pair<u8 *, std::size_t>> open_mapped_file(std::filesystem::path path, Access access)
{
    auto [open_flags, mmap_flags] = get_flags(access);
    int fd = ::open(path.c_str(), open_flags);
    if (fd < 0)
        return tl::unexpected(make_error());
    struct stat statbuf;
    int err = fstat(fd, &statbuf);
    if (err < 0)
        return tl::unexpected(make_error());
    auto *ptr = (u8 *) mmap(nullptr, statbuf.st_size, mmap_flags, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED)
        return tl::unexpected(make_error());
    close(fd);
    return std::make_pair(ptr, static_cast<std::size_t>(statbuf.st_size));
}

int close_mapped_file(u8 *ptr, std::size_t len)
{
    return ::munmap(ptr, len);
}

#endif

} // namespace detail

namespace directory {

std::filesystem::path home()
{
#ifdef PLATFORM_WINDOWS
    wchar_t path_string[PATH_MAX] = L"";
    SHGetFolderPathW(nullptr, CSIDL_PROFILE | CSIDL_FLAG_CREATE, nullptr, 0, path_string);
    return fs::path(path_string);
#else
    auto *userinfo = getpwuid(getuid());
    return fs::path(userinfo->pw_dir);
#endif
}

std::filesystem::path config()
{
#ifdef PLATFORM_WINDOWS
    wchar_t path_string[PATH_MAX] = L"";
    SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE, nullptr, 0, path_string);
    return fs::path(path_string);
#elif defined(PLATFORM_MACOS)
    return home() / "Library/Application Support";
#else
    if (auto *env = getenv("XDG_CONFIG_HOME"))
        return fs::path(env);
    return home() / fs::path(".config");
#endif
}

std::filesystem::path data()
{
#ifdef PLATFORM_WINDOWS
    wchar_t path_string[PATH_MAX] = L"";
    SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, nullptr, 0, path_string);
    return fs::path(path_string);
#elif defined(PLATFORM_MACOS)
    return home() / "Library/Application Support";
#else
    if (auto *env = getenv("XDG_DATA_HOME"))
        return fs::path(env);
    return home() / fs::path(".local/share");
#endif
}

} // namespace directory

} // namespace io
