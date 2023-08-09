/*
 * The missing string function from the standard library.
 *
 * This library implements the following functions:
 *
 *     - split(): you know this one. Uses a single character delimiter and
 *       returns an std::vector.
 *     - split_lines(): splits a single string in multiple lines, using a
 *       number that describes the maximum column of each line. It tries
 *       doing the operation using words;
 *     - trim(): you also know this one;
 *     - trim_in_place(): same as above, but in place;
 *     - to_number(): converts a string to number, returns an std::optional
 *       instead of usig clunky error handling mechanisms;
 *
 * All these functions work on both std::string and std::string_view (and
 * probably other string types too). Some of these (split(), trim()) have a
 * version for std::string_view (split_view(), trim_view()) as helpers
 * (otherwise you'd have to specify those in template parameters.
 */

#pragma once

#include <algorithm>
#include <cstring>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace string {

inline bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r'; }
inline bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

template <typename From = std::string, typename To = std::string>
inline std::vector<To> split(const From &s, char delim = ',')
{
    std::vector<To> res;
    for (std::size_t i = 0, p = 0; i != s.size(); i = p+1) {
        p = s.find(delim, i);
        if (p == s.npos) {
            res.emplace_back(s.substr(i, s.size() - i));
            break;
        }
        res.emplace_back(s.substr(i, p-i));
        i = p+1;
    }
    return res;
}

inline std::vector<std::string_view> split_view(std::string_view s, char delim = ',')
{
    return split<std::string_view, std::string_view>(s, delim);
}

template <typename From = std::string, typename To = std::string>
inline std::vector<To> split_lines(const From &s, std::size_t col)
{
    std::vector<To> result;
    auto it = s.begin();
    while (it != s.end()) {
        it = std::find_if_not(it, s.end(), is_space);
        auto start = it;
        it += std::min(size_t(s.end() - it), size_t(col));
        it = std::find_if(it, s.end(), is_space);
        result.emplace_back(start, it);
    }
    return result;
}

template <typename From = std::string, typename To = std::string>
inline To trim(const From &s)
{
    auto i = std::find_if_not(s.begin(),  s.end(),  is_space);
    auto j = std::find_if_not(s.rbegin(), s.rend(), is_space).base();
    return {i, j};
}

inline std::string_view trim_view(std::string_view s) { return trim<std::string_view, std::string_view>(s); }

template <typename T = std::string>
inline void trim_in_place(T &s)
{
    auto j = std::find_if_not(s.rbegin(), s.rend(), is_space).base();
    s.erase(j, s.end());
    auto i = std::find_if_not(s.begin(), s.end(), is_space);
    s.erase(s.begin(), i);
}

template <typename T = int, typename TStr = std::string>
inline std::optional<T> to_number(const TStr &str, unsigned base = 10)
    requires std::is_integral_v<T> || std::is_floating_point_v<T>
{
    auto helper = [](const char *start, const char *end, unsigned base) -> std::optional<T> {
        T value = 0;
        std::from_chars_result res;
        if constexpr(std::is_floating_point_v<T>)
            res = std::from_chars(start, end, value);
        else
            res = std::from_chars(start, end, value, base);
        if (res.ec != std::errc() || res.ptr != end)
            return std::nullopt;
        return value;
    };
    if constexpr(std::is_same<std::decay_t<TStr>, char *>::value)
        return helper(str, str + std::strlen(str), base);
    else
        return helper(str.data(), str.data() + str.size(), base);
}

} // namespace string
