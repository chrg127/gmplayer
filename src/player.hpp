#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <vector>
#include <SDL_audio.h> // SDL_AudioDeviceID
#include <mpris_server.hpp>
#include "common.hpp"

class Music_Emu;
class gme_info_t;
namespace io {
    class File;
    class MappedFile;
}

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
    std::vector<std::pair<std::string, std::error_condition>> errors;
};

struct TrackMetadata {
    int length;
    std::string_view system;
    std::string_view game;
    std::string_view song;
    std::string_view author;
    std::string_view copyright;
    std::string_view comment;
    std::string_view dumper;
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
    // emulator and audio device objects.
    // a mutex is needed because audio plays in another thread.
    Music_Emu *emu           = nullptr;
    int id                   = 0;
    SDL_AudioDeviceID dev_id = 0;
    mutable SDLMutex audio_mutex;
    SDL_AudioSpec obtained;

    std::vector<io::MappedFile> cache;
    std::unique_ptr<mpris::Server> mpris = nullptr;
    Playlist files;
    Playlist tracks;

    struct {
        bool autoplay;
        bool silence_detection;
        int default_duration;
        int fade_out;
        int volume;
        double tempo;
    } opts;

    // current track information:
    struct {
        gme_info_t *metadata = nullptr;
        int length = 0;
    } track;

    void audio_callback(std::span<u8> stream);
    friend void audio_callback(void *, u8 *stream, int len);
    std::error_condition add_file_internal(std::filesystem::path path);

public:
    enum class List { Track, File };

    Player(PlayerOptions &&options);
    ~Player();

    Player(const Player &) = delete;
    Player & operator=(const Player &) = delete;

    bool no_file_loaded() const;

    OpenPlaylistResult open_file_playlist(std::filesystem::path path);
    std::error_condition add_file(std::filesystem::path path);
    std::error_condition remove_file(int fileno);
    void save_playlist(List which, io::File &to);
    void clear();
    void load(List which, int n) { which == List::Track ? load_track(n) : load_file(n); }
    void load_file(int fileno);
    void load_track(int num);

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

#define CALLBACK(name, ...) \
private: std::function<void(__VA_ARGS__)> name;      \
public:  void on_##name(auto &&fn) { name = fn; }    \

    CALLBACK(track_changed, int, gme_info_t *, int)
    CALLBACK(position_changed, int)
    CALLBACK(track_ended, void)
    CALLBACK(file_changed, int)
    CALLBACK(volume_changed, int)
    CALLBACK(paused, void)
    CALLBACK(played, void)
    CALLBACK(stopped, void)
    CALLBACK(file_order_changed,  const std::vector<std::string> &)
    CALLBACK(track_order_changed, const std::vector<std::string> &)
    CALLBACK(playlist_changed, List)
    CALLBACK(repeat_changed, bool, bool, bool)
    CALLBACK(tempo_changed, double)
    CALLBACK(load_file_error, std::string_view, std::string_view)
    CALLBACK(load_track_error, std::string_view, int, std::string_view, std::string_view)
    CALLBACK(seek_error, std::string_view)
    CALLBACK(fade_set, int);

#undef CALLBACK
};

// this is here for portability
inline constexpr int get_max_volume_value() { return SDL_MIX_MAXVOLUME; }
