#pragma once

#include <cstddef>
#include <cstdint>

using uint = unsigned;
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;
using intptr = intptr_t;
using uintptr = uintptr_t;

enum class Compiler : uint { Clang, GCC, MSVC, Unknown };
enum class Platform : uint { Windows, MacOS, Linux, Unknown };

#ifdef __clang__
#   define COMPILER_CLANG
    static inline constexpr Compiler compiler() { return Compiler::Clang; }
#elif defined(__GNUC__)
#   define COMPILER_GCC
    static inline constexpr Compiler compiler() { return Compiler::GCC; }
#elif defined(_MSC_VER)
#   define COMPILER_MICROSOFT
    static inline constexpr Compiler compiler() { return Compiler::MSVC; }
#else
#   warning "unable to detect compiler"
#   define COMPILER_UNKNOWN
    static inline constexpr Compiler compiler() { return Compiler::Unknown; }
#endif

#ifdef _WIN32
#   define PLATFORM_WINDOWS
    static inline constexpr Platform platform() { return Platform::Windows; }
#elif defined(__APPLE__)
#   define PLATFORM_MACOS
    static inline constexpr Platform platform() { return Platform::MacOS; }
#elif defined(linux) || defined(__linux__)
#   define PLATFORM_LINUX
    static inline constexpr Platform platform() { return Platform::Linux; }
#else
#   define PLATFORM_UNKNOWN
#   warning "unable to detect platform"
    static inline constexpr Platform platform() { return Platform::Unknown; }
#endif

#if defined(COMPILER_CLANG) || defined(COMPILER_GCC)
  #define noinline   __attribute__((noinline))
  #define alwaysinline  inline __attribute__((always_inline))
#elif defined(COMPILER_MICROSOFT)
  #define noinline   __declspec(noinline)
  #define alwaysinline  inline __forceinline
#else
  #define noinline
  #define alwaysinline  inline
#endif

#define FWD(x) std::forward<decltype(x)>(x)

#define DEFINE_OPTION_ENUM(name, ...)       \
enum class name : std::uint32_t {           \
    None = 0x0,                             \
    __VA_ARGS__                             \
};                                          \
                                            \
inline name operator|(name a, name b) {     \
    return static_cast<name>(               \
        static_cast<u32>(a)                 \
      | static_cast<u32>(b)                 \
    );                                      \
}                                           \
                                            \
inline name operator&(name a, name b) {     \
    return static_cast<name>(               \
        static_cast<u32>(a)                 \
      & static_cast<u32>(b)                 \
    );                                      \
}                                           \

