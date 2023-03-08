#ifndef MPRIS_SERVER_HPP_INCLUDED
#define MPRIS_SERVER_HPP_INCLUDED

#include <cstdio>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include <memory>
#include <initializer_list>

#ifdef _WIN32
#define MPRIS_SERVER_NO_IMPL
#endif

#ifndef MPRIS_SERVER_NO_IMPL
#include <sdbus-c++/Types.h>
#include <sdbus-c++/IObject.h>
#include <sdbus-c++/IConnection.h>
#else

namespace sdbus {

struct Variant {
    Variant() = default;
    template <typename T>
    Variant(const T &) { }
};

struct ObjectPath { };
struct IConnection { };
struct IObject { };

} // namespace sdbus

#endif

namespace mpris {

using namespace std::literals::string_literals;

using StringList = std::vector<std::string>;
using Metadata   = std::map<std::string, sdbus::Variant>;

static const auto PREFIX      = "org.mpris.MediaPlayer2."s;
static const auto OBJECT_PATH = "/org/mpris/MediaPlayer2"s;
static const auto MP2         = "org.mpris.MediaPlayer2"s;
static const auto MP2P        = "org.mpris.MediaPlayer2.Player"s;
static const auto PROPS       = "org.freedesktop.DBus.Properties"s;

enum class PlaybackStatus { Playing, Paused, Stopped };
enum class LoopStatus     { None, Track, Playlist };

enum class Field {
    TrackId     , Length     , ArtUrl      , Album          ,
    AlbumArtist , Artist     , AsText      , AudioBPM       ,
    AutoRating  , Comment    , Composer    , ContentCreated ,
    DiscNumber  , FirstUsed  , Genre       , LastUsed       ,
    Lyricist    , Title      , TrackNumber , Url            ,
    UseCount    , UserRating
};

static const char *playback_status_strings[] = { "Playing"           , "Paused"              , "Stopped" };
static const char *loop_status_strings[]     = { "None"              , "Track"               , "Playlist" };
static const char *metadata_strings[]        = { "mpris:trackid"     , "mpris:length"        , "mpris:artUrl"      , "xesam:album"          ,
                                                 "xesam:albumArtist" , "xesam:artist"        , "xesam:asText"      , "xesam:audioBPM"       ,
                                                 "xesam:autoRating"  , "xesam:comment"       , "xesam:composer"    , "xesam:contentCreated" ,
                                                 "xesam:discNumber"  , "xesam:firstUsed"     , "xesam:genre"       , "xesam:lastUsed"       ,
                                                 "xesam:lyricist"    , "xesam:title"         , "xesam:trackNumber" , "xesam:url"            ,
                                                 "xesam:useCount"    , "xesam:userRating" };

namespace detail {

    template <typename T, typename R, typename... Args>
    std::function<R(Args...)> member_fn(T *obj, R (T::*fn)(Args...))
    {
        return [=](Args&&... args) -> R { return (obj->*fn)(args...); };
    }

    template <typename T, typename R, typename... Args>
    std::function<R(Args...)> member_fn(T *obj, R (T::*fn)(Args...) const)
    {
        return [=](Args&&... args) -> R { return (obj->*fn)(args...); };
    }

    inline std::string playback_status_to_string(PlaybackStatus status) { return playback_status_strings[static_cast<int>(status)]; }
    inline std::string loop_status_to_string(        LoopStatus status) { return     loop_status_strings[static_cast<int>(status)]; }
    inline std::string field_to_string(                    Field entry) { return        metadata_strings[static_cast<int>( entry)]; }

} // namespace detail

class Server {
protected:
    std::string service_name;
    std::unique_ptr<sdbus::IConnection> connection;
    std::unique_ptr<sdbus::IObject> object;

