#include <cstdint>
#include <mutex>
#include <utility>
#include <QString>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <fmt/core.h>
#include <gme/gme.h>
#include "player.hpp"

using u8  = uint8_t;
using u32 = uint32_t;

const int FREQUENCY = 44100;
const int SAMPLES   = 2048;
const int CHANNELS  = 2;

namespace {
    constexpr std::size_t operator"" _s(unsigned long long secs) { return secs * 1000ull; }

    int get_track_length(gme_info_t *info)
    {
        if (info->length > 0)
            return info->length;
        if (info->loop_length > 0)
            return info->intro_length + info->loop_length * 2;
        return 150_s; // 2.5 minutes
    }
} // namespace



struct Player {
    std::mutex emu_mutex;
    Music_Emu *emu           = nullptr;
    SDL_AudioDeviceID dev_id = 0;

    // file information:
    int track_count = 0;

    // current track information:
    gme_info_t *info = nullptr;
    int cur_track    = 0;
    int length       = 0;

    // player related state:
    bool stopped = true;

    void use_file(const char *filename)
    {
        std::lock_guard<std::mutex> lock(emu_mutex);
        gme_open_file(filename, &emu, 44100);
        track_count = gme_track_count(emu);
        cur_track = 0;
        load_track(0);
    }

    void load_track(int num)
    {
        gme_track_info(emu, &info, num);
        length = get_track_length(info);
        gme_start_track(emu, num);
    }

    void start_or_resume()
    {
        std::lock_guard<std::mutex> lock(emu_mutex);
        SDL_PauseAudioDevice(dev_id, 0);
        // if (gme_track_ended(emu) && cur_track+1 != track_count) {
        //     load_track(++cur_track);
        // }
    }

    void pause()
    {
        std::lock_guard<std::mutex> lock(emu_mutex);
        SDL_PauseAudioDevice(dev_id, 1);
    }

    void stop()
    {
        std::lock_guard<std::mutex> lock(emu_mutex);
        SDL_PauseAudioDevice(dev_id, 1);
    }

    void next()
    {
        std::lock_guard<std::mutex> lock(emu_mutex);
        cur_track++;
    }

    void prev()
    {
        std::lock_guard<std::mutex> lock(emu_mutex);
        cur_track++;
    }

    int get_track_time()
    {
        std::lock_guard<std::mutex> lock(emu_mutex);
        return gme_tell(emu);
    }
} music_player;



namespace {

void audio_callback(void *, u8 *stream, int)
{
    std::lock_guard<std::mutex> lock(music_player.emu_mutex);
    if (!music_player.emu
     || gme_track_ended(music_player.emu))
        return;
    short buf[SAMPLES * CHANNELS];
    gme_play(music_player.emu, std::size(buf), buf);
    std::memcpy(stream, buf, sizeof(buf));
    // mix from one buffer into another
    // SDL_MixAudio(stream, (const u8 *) buf, sizeof(buf), SDL_MIX_MAXVOLUME);
}

} // namespace



// public functions:

namespace player {

void init()
{
    SDL_AudioSpec desired, obtained;
    std::memset(&desired, 0, sizeof(desired));
    desired.freq       = 44100;
    desired.format     = AUDIO_S16SYS;
    desired.channels   = CHANNELS;
    desired.samples    = SAMPLES;
    desired.callback   = audio_callback;
    desired.userdata   = nullptr;
    music_player.dev_id = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
}

void quit()
{
    SDL_CloseAudioDevice(music_player.dev_id);
}

void use_file(const QString &filename)
{
    music_player.use_file(filename.toUtf8().constData());
}

void start_or_resume() { music_player.start_or_resume(); }
void pause()           { music_player.pause(); }
void stop()            { music_player.stop(); }
void next()            { music_player.next(); }
void prev()            { music_player.prev(); }

Metadata get_track_metadata()
{
    return {
        .info   = music_player.info,
        .length = music_player.length,
    };
}

int get_track_time() { return music_player.get_track_time(); }

} // namespace player
