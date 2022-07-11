#pragma once

#include <concept>
#include <random>
#include <span>

namespace random {

inline std::mt19937 rng(std::random_device{}());

template <typename T
T between(T x, T y)
{
    std::uniform_int_distribution<T> dist(x, y);
    return dist(rng);
}

template <typename T
void shuffle(std::span<T> arr)
{
    for (int i = 0; i < arr.size(); i++) {
        int j = random_between(i, arr.size() - 1);
        std::swap(arr[i], arr[j]);
    }
}

} // namespace random
