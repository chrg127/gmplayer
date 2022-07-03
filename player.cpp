#include <cassert>
#include <cstdint>
#include <array>
#include <optional>
#include <mutex>
#include <utility>
#include <functional>
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
    /*
     * problem: we have one single player and we'd like to make it into a class to use
     * actual ctors and dtors (instead of manually calling an init() and free().
     * we can't make it global due to global ctors running before main().
     * solution: let us created more than one player (this is feasible and makes sense anyway, since
     * we can open multiple audio devices) and use this thing here that lets us register more players
     * and select which one to use. audio_callback() will select the 'current' player and call
     * Player::audio_callback for it.
     */
    struct {
        std::vector<Player *> players;
        int cur = 0;

        int add(Player *player)     { players.push_back(player); return players.size() - 1; }
        Player & get()              { return *players[cur]; }
        void change_cur_to(int id)  { cur = id; }
    } object_handler;
}

// this must be in the global scope for the friend declaration inside Player to work.
void audio_callback(void *unused, u8 *stream, int stream_length)
{
    object_handler.get().audio_callback(unused, stream, stream_length);
}



Player::Player()
    : options{{}}
{
    SDL_AudioSpec desired;
    std::memset(&desired, 0, sizeof(desired));
    desired.freq       = 44100;
    desired.format     = AUDIO_S16SYS;
    desired.channels   = CHANNELS;
    desired.samples    = SAMPLES;
    desired.callback   = ::audio_callback;
    desired.userdata   = nullptr;
    dev_id = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    audio_mutex = SDLMutex(dev_id);
    id = object_handler.add(this);
    object_handler.change_cur_to(id);
}

Player::~Player()
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    gme_delete(emu);
    emu = nullptr;
    SDL_CloseAudioDevice(dev_id);
}

void Player::audio_callback(void *, u8 *stream, int len)
{
    if (!emu || gme_track_ended(emu)) {
        fmt::print("ended\n");
        SDL_PauseAudioDevice(dev_id, 1);
        track_ended();
        return;
    }

    // if (gme_tell(emu) + 5 > track.length) {
    //     fmt::print("ended with check\n");
    //     SDL_PauseAudioDevice(dev_id, 1);
    //     track_ended();
    //     return;
    // }

    // fill stream with silence. this is needed for MixAudio to work how we want.
    std::memset(stream, 0, len);
    short buf[SAMPLES * CHANNELS];
    gme_play(emu, SAMPLES * CHANNELS, buf);
    // we could also use memcpy here, but then we wouldn't have volume control
    SDL_MixAudioFormat(stream, (const u8 *) buf, obtained.format, sizeof(buf), volume);
    position_changed(gme_tell(emu));
}

int Player::get_track_length(gme_info_t *info)
{
    if (info->length > 0)
        return info->length;
    if (info->loop_length > 0)
        return info->intro_length + info->loop_length * 2;
    return options.default_duration;
}

void Player::set_fade(int length, int ms)
{
    if (!options.fade_out)
        return;
    if (ms > length)
        return;
    fmt::print("setting fade\n");
    gme_set_fade(emu, length - ms);
}

void Player::use_file(std::string_view filename)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    gme_open_file(filename.data(), &emu, 44100);
    track_count = gme_track_count(emu);
    cur_track = 0;
}

void Player::load_track(int num)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    gme_track_info(emu, &track.metadata, num);
    track.length = get_track_length(track.metadata);
    gme_set_fade(emu, track.length - 6_sec);
    // set_fade(track.length, options.fade_out_secs);
    gme_start_track(emu, num);
    track_changed(num, track.metadata, track.length);
}

void Player::start_or_resume()
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    if (gme_track_ended(emu))
        return;
    SDL_PauseAudioDevice(dev_id, 0);
    // if (gme_track_ended(emu) && cur_track+1 != track_count) {
    //     load_track(++cur_track);
    // }
}

void Player::pause()
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    SDL_PauseAudioDevice(dev_id, 1);
}

bool Player::has_next()
{
    return cur_track+1 < track_count;
}

void Player::next()
{
    if (has_next())
        load_track(++cur_track);
}

bool Player::has_prev()
{
    return cur_track-1 >= 0;
}

void Player::prev()
{
    if (has_prev())
        load_track(--cur_track);
}

void Player::seek(int ms)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    gme_seek(emu, ms);
}

void Player::set_volume(int value)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    volume = value;
}

std::vector<std::string> Player::track_names()
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    std::vector<std::string> names;
    for (int i = 0; i < track_count; i++) {
        gme_info_t *info;
        gme_track_info(emu, &info, i);
        names.push_back(info->song[0] == '\0' ? fmt::format("Track {}", i) : info->song);
    }
    return names;
}

void Player::set_options(PlayerOptions opts)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    options = opts;

    // automatically change some of the current song's [...]
    // return if no song playing
    if (cur_track == -1)
        return;

    set_fade(track.length, options.fade_out_secs);
}
