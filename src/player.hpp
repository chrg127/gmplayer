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
    std::vector<std::string> not_opened;
    std::vector<std::error_condition> errors;
};

class Player {
    // emulator and audio device objects.
    // a mutex is needed because audio plays in another thread.
    Music_Emu *emu           = nullptr;
    int id                   = 0;
    SDL_AudioDeviceID dev_id = 0;
    mutable SDLMutex audio_mutex;
    SDL_AudioSpec obtained;
    PlayerOptions options = {};
    std::unique_ptr<mpris::Server> mpris = nullptr;

    // current track information:
    struct {
        gme_info_t *metadata = nullptr;
        int length = 0;
    } track;

    struct {
        std::vector<io::MappedFile> cache;
        std::vector<int> order;
        int current = -1;
    } files;

    struct {
        std::vector<int> order;
        int current = -1;
        int count = 0;
    } tracks;

    void audio_callback(std::span<u8> stream);
    friend void audio_callback(void *, u8 *stream, int len);

public:
    Player(PlayerOptions &&options);
    ~Player();

    Player(const Player &) = delete;
    Player & operator=(const Player &) = delete;

    bool no_file_loaded() const;

    OpenPlaylistResult open_file_playlist(std::filesystem::path path);
    std::error_condition add_file(std::filesystem::path path);
    std::error_condition remove_file(int fileno);
    void save_file_playlist(io::File &to);
    void clear_file_playlist();
    void load_file(int fileno);
    void load_track(int num);

    bool is_playing() const;
    void start_or_resume();
    void pause();
    void play_pause();
    void stop();
    void next();
    void prev();
    void seek(int ms);
    void seek_relative(int off);

    int current_track() const;
    int current_file() const;
    std::optional<int> get_next_file() const;
    std::optional<int> get_prev_file() const;
    std::optional<int> get_next_track() const;
    std::optional<int> get_prev_track() const;
    int position();
    int length() const;
    int effective_length() const;
    std::vector<std::string> file_names() const;
    std::vector<std::string> track_names() const;
    void shuffle_tracks(bool do_shuffle);
    void shuffle_files(bool do_shuffle);
    int move_track(int n, int where, int min, int max);
    int move_file(int n, int where, int min, int max);
    int move_track_up(int trackno);
    int move_track_down(int trackno);
    int move_file_up(int fileno);
    int move_file_down(int fileno);

    const PlayerOptions & get_options();
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
