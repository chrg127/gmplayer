/*
 * This is a small random library. It offers a thread-safe generator portable
 * to any platform (I.E. generates the same sequences everywhere). It also
 * offers some helpful functions.
 */

#pragma once

#include <cstddef>
#include <concepts>
#include <array>
#include <bit>
#include <numeric>
#include <random>
#include <span>
#include <vector>
#include "common.hpp"

namespace rng {

// https://prng.di.unimi.it
constexpr inline u64 xoshiro256starstar(u64 s[4]) {
	const u64 result = std::rotl(s[1] * 5, 7) * 9;
	const u64 t = s[1] << 17;
	s[2] ^= s[0];
	s[3] ^= s[1];
	s[1] ^= s[2];
	s[0] ^= s[3];
	s[2] ^= t;
	s[3] = std::rotl(s[3], 45);
	return result;
}

struct Generator {
    using result_type = u64;
    static const size_t state_size = 4;

private:
    std::array<result_type, state_size> seed;
    std::array<result_type, state_size> state;

public:
    explicit Generator(std::array<result_type, state_size> s) : seed(s), state(s) { }
    result_type next()                 { return xoshiro256starstar(state.data()); }
    static constexpr result_type min() { return std::numeric_limits<result_type>::min(); }
    static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }
    result_type operator()()           { return this->next(); }
};

inline auto make_seed()
{
    std::random_device rd;
    std::array<Generator::result_type, Generator::state_size> seed;
    for (auto &x : seed)
        x = rd();
    return seed;
}

/*
 * @seed: the seed generated at program/thread start-up. Note this is actually
 *        an array, not a single value. A seed value is generated for every
 *        thread.
 * @rng: the actual generator. The generator itself is based on standard
 *       <random> generators. A copy of the generator exists for every thread.
 */
inline thread_local const auto seed = make_seed();
inline thread_local auto rng = Generator(seed);

/* Helpful function for extracting values from the generator. */
template <std::integral       T = int>   T get()             { return std::uniform_int_distribution <T>(    )(rng); }
template <std::floating_point T = float> T get()             { return std::uniform_real_distribution<T>(    )(rng); }
template <std::integral       T = int>   T between(T x, T y) { return std::uniform_int_distribution <T>(x, y)(rng); }
template <std::floating_point T = float> T between(T x, T y) { return std::uniform_real_distribution<T>(x, y)(rng); }

/* Randomly picks an element from a list of elements. */
template <typename T> T pick(std::span<T> from)                    { return from        [between(0ul, from.size()-1)]; }
template <typename T> T pick(const std::initializer_list<T> &from) { return from.begin()[between(0ul, from.size()-1)]; }

} // namespace random
