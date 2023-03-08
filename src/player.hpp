#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <vector>
#include <SDL_audio.h> // SDL_AudioDeviceID
#include "mpris_server.hpp"
#include "common.hpp"
#include "io.hpp"
#include "format.hpp"

namespace gmplayer {

namespace literals {
    inline constexpr long long operator"" _sec(unsigned long long secs) { return secs * 1000ull; }
    inline constexpr long long operator"" _min(unsigned long long mins) { return mins * 60_sec; }
}

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
    int silence_detection   = 0;
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
    void clear() { order.clear(); current = -1; }
    void remove(int i) { order.erase(order.begin() + i); }

    int move(int i, int pos)
    {
        if (i + pos < 0 || i + pos > order.size() - 1)
            return i;
        std::swap(order[i], order[i+pos]);
        return i+pos;
    }

    std::optional<int> next() const
    {
        return repeat && current != -1    ? std::optional{current}
             : current + 1 < order.size() ? std::optional{current + 1}
             : std::nullopt;
    }

    std::optional<int> prev() const
    {
        return repeat && current != -1 ? std::optional{current}
             : current - 1 >= 0        ? std::optional{current - 1}
             : std::nullopt;
    }
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
    std::unique_ptr<Interface> format = nullptr;
    std::vector<io::MappedFile> file_cache;
    std::vector<Metadata> track_cache;
    Playlist files;
    Playlist tracks;
    std::unique_ptr<mpris::Server> mpris = nullptr;

    struct {
        int id                   = 0;
        SDL_AudioDeviceID dev_id = 0;
        mutable SDLMutex mutex;
        SDL_AudioSpec spec;
    } audio;

    struct {
        bool autoplay;
        bool silence_detection;
        int default_duration;
        int fade_out;
        int volume;
        double tempo;
    } opts;

    void audio_callback(std::span<u8> stream);
    friend void audio_callback(void *, u8 *stream, int len);
    Error add_file_internal(std::filesystem::path path);

public:
    enum class List { Track, File };

    Player(PlayerOptions &&options);
    ~Player();

    Player(const Player &) = delete;
    Player & operator=(const Player &) = delete;

    Error add_file(std::filesystem::path path);
    std::vector<Error> add_files(std::span<std::filesystem::path> path);
    void remove_file(int fileno);
    bool load_file(int fileno);
    bool load_track(int num);
    void load_pair(int file, int track);
    void save_playlist(List which, io::File &to);
    void clear();
    const io::MappedFile &current_file() const;
    const       Metadata &current_track() const;

    bool is_playing() const;
    void start_or_resume();
    void pause();
    void play_pause();
    void stop();
    void seek(int ms);
    void seek_relative(int off);
    int position();
    int length() const;

    void next();
    void prev();
    bool has_next() const;
    bool has_prev() const;
    void shuffle(List which);
    int move(List which, int n, int pos);
    std::vector<std::string> names(List which) const;

    PlayerOptions options();
    void set_fade(int secs);
    void set_tempo(double tempo);
    void set_silence_detection(bool ignore);
    void set_default_duration(int secs);
    void set_autoplay(bool value);
    void set_track_repeat(bool value);
    void set_file_repeat(bool value);
    void set_volume(int value);
    void set_volume_relative(int offset);

    mpris::Server &mpris_server() { return *mpris; }

    // Signal<int> file_changed;
    // Signal<int, const Metadata &> track_changed;
    // Signal<int> position_changed;
    // Signal<void> track_ended;
    // Signal<void> paused;
    // Signal<void> played;
    // Signal<void> seeked;
    // Signal<int> volume_changed;
    // Signal<double> tempo_changed;
    // Signal<int;> fade_changed;
    // Signal<bool, bool> repeat_changed;
    // Signal<List> shuffled;
    // Signal<Error> error;
    // Signal<void> cleared;

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

#undef MAKE_SIGNAL

    // std::array<std::function<void(void)>, 2> playlist_changed_callbacks;

    // void on_playlist_changed(List list, auto &&fn) { playlist_changed_callbacks[static_cast<int>(list)] = fn; }
    // void playlist_changed(List list) { playlist_changed_callbacks[static_cast<int>(list)](); }
};

// this is here for portability
inline constexpr int get_max_volume_value() { return SDL_MIX_MAXVOLUME; }

} // namespace gmplayer
