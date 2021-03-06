#pragma once

#include <cstdint>
#include <mutex>
#include <string_view>
#include <vector>
#include <optional>
#include <SDL_audio.h> // SDL_AudioDeviceID

class Music_Emu;
class gme_info_t;

typedef const char* gme_err_t;

inline constexpr std::size_t operator"" _sec(unsigned long long secs) { return secs * 1000ull; }
inline constexpr std::size_t operator"" _min(unsigned long long mins) { return mins * 60_sec; }

struct TrackInfo {
    gme_info_t *metadata;
    int length;
};

struct SDLMutex {
    SDL_AudioDeviceID id;
    SDLMutex() = default;
    SDLMutex(SDL_AudioDeviceID id) : id{id} {}
    void lock()   { SDL_LockAudioDevice(id); }
    void unlock() { SDL_UnlockAudioDevice(id); }
};

struct PlayerOptions {
    // int fade_in_secs        = 0;
    int fade_out_ms         = 0;

    bool autoplay_next      = false;
    bool repeat             = false;
    bool shuffle            = false;

    int default_duration    = 3_min;
    int silence_detection   = 0;
    // int loops_limit         = 0;
    double tempo            = 1.0;

    int volume              = SDL_MIX_MAXVOLUME;
};

class Player {
    // emulator and audio device objects.
    // a mutex is needed because audio plays in another thread.
    Music_Emu *emu           = nullptr;
    int id                   = 0;
    SDL_AudioDeviceID dev_id = 0;
    mutable SDLMutex audio_mutex;
    SDL_AudioSpec obtained;
    bool paused = true;

    // current track information:
    TrackInfo track = {
        .metadata = nullptr,
        .length = 0
    };

    // playlist related stuff
    int cur_track = -1;
    int track_count = 0;
    std::vector<int> order;

    // callbacks
    std::function<void(int, gme_info_t *, int)> track_changed;
    std::function<void(int)>                    position_changed;
    std::function<void(void)>                   track_ended;

    // options
    PlayerOptions options = {};

    void audio_callback(void *unused, uint8_t *stream, int stream_length);
    void load_track_without_mutex(int num);

    friend void audio_callback(void *unused, uint8_t *stream, int stream_length);

public:
    Player();
    ~Player();

    Player(const Player &) = delete;
    Player & operator=(const Player &) = delete;

    gme_err_t use_file(std::string_view filename);
    void load_track(int num);
    bool loaded() const;
    bool is_paused() const;
    void start_or_resume();
    void pause();
    std::optional<int> get_next() const;
    std::optional<int> get_prev() const;
    void next();
    void prev();
    void seek(int ms);
    int tell();
    int length() const;
    int effective_length() const;
    int get_index(int trackno) const;
    std::vector<std::string> track_names() const;

    PlayerOptions & get_options();
    void set_fade(int secs);
    void set_tempo(double tempo);
    void set_silence_detection(bool ignore);
    void set_default_duration(int secs);
    void set_autoplay(bool autoplay);
    void set_repeat(bool repeat);
    void set_shuffle(bool shuffle);
    void set_volume(int value);

    void on_track_changed(auto &&fn)    { track_changed    = fn; }
    void on_position_changed(auto &&fn) { position_changed = fn; }
    void on_track_ended(auto &&fn)      { track_ended      = fn; }
};

// this is here for portability
inline constexpr int get_max_volume_value() { return SDL_MIX_MAXVOLUME; }
