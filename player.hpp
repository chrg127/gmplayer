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

    // callbacks
    std::function<void(int, gme_info_t *, int)> track_changed;
    std::function<void(int)>                    position_changed;
    std::function<void(void)>                   track_ended;
    std::function<void(int)>                    file_changed;

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
    void remove_file(int fileno);
    void save_file_playlist(io::File &to);
    void clear_file_playlist();

    int load_file(int fileno);
    int load_track(int num);
    bool can_play() const;
    bool is_playing() const;

    void start_or_resume();
    void pause();
    void next();
    void prev();
    void seek(int ms);

    std::optional<std::pair<int, int>> get_next() const;
    std::optional<int> get_prev_file() const;
    std::optional<int> get_prev_track() const;
    int position();
    int length() const;
    int effective_length() const;
    void file_names(std::function<void(const std::string &)> f) const;
    void track_names(std::function<void(const std::string &)> f) const;

    PlayerOptions & get_options();
    void set_fade(int secs);
    void set_tempo(double tempo);
    void set_silence_detection(bool ignore);
    void set_default_duration(int secs);
    void set_autoplay(bool value);
    void set_track_repeat(bool value);
    void set_track_shuffle(bool value);
    void set_file_repeat(bool value);
    void set_file_shuffle(bool value);
    void set_volume(int value);

    void on_track_changed(auto &&fn)    { track_changed    = fn; }
    void on_position_changed(auto &&fn) { position_changed = fn; }
    void on_track_ended(auto &&fn)      { track_ended      = fn; }
    void on_file_changed(auto &&fn)     { file_changed     = fn; }
};

// this is here for portability
inline constexpr int get_max_volume_value() { return SDL_MIX_MAXVOLUME; }
