#include <cassert>
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



std::mutex audio_mutex;
Music_Emu **emulator = nullptr;

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

    void audio_callback(void *, u8 *stream, int)
    {
        std::lock_guard<std::mutex> lock(audio_mutex);
        fmt::print("{}\n", (uintptr_t) *emulator);
        if (!*emulator || gme_track_ended(*emulator))
            return;
        short buf[SAMPLES * CHANNELS];
        gme_play(*emulator, std::size(buf), buf);
        std::memcpy(stream, buf, sizeof(buf));
        // mix from one buffer into another
        // SDL_MixAudio(stream, (const u8 *) buf, sizeof(buf), SDL_MIX_MAXVOLUME);
    }
} // namespace



Player::Player(QObject *parent)
    : QObject(parent)
{
    SDL_AudioSpec desired, obtained;
    std::memset(&desired, 0, sizeof(desired));
    desired.freq       = 44100;
    desired.format     = AUDIO_S16SYS;
    desired.channels   = CHANNELS;
    desired.samples    = SAMPLES;
    desired.callback   = audio_callback;
    desired.userdata   = nullptr;
    dev_id = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    assert(dev_id != 0);
    std::lock_guard<std::mutex> lock(audio_mutex);
    emulator = &emu;
}

Player::~Player()
{
    SDL_CloseAudioDevice(dev_id);
}

void Player::use_file(const char *filename)
{
    std::lock_guard<std::mutex> lock(audio_mutex);
    gme_open_file(filename, &emu, 44100);
    track_count = gme_track_count(emu);
    cur_track = 0;
    load_track(0);
}

void Player::use_file(const QString &filename)
{
    use_file(filename.toUtf8().constData());
}

void Player::load_track(int num)
{
    gme_track_info(emu, &track.metadata, num);
    track.length = get_track_length(track.metadata);
    gme_start_track(emu, num);
}

void Player::start_or_resume()
{
    std::lock_guard<std::mutex> lock(audio_mutex);
    SDL_PauseAudioDevice(dev_id, 0);
    // if (gme_track_ended(emu) && cur_track+1 != track_count) {
    //     load_track(++cur_track);
    // }
}

void Player::pause()
{
    std::lock_guard<std::mutex> lock(audio_mutex);
    SDL_PauseAudioDevice(dev_id, 1);
}

void Player::stop()
{
    std::lock_guard<std::mutex> lock(audio_mutex);
    SDL_PauseAudioDevice(dev_id, 1);
}

void Player::next()
{
    std::lock_guard<std::mutex> lock(audio_mutex);
    cur_track++;
}

void Player::prev()
{
    std::lock_guard<std::mutex> lock(audio_mutex);
    cur_track++;
}

// int get_track_time()
// {
//     std::lock_guard<std::mutex> lock(audio_mutex);
//     return gme_tell(emu);
// }
