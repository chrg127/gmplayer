#pragma once

#include <concepts>

template <typename T>
concept Number = std::is_integral_v<T>
              || std::is_floating_point_v<T>;

template <typename T>
concept ContainerType = requires(T t) {
    t.data();
    t.size();
};
