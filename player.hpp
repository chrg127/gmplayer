#pragma once

#include <mutex>
#include <string_view>
#include <cstdint>
#include <SDL_audio.h> // SDL_AudioDeviceID

class Music_Emu;
class gme_info_t;

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

class Player {
    // emulator and audio device objects.
    // a mutex is needed because audio plays in another thread.
    Music_Emu *emu           = nullptr;
    int id                   = 0;
    SDL_AudioDeviceID dev_id = 0;
    SDLMutex audio_mutex;
    SDL_AudioSpec obtained;

    // file information:
    int track_count = 0;

    // current track information:
    TrackInfo track = {
        .metadata = nullptr,
        .length = 0
    };
    int cur_track = 0;
    int volume = SDL_MIX_MAXVOLUME;

    // callbacks
    std::function<void(gme_info_t *, int)> track_changed;
    std::function<void(int)>               position_changed;
    std::function<void(void)>              track_ended;

    void load_track(int num);
    void audio_callback(void *unused, uint8_t *stream, int stream_length);

    friend void audio_callback(void *unused, uint8_t *stream, int stream_length);

public:
    Player();
    ~Player();

    Player(const Player &) = delete;
    Player & operator=(const Player &) = delete;

    void use_file(std::string_view filename);
    void start_or_resume();
    void pause();
    void next();
    void prev();
    void seek(int ms);
    void set_volume(int value);

    void on_track_changed(auto &&fn)    { track_changed    = fn; }
    void on_position_changed(auto &&fn) { position_changed = fn; }
    void on_track_ended(auto &&fn)      { track_ended      = fn; }
};

// this is here for portability
inline constexpr int get_max_volume_value() { return SDL_MIX_MAXVOLUME; }
