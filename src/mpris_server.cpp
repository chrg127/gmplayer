#include "mpris_server.hpp"

#ifndef MPRIS_SERVER_NO_IMPL
    #include <sdbus-c++/sdbus-c++.h>
#endif

namespace mpris {

#ifndef MPRIS_SERVER_NO_IMPL

struct SDBusServer : public Server {
    explicit SDBusServer(std::string_view name) : Server(name)
    {
        connection = sdbus::createSessionBusConnection();
        connection->requestName(service_name);
        object = sdbus::createObject(*connection, OBJECT_PATH);

#define M(f) detail::member_fn(this, &SDBusServer::f)
        object->registerMethod("Raise")      .onInterface(MP2) .implementedAs([&] { if (raise_fn)                  raise_fn();      });
        object->registerMethod("Quit")       .onInterface(MP2) .implementedAs([&] { if (quit_fn)                   quit_fn();       });
        object->registerMethod("Next")       .onInterface(MP2P).implementedAs([&] { if (can_go_next())             next_fn();       });
        object->registerMethod("Previous")   .onInterface(MP2P).implementedAs([&] { if (can_go_previous())         previous_fn();   });
        object->registerMethod("Pause")      .onInterface(MP2P).implementedAs([&] { if (can_pause())               pause_fn();      });
        object->registerMethod("PlayPause")  .onInterface(MP2P).implementedAs([&] { if (can_play() || can_pause()) play_pause_fn(); });
        object->registerMethod("Stop")       .onInterface(MP2P).implementedAs([&] { if (can_control())             stop_fn();       });
        object->registerMethod("Play")       .onInterface(MP2P).implementedAs([&] { if (can_play())                play_fn();       });
        object->registerMethod("Seek")       .onInterface(MP2P).implementedAs([&] (int64_t n) { if (can_seek()) seek_fn(n); }).withInputParamNames("Offset");
        object->registerMethod("SetPosition").onInterface(MP2P).implementedAs(M(set_position_method))                         .withInputParamNames("TrackId", "Position");
        object->registerMethod("OpenUri")    .onInterface(MP2P).implementedAs(M(open_uri))                                    .withInputParamNames("Uri");

        object->registerProperty("CanQuit")             .onInterface(MP2).withGetter([&] { return bool(quit_fn); });
        object->registerProperty("Fullscreen")          .onInterface(MP2).withGetter([&] { return fullscreen; })
                                                                         .withSetter(M(set_fullscreen_external));
        object->registerProperty("CanSetFullscreen")    .onInterface(MP2).withGetter([&] { return bool(fullscreen_changed_fn); });
        object->registerProperty("CanRaise")            .onInterface(MP2).withGetter([&] { return bool(raise_fn); });
        object->registerProperty("HasTrackList")        .onInterface(MP2).withGetter([&] { return false; });
        object->registerProperty("Identity")            .onInterface(MP2).withGetter([&] { return identity; });
        object->registerProperty("DesktopEntry")        .onInterface(MP2).withGetter([&] { return desktop_entry; });
        object->registerProperty("SupportedUriSchemes") .onInterface(MP2).withGetter([&] { return supported_uri_schemes; });
        object->registerProperty("SupportedMimeTypes")  .onInterface(MP2).withGetter([&] { return supported_mime_types; });

        object->registerProperty("PlaybackStatus").onInterface(MP2P).withGetter([&] { return detail::playback_status_to_string(playback_status); });
        object->registerProperty("LoopStatus")    .onInterface(MP2P).withGetter([&] { return detail::loop_status_to_string(loop_status); })
                                                                    .withSetter(M(set_loop_status_external));
        object->registerProperty("Rate")          .onInterface(MP2P).withGetter([&] { return rate; })
                                                                    .withSetter(M(set_rate_external));
        object->registerProperty("Shuffle")       .onInterface(MP2P).withGetter([&] { return shuffle; })
                                                                    .withSetter(M(set_shuffle_external));
        object->registerProperty("Metadata")      .onInterface(MP2P).withGetter([&] { return metadata; });
        object->registerProperty("Volume")        .onInterface(MP2P).withGetter([&] { return volume; })
                                                                    .withSetter(M(set_volume_external));
        object->registerProperty("Position")      .onInterface(MP2P).withGetter([&] { return position; });
        object->registerProperty("MinimumRate")   .onInterface(MP2P).withGetter([&] { return minimum_rate; });
        object->registerProperty("MaximumRate")   .onInterface(MP2P).withGetter([&] { return maximum_rate; });
        object->registerProperty("CanGoNext")     .onInterface(MP2P).withGetter([&] { return can_go_next(); });
        object->registerProperty("CanGoPrevious") .onInterface(MP2P).withGetter([&] { return can_go_previous(); });
        object->registerProperty("CanPlay")       .onInterface(MP2P).withGetter([&] { return can_play(); });
        object->registerProperty("CanPause")      .onInterface(MP2P).withGetter([&] { return can_pause(); });
        object->registerProperty("CanSeek")       .onInterface(MP2P).withGetter([&] { return can_seek(); });
        object->registerProperty("CanControl")    .onInterface(MP2P).withGetter([&] { return can_control(); });
#undef M

        object->registerSignal("Seeked").onInterface(MP2P).withParameters<int64_t>("Position");

        object->finishRegistration();
    }

