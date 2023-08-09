#pragma once

#include <type_traits>
#include <bit>
#include <initializer_list>
#include "common.hpp"

template <typename T>
class Flags {
    using NumType = u64;
    NumType data = 0;
public:
    Flags() = default;
    Flags(std::initializer_list<T> values) { for (auto v : values) add(v); }
    void add(T value)                     { data |=  (1 << static_cast<NumType>(value)); }
    void remove(T value)                  { data &= ~(1 << static_cast<NumType>(value)); }
    bool contains(T value) const          { return data & (1 << static_cast<NumType>(value)); }
    int count()            const          { return std::popcount(data); }
    NumType value()        const          { return data; }
    Flags<T> & unite(Flags<T> &other)     { data |= other.data; return *this; }
    Flags<T> & intersect(Flags<T> &other) { data &= other.data; return *this; }
    explicit operator bool() const        { return data; }
};
