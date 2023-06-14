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
    #define COMPILER_CLANG
    static constexpr inline Compiler compiler() { return Compiler::Clang; }
#elif defined(__GNUC__)
    #define COMPILER_GCC
    static constexpr inline Compiler compiler() { return Compiler::GCC; }
#elif defined(_MSC_VER)
    #define COMPILER_MSVC
    static constexpr inline Compiler compiler() { return Compiler::MSVC; }
#else
    #warning "unable to detect compiler"
    #define COMPILER_UNKNOWN
    static constexpr inline Compiler compiler() { return Compiler::Unknown; }
#endif

#ifdef _WIN32
    #define PLATFORM_WINDOWS
    static inline constexpr Platform platform() { return Platform::Windows; }
#elif defined(__APPLE__)
    #define PLATFORM_MACOS
    static inline constexpr Platform platform() { return Platform::MacOS; }
#elif defined(linux) || defined(__linux__)
    #define PLATFORM_LINUX
    static inline constexpr Platform platform() { return Platform::Linux; }
#else
    #define PLATFORM_UNKNOWN
    #warning "unable to detect platform"
    static inline constexpr Platform platform() { return Platform::Unknown; }
#endif

#if defined(COMPILER_CLANG) || defined(COMPILER_GCC)
  #define noinline   __attribute__((noinline))
  #define alwaysinline  inline __attribute__((always_inline))
#elif defined(COMPILER_MSVC)
  #define noinline   __declspec(noinline)
  #define alwaysinline  inline __forceinline
#else
  #define noinline
  #define alwaysinline  inline
#endif

#define FWD(x) std::forward<decltype(x)>(x)

// A better assert macro. Should trigger a quick break on any debugger.
#ifdef DEBUG
    #if __GNUC__
        #define ASSERT(c) if (!(c)) __builtin_trap()
    #elif _MSC_VER
        #define ASSERT(c) if (!(c)) __debugbreak()
    #else
        #define ASSERT(c) if (!(c)) *(volatile int *)0 = 0
    #endif
#else
    #define ASSERT(c)
#endif

[[noreturn]] inline void unreachable()
{
    // Uses compiler specific extensions if possible.
    // Even if no extension is used, undefined behavior is still raised by
    // an empty function body and the noreturn attribute.
#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
    __builtin_unreachable();
#elif defined(COMPILER_MSVC)
    __assume(false);
#endif
}
