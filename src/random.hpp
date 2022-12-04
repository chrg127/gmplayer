#pragma once

#include <cstddef>
#include <concepts>
#include <numeric>
#include <random>
#include <span>
#include <vector>

namespace rng {

inline std::seed_seq make_seed()
{
    std::random_device rd;
    std::array<std::mt19937::result_type, std::mt19937::state_size> seed;
    for (auto &x : seed)
        x = rd();
    return std::seed_seq{seed.begin(), seed.end()};
}

inline thread_local auto seed = make_seed();
inline thread_local auto rng = std::mt19937(seed);

template <std::integral       T = int>   T get()             { return std::uniform_int_distribution <T>(    )(rng); }
template <std::floating_point T = float> T get()             { return std::uniform_real_distribution<T>(    )(rng); }
template <std::integral       T = int>   T between(T x, T y) { return std::uniform_int_distribution <T>(x, y)(rng); }
template <std::floating_point T = float> T between(T x, T y) { return std::uniform_real_distribution<T>(x, y)(rng); }

template <typename T> T pick(std::span<T> from)                    { return from        [between(0ul, from.size()-1)]; }
template <typename T> T pick(const std::initializer_list<T> &from) { return from.begin()[between(0ul, from.size()-1)]; }

} // namespace random