    std::function<void(void)>               quit_fn;
    std::function<void(void)>               raise_fn;
    std::function<void(void)>               next_fn;
    std::function<void(void)>               previous_fn;
    std::function<void(void)>               pause_fn;
    std::function<void(void)>               play_pause_fn;
    std::function<void(void)>               stop_fn;
    std::function<void(void)>               play_fn;
    std::function<void(int64_t)>            seek_fn;
    std::function<void(int64_t)>            set_position_fn;
    std::function<void(std::string_view)>   open_uri_fn;
    std::function<void(bool)>               fullscreen_changed_fn;
    std::function<void(LoopStatus)>         loop_status_changed_fn;
    std::function<void(double)>             rate_changed_fn;
    std::function<void(bool)>               shuffle_changed_fn;
    std::function<void(double)>             volume_changed_fn;

    bool fullscreen                  = false;
    std::string identity             = "";
    std::string desktop_entry        = "";
    StringList supported_uri_schemes = {};
    StringList supported_mime_types  = {};
    PlaybackStatus playback_status   = PlaybackStatus::Stopped;
    LoopStatus loop_status           = LoopStatus::None;
    double rate                      = 1.0;
    bool shuffle                     = false;
    Metadata metadata                = {};
    double volume                    = 0.0;
    int64_t position                 = 0;
    double maximum_rate              = 1.0;
    double minimum_rate              = 1.0;

    virtual void prop_changed(const std::string &interface, const std::string &name, sdbus::Variant value) = 0;
    virtual void control_props_changed(std::initializer_list<std::string_view> args) = 0;
    virtual void set_fullscreen_external(bool value) = 0;
    virtual void set_loop_status_external(const std::string &value) = 0;
    virtual void set_rate_external(double value) = 0;
    virtual void set_shuffle_external(bool value) = 0;
    virtual void set_volume_external(double value) = 0;
    virtual void set_position_method(sdbus::ObjectPath id, int64_t pos) = 0;
    virtual void open_uri(const std::string &uri) = 0;

public:
    explicit Server(std::string_view name)
        : service_name(PREFIX + std::string(name))
    { }

    virtual ~Server() = default;

    virtual void start_loop() = 0;
    virtual void start_loop_async() = 0;

    bool can_control()     const { return bool(loop_status_changed_fn) && bool(shuffle_changed_fn)
                                       && bool(volume_changed_fn)      && bool(stop_fn);                }
    bool can_go_next()     const { return can_control() && bool(next_fn);                               }
    bool can_go_previous() const { return can_control() && bool(previous_fn);                           }
    bool can_play()        const { return can_control() && bool(play_fn)      && bool(play_pause_fn);   }
    bool can_pause()       const { return can_control() && bool(pause_fn)     && bool(play_pause_fn);   }
    bool can_seek()        const { return can_control() && bool(seek_fn)      && bool(set_position_fn); }

    void on_quit                ( auto &&fn) { quit_fn                = fn; prop_changed(MP2, "CanQuit", true);                                                    }
    void on_raise               ( auto &&fn) { raise_fn               = fn; prop_changed(MP2, "CanQuit", true);                                                    }
    void on_next                ( auto &&fn) { next_fn                = fn; control_props_changed({"CanGoNext"});                                                    }
    void on_previous            ( auto &&fn) { previous_fn            = fn; control_props_changed({"CanGoPrevious"});                                                }
    void on_pause               ( auto &&fn) { pause_fn               = fn; control_props_changed({"CanPause"});                                                     }
    void on_play_pause          ( auto &&fn) { play_pause_fn          = fn; control_props_changed({"CanPlay", "CanPause"});                                          }
    void on_stop                ( auto &&fn) { stop_fn                = fn; control_props_changed({"CanGoNext", "CanGoPrevious", "CanPause", "CanPlay", "CanSeek"}); }
    void on_play                ( auto &&fn) { play_fn                = fn; control_props_changed({"CanPlay"});                                                      }
    void on_seek                ( auto &&fn) { seek_fn                = fn; control_props_changed({"CanSeek"});                                                      }
    void on_set_position        ( auto &&fn) { set_position_fn        = fn; control_props_changed({"CanSeek"});                                                      }
    void on_open_uri            ( auto &&fn) { open_uri_fn            = fn;                                                                                        }
    void on_fullscreen_changed  ( auto &&fn) { fullscreen_changed_fn  = fn; prop_changed(MP2, "CanSetFullscreen", true);                                           }
    void on_loop_status_changed ( auto &&fn) { loop_status_changed_fn = fn; control_props_changed({"CanGoNext", "CanGoPrevious", "CanPause", "CanPlay", "CanSeek"}); }
    void on_rate_changed        ( auto &&fn) { rate_changed_fn        = fn;                                                                                        }
    void on_shuffle_changed     ( auto &&fn) { shuffle_changed_fn     = fn; control_props_changed({"CanGoNext", "CanGoPrevious", "CanPause", "CanPlay", "CanSeek"}); }
    void on_volume_changed      ( auto &&fn) { volume_changed_fn      = fn; control_props_changed({"CanGoNext", "CanGoPrevious", "CanPause", "CanPlay", "CanSeek"}); }