    void prop_changed(const std::string &interface, const std::string &name, sdbus::Variant value)
    {
        std::map<std::string, sdbus::Variant> d;
        d[name] = value;
        object->emitSignal("PropertiesChanged").onInterface("org.freedesktop.DBus.Properties").withArguments(interface, d, std::vector<std::string>{});
    }

    void control_props_changed(std::initializer_list<std::string_view> args)
    {
        auto f = [&] (std::string_view name) {
            if (name == "CanGoNext")     return can_go_next();
            if (name == "CanGoPrevious") return can_go_previous();
            if (name == "CanPause")      return can_pause();
            if (name == "CanPlay")       return can_play();
            if (name == "CanSeek")       return can_seek();
            return false;
        };
        std::map<std::string, sdbus::Variant> d;
        for (auto s : args)
            if (f(s))
                d[std::string(s)] = true;
        // auto g = [&] (const std::string &v) { if (f(v)) d[v] = true; };
        // (g(args), ...);
        object->emitSignal("PropertiesChanged").onInterface("org.freedesktop.DBus.Properties").withArguments(MP2P, d, std::vector<std::string>{});
    }

    void set_fullscreen_external(bool value)
    {
        if (fullscreen_changed_fn)
            throw sdbus::Error(service_name + ".Error", "Cannot set Fullscreen (CanSetFullscreen is false).");
        set_fullscreen(value);
        fullscreen_changed_fn(fullscreen);
    }

    void set_loop_status_external(const std::string &value)
    {
        for (auto i = 0u; i < std::size(loop_status_strings); i++) {
            if (value == loop_status_strings[i]) {
                if (!can_control())
                    throw sdbus::Error(service_name + ".Error", "Cannot set loop status (CanControl is false).");
                set_loop_status(static_cast<LoopStatus>(i));
                loop_status_changed_fn(loop_status);
            }
        }
    }

    void set_rate_external(double value)
    {
        if (value <= minimum_rate || value > maximum_rate)
            throw sdbus::Error(service_name + ".Error", "Rate value not in range.");
        if (value == 0.0)
            throw sdbus::Error(service_name + ".Error", "Rate value must not be 0.0.");
        set_rate(value);
        if (rate_changed_fn)
            rate_changed_fn(rate);
    }

    void set_shuffle_external(bool value)
    {
        if (!can_control())
            throw sdbus::Error(service_name + ".Error", "Cannot set shuffle (CanControl is false).");
        set_shuffle(value);
        shuffle_changed_fn(shuffle);
    }

    void set_volume_external(double value)
    {
        if (!can_control())
            throw sdbus::Error(service_name + ".Error", "Cannot set volume (CanControl is false).");
        set_volume(value < 0.0 ? 0.0 : value > 1.0 ? 1.0 : value);
        volume_changed_fn(volume);
    }

    void set_position_method(sdbus::ObjectPath id, int64_t pos)
    {
        if (!can_seek())
            return;
        auto tid = metadata.find(detail::field_to_string(Field::TrackId));
        if (tid == metadata.end() || tid->second.get<std::string>() != id)
            return;
        set_position_fn(pos);
    }

    void open_uri(const std::string &uri)
    {
        if (open_uri_fn)
            open_uri_fn(uri);
    }

    void start_loop()       { connection->enterEventLoop(); }
    void start_loop_async() { connection->enterEventLoopAsync(); }

    void send_seeked_signal(int64_t position)
    {
        object->emitSignal("Seeked").onInterface(MP2P).withArguments(position);
    }
};

#endif

struct EmptyServer : public Server {
    explicit EmptyServer(std::string_view name) : Server(name) { }
    void prop_changed(const std::string &interface, const std::string &name, sdbus::Variant value) { }
    void control_props_changed(std::initializer_list<std::string_view> args) {}
    void set_fullscreen_external(bool value) { }
    void set_loop_status_external(const std::string &value) { }
    void set_rate_external(double value) { }
    void set_shuffle_external(bool value) { }
    void set_volume_external(double value) { }
    void set_position_method(sdbus::ObjectPath id, int64_t pos) { }
    void open_uri(const std::string &uri) { }
    void start_loop() { }
    void start_loop_async() { }
    void send_seeked_signal(int64_t position) { }
};

std::unique_ptr<Server> make_server(std::string_view name, bool create_empty)
{
#ifdef MPRIS_SERVER_NO_IMPL
    return std::make_unique<EmptyServer>(name);
#else
    try {
        auto s = std::make_unique<SDBusServer>(name);
        return s;
    } catch (const sdbus::Error &error) {
        return std::make_unique<EmptyServer>(name);
    }
#endif
}

} // namespace mpris
