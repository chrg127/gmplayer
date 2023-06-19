#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <vector>
#include <SDL_audio.h> // SDL_AudioDeviceID
#include "common.hpp"
#include "format.hpp"

namespace mpris { struct Server; }
namespace io { class File; class MappedFile; }

namespace gmplayer {

struct SDLMutex {
    SDL_AudioDeviceID id;
    SDLMutex() = default;
    SDLMutex(SDL_AudioDeviceID id) : id{id} {}
    void lock()   { SDL_LockAudioDevice(id); }
    void unlock() { SDL_UnlockAudioDevice(id); }
};

struct PlayerOptions {
    int fade_out            = 0;
    bool autoplay           = false;
    bool track_repeat       = false;
    bool file_repeat        = false;
    int default_duration    = 3 * 60 * 1000ull;
    double tempo            = 1.0;
    int volume              = SDL_MIX_MAXVOLUME;
};

struct Playlist {
    std::vector<int> order;
    int current = -1;
    bool repeat;

    void regen();
    void regen(int size);
    void shuffle();
    void clear()         { order.clear(); current = -1; }
    void remove(int i)   { order.erase(order.begin() + i); }

    int move(int i, int pos)
    {
        if (i + pos < 0 || i + pos > order.size() - 1)
            return i;
        std::swap(order[i], order[i+pos]);
        return i+pos;
    }

    std::optional<int> get(int off, int min, int max) const
    {
        return repeat && current != -1                    ? std::optional{current}
             : current + off < max && current + off > min ? std::optional{current + off}
             : std::nullopt;
    }

    std::optional<int> next() const { return get(+1, -1, order.size()); }
    std::optional<int> prev() const { return get(-1, -1, order.size()); }
};

template <typename T> class Signal;

template <typename R, typename... P>
class Signal<R(P...)> {
    std::vector<std::function<R(P...)>> callbacks;
public:
    void add(auto &&fn) { callbacks.push_back(fn); }
    void call(auto&&... args)
    {
        for (auto &f : callbacks)
            f(std::forward<decltype(args)>(args)...);
    }
    void operator()(auto&&... args) { call(std::forward<decltype(args)>(args)...); }
};

class Player {
    std::unique_ptr<FormatInterface> format;
    std::vector<io::MappedFile> file_cache;
    std::vector<Metadata> track_cache;
    Playlist files;
    Playlist tracks;
    std::unique_ptr<mpris::Server> mpris;

    struct {
        SDL_AudioDeviceID dev_id = 0;
        mutable SDLMutex mutex;
        SDL_AudioSpec spec;
    } audio;

    struct {
        bool autoplay;
        int default_duration;
        int fade_out;
        int volume;
        double tempo;
    } opts;

    void audio_callback(std::span<u8> stream);
    Error add_file_internal(std::filesystem::path path);

public:
    enum class List { Track, File };

    Player(PlayerOptions &&options);
    ~Player();

    Error add_file(std::filesystem::path path);
    std::pair<std::vector<Error>, int> add_files(std::span<std::filesystem::path> path);
    void remove_file(int fileno);
    bool load_file(int fileno);
    bool load_track(int num);
    void load_pair(int file, int track);
    void save_playlist(List which, io::File &to);
    void clear();
    const io::MappedFile & current_file()  const;
    const Metadata       & current_track() const;

    bool is_playing() const;
    void start_or_resume();
    void pause();
    void play_pause();
    void stop();
    void seek(int ms);
    void seek_relative(int off);
    int position();
    int length() const;
    bool is_multi_channel() const;

    void next();
    void prev();
    bool has_next() const;
    bool has_prev() const;
    void shuffle(List which);
    int move(List which, int n, int pos);
    std::vector<std::string> names(List which) const;

    std::vector<std::string> channel_names();
    void mute_channel(int index, bool mute);

    PlayerOptions options();
    void set_fade(int secs);
    void set_tempo(double tempo);
    void set_default_duration(int secs);
    void set_autoplay(bool value);
    void set_track_repeat(bool value);
    void set_file_repeat(bool value);
    void set_volume(int value);
    void set_volume_relative(int offset);
    void set_load_m3u(bool value);

    mpris::Server &mpris_server();

#define MAKE_SIGNAL(name, ...) \
private:                                            \
    Signal<void(__VA_ARGS__)> name;                 \
public:                                             \
    void on_##name(auto &&fn) { name.add(fn); }     \

    MAKE_SIGNAL(file_changed, int)
    MAKE_SIGNAL(track_changed, int, const Metadata &)
    MAKE_SIGNAL(position_changed, int)
    MAKE_SIGNAL(track_ended, void)
    MAKE_SIGNAL(paused, void)
    MAKE_SIGNAL(played, void)
    MAKE_SIGNAL(seeked, void)
    MAKE_SIGNAL(volume_changed, int)
    MAKE_SIGNAL(tempo_changed, double)
    MAKE_SIGNAL(fade_changed, int);
    MAKE_SIGNAL(repeat_changed, bool, bool)
    MAKE_SIGNAL(shuffled, List)
    MAKE_SIGNAL(error, Error)
    MAKE_SIGNAL(cleared, void)
    MAKE_SIGNAL(playlist_changed, List)
    MAKE_SIGNAL(file_removed, int)
    MAKE_SIGNAL(samples_played, std::span<i16>, std::span<i16>)

#undef MAKE_SIGNAL
};

// this is here for portability
inline constexpr int get_max_volume_value() { return SDL_MIX_MAXVOLUME; }

inline bool is_playlist(std::filesystem::path filename) { return filename.extension() == ".playlist"; }

} // namespace gmplayer