    void set_fullscreen(bool value)                         { fullscreen            = value; prop_changed(MP2,  "Fullscreen"          , fullscreen);                                         }
    void set_identity(std::string_view value)               { identity              = value; prop_changed(MP2,  "Identity"            , identity);                                           }
    void set_desktop_entry(std::string_view value)          { desktop_entry         = value; prop_changed(MP2,  "DesktopEntry"        , desktop_entry);                                      }
    void set_supported_uri_schemes(const StringList &value) { supported_uri_schemes = value; prop_changed(MP2,  "SupportedUriSchemes" , supported_uri_schemes );                             }
    void set_supported_mime_types(const StringList &value)  { supported_mime_types  = value; prop_changed(MP2,  "SupportedMimeTypes"  , supported_mime_types);                               }
    void set_playback_status(PlaybackStatus value)          { playback_status       = value; prop_changed(MP2P, "PlaybackStatus"      , detail::playback_status_to_string(playback_status)); }
    void set_loop_status(LoopStatus value)                  { loop_status           = value; prop_changed(MP2P, "LoopStatus"          , detail::loop_status_to_string(loop_status));         }
    void set_shuffle(bool value)                            { shuffle               = value; prop_changed(MP2P, "Shuffle"             , shuffle);                                            }
    void set_volume(double value)                           { volume                = value; prop_changed(MP2P, "Volume"              , volume);                                             }
    void set_position(int64_t value)                        { position              = value;                                                                                                 }

    void set_rate(double value)
    {
        if (value == 0.0) {
            fprintf(stderr, "warning: rate value must not be 0.0.\n");
            return;
        }
        if (value < minimum_rate || value > maximum_rate) {
            fprintf(stderr, "warning: rate value not in range.\n");
            return;
        }
        rate = value;
        prop_changed(MP2P, "Rate", rate);
    }

    void set_metadata(const std::map<Field, sdbus::Variant> &value)
    {
        metadata.clear();
        for (auto [k, v] : value)
            metadata[detail::field_to_string(k)] = v;
        prop_changed(MP2P, "Metadata", metadata);
    }

    void set_minimum_rate(double value)
    {
        if (value > 1.0) {
            fprintf(stderr, "warning: minimum value should always be 1.0 or lower\n");
            return;
        }
        minimum_rate = value;
        prop_changed(MP2P, "MinimumRate", minimum_rate);
    }

    void set_maximum_rate(double value)
    {
        if (value < 1.0) {
            fprintf(stderr, "warning: maximum rate should always be 1.0 or higher\n");
            return;
        }
        maximum_rate = value;
        prop_changed(MP2P, "MaximumRate"         , maximum_rate);
    }

    void send_seeked_signal(int64_t position);
};

std::unique_ptr<Server> make_server(std::string_view name, bool create_empty = true);

} // namespace mpris

#endif
