#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <vector>
#include <SDL_audio.h> // SDL_AudioDeviceID
#include <mpris_server.hpp>
#include "common.hpp"
#include "io.hpp"
#include "format.hpp"

inline constexpr std::size_t operator"" _sec(unsigned long long secs) { return secs * 1000ull; }
inline constexpr std::size_t operator"" _min(unsigned long long mins) { return mins * 60_sec; }

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
    int default_duration    = 3_min;
    int silence_detection   = 0;
    double tempo            = 1.0;
    int volume              = SDL_MIX_MAXVOLUME;
};

struct OpenPlaylistResult {
    std::error_condition pl_error = std::error_condition{};
    std::vector<std::pair<std::string, Error>> errors;
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
        return repeat                     ? std::optional{current}
             : current + 1 < order.size() ? std::optional{current + 1}
             : std::nullopt;
    }

    std::optional<int> prev() const
    {
        return repeat           ? std::optional{current}
             : current - 1 >= 0 ? std::optional{current - 1}
             : std::nullopt;
    }
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

    bool no_file_loaded() const;

    OpenPlaylistResult open_file_playlist(std::filesystem::path path);
    Error add_file(std::filesystem::path path);
    void remove_file(int fileno);
    Error load_file(int fileno);
    Error load_track(int num);
    Error load(List which, int n) { return which == List::Track ? load_track(n) : load_file(n); }
    void save_playlist(List which, io::File &to);
    void clear();
    const io::MappedFile &current_file() const;
    const       Metadata &current_track() const;

    bool is_playing() const;
    void start_or_resume();
    void pause();
    void play_pause();
    void stop();
    Error seek(int ms);
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

#define CALLBACK(name, ...) \
private: std::function<void(__VA_ARGS__)> name;      \
public:  void on_##name(auto &&fn) { name = fn; }    \

    CALLBACK(file_changed, int)
    CALLBACK(track_changed, int, const Metadata &)
    CALLBACK(position_changed, int)
    CALLBACK(track_ended, void)
    CALLBACK(paused, void)
    CALLBACK(played, void)
    CALLBACK(stopped, void)
    CALLBACK(volume_changed, int)
    CALLBACK(tempo_changed, double)
    CALLBACK(fade_changed, int);
    CALLBACK(playlist_changed, List)
    CALLBACK(repeat_changed, bool, bool)

#undef CALLBACK
};

// this is here for portability
inline constexpr int get_max_volume_value() { return SDL_MIX_MAXVOLUME; }
