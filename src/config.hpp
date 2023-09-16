#pragma once

#include <unordered_map>
#include <fmt/core.h>
#include <mutex>
#include "common.hpp"
#include "const.hpp"
#include "types.hpp"
#include "conf.hpp"
#include "callback_handler.hpp"

namespace detail {

using namespace gmplayer::literals;
using namespace conf::literals;

const conf::Data defaults = {
    // player options
    { "autoplay",               conf::Value(false) },
    { "repeat_file",            conf::Value(false) },
    { "repeat_track",           conf::Value(false) },
    { "default_duration",       conf::Value(3_min) },
    { "fade",                   0_v },
    { "tempo",                  50_v },
    { "volume",                 conf::Value(MAX_VOLUME_VALUE) },
    // gui options
    { "last_visited",           ""_v },
    { "status_format_string",   "%s - %g - %a"_v },
    { "recent_files",           conf::Value{ conf::ValueList{} } },
    { "recent_playlists",       conf::Value{ conf::ValueList{} } },
    // shortcuts
    { "play_pause",             "Ctrl+Space"_v },
    { "next",                   "Ctrl+Right"_v },
    { "prev",                   "Ctrl+Left"_v },
    { "stop",                   "Ctrl+S"_v    },
    { "seek_forward",           "Right"_v },
    { "seek_backward",          "Left"_v },
    { "volume_up",              "0"_v },
    { "volume_down",            "9"_v },
};

} // namespace detail

struct Config {
    conf::Data data;
    std::unordered_map<std::string, CallbackHandler<void(const conf::Value &)>> callbacks;
    // mutable std::mutex mut;

    auto load()
    {
        auto [data, errors] = conf::parse_or_create(APP_NAME, detail::defaults);
        this->data = std::move(data);
        return errors;
    }

    template <typename T>
    T get(const std::string &key) const
    {
        // std::lock_guard<std::mutex> lock(mut);
        auto it = data.find(key);
        if (it == data.end())
            fmt::print("Config::get(): missing key {}\n", key);
        return it->second.as<T>();
    }

    template <typename T>
    void set(const std::string &key, const T &value)
    {
        // std::lock_guard<std::mutex> lock(mut);
        auto v = conf::Value(value);
        data[key] = v;
        callbacks[key](v);
    }

    void when_set(const std::string &key, auto &&fn)
    {
        callbacks[key].add(fn);
    }

    void save()
    {
        conf::write(APP_NAME, data);
    }
};

extern Config config;
