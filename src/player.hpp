#pragma once

#include <cstdint>
#include <mutex>
#include <string_view>
#include <vector>
#include <optional>
#include <functional>
#include <filesystem>
#include <span>
#include <SDL_audio.h> // SDL_AudioDeviceID

class Music_Emu;
class gme_info_t;
namespace io {
    class File;
    class MappedFile;
}

using gme_err_t = const char *;

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
    int fade_out_ms         = 0;
    bool autoplay           = false;
    bool track_repeat       = false;
    bool track_shuffle      = false;
    bool file_repeat        = false;
    bool file_shuffle       = false;
    int default_duration    = 3_min;
    int silence_detection   = 0;
    double tempo            = 1.0;
    int volume              = SDL_MIX_MAXVOLUME;
};

struct OpenPlaylistResult {
    std::error_code pl_error;
    std::vector<std::string> not_opened;
    std::vector<std::string> errors;
};

class Player {
    // emulator and audio device objects.
    // a mutex is needed because audio plays in another thread.
    Music_Emu *emu           = nullptr;
    int id                   = 0;
    SDL_AudioDeviceID dev_id = 0;
    mutable SDLMutex audio_mutex;
    SDL_AudioSpec obtained;

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

    // options
    PlayerOptions options = {};

    void audio_callback(void *unused, uint8_t *stream, int stream_length);
    friend void audio_callback(void *unused, uint8_t *stream, int stream_length);

public:
    Player();
    ~Player();

    Player(const Player &) = delete;
    Player & operator=(const Player &) = delete;

    OpenPlaylistResult open_file_playlist(std::filesystem::path path);
    std::error_code add_file(std::filesystem::path path);
    bool remove_file(int fileno);
    void save_file_playlist(io::File &to);
    void clear_file_playlist();

    int load_file(int fileno);
    int load_track(int num);
    bool can_play() const;
    bool is_playing() const;

    void start_or_resume();
    void pause();
    void play_pause();
    void stop();
    void next();
    void prev();
    void seek(int ms);
    void seek_relative(int off);

    std::optional<std::pair<int, int>> get_next() const;
    std::optional<int> get_prev_file() const;
    std::optional<int> get_prev_track() const;
    int position();
    int length() const;
    int effective_length() const;
    std::vector<std::string> file_names() const;
    std::vector<std::string> track_names() const;
    void shuffle_tracks();
    void shuffle_files();

    PlayerOptions & get_options();
    void set_fade(int secs);
    void set_tempo(double tempo);
    void set_silence_detection(bool ignore);
    void set_default_duration(int secs);
    void set_autoplay(bool value);
    void set_track_repeat(bool value);
    void set_file_repeat(bool value);
    void set_volume(int value);
    void set_volume_relative(int offset);

    // callbacks
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
    CALLBACK(file_order_changed, const std::vector<std::string> &)
    CALLBACK(track_order_changed, const std::vector<std::string> &)

#undef CALLBACK
};

// this is here for portability
inline constexpr int get_max_volume_value() { return SDL_MIX_MAXVOLUME; }
