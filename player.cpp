#include <cassert>
#include <cstdint>
#include <array>
#include <optional>
#include <mutex>
#include <utility>
#include <functional>
#include <algorithm>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <fmt/core.h>
#include <gme/gme.h>
#include "player.hpp"
#include "random.hpp"

using u8  = uint8_t;
using u32 = uint32_t;

const int FREQUENCY = 44100;
const int SAMPLES   = 2048;
const int CHANNELS  = 2;



namespace {
    void dump_info(gme_info_t *info)
    {
        fmt::print(
            "length = {}\n"
            "intro length = {}\n"
            "loop length = {}\n"
            "play length = {}\n"
            "system = {}\n"
            "game = {}\n"
            "song = {}\n"
            "author = {}\n"
            "copyright = {}\n"
            "comment = {}\n"
            "dumper = {}\n",
            info->length, info->intro_length, info->loop_length, info->play_length,
            info->system, info->game, info->song, info->author, info->copyright,
            info->comment, info->dumper
        );
    }

    int get_track_length(gme_info_t *info, int default_duration = 3_min)
    {
        if (info->length > 0)
            return info->length;
        if (info->loop_length > 0)
            return info->intro_length + info->loop_length * 2;
        return default_duration;
    }

    void generate_order(std::span<int> buf, bool shuffle)
    {
        std::iota(buf.begin(), buf.end(), 0);
        if (shuffle) {
            rng::shuffle_in_place(buf);
        }
    }

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
    // some songs don't have length information, hence the need for the third check.
    if (!emu
     || gme_track_ended(emu)
     || gme_tell(emu) > track.length + 1_sec/2) {
        SDL_PauseAudioDevice(dev_id, 1);
        track_ended();

        if (options.autoplay_next) {
            auto next = get_next();
            if (next)
                load_track_without_mutex(next.value());
        }

        return;
    }

    // fill stream with silence. this is needed for MixAudio to work how we want.
    std::memset(stream, 0, len);
    short buf[SAMPLES * CHANNELS];
    gme_play(emu, SAMPLES * CHANNELS, buf);
    // we could also use memcpy here, but then we wouldn't have volume control
    SDL_MixAudioFormat(stream, (const u8 *) buf, obtained.format, sizeof(buf), volume);
    position_changed(gme_tell(emu));
}

void Player::set_fade(int length, int ms)
{
    if (!options.fade_out)
        return;
    if (ms > length)
        return;
    gme_set_fade(emu, length - ms);
}

void Player::use_file(std::string_view filename)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    gme_open_file(filename.data(), &emu, 44100);
    track_count = gme_track_count(emu);
    cur_track = 0;
    order.resize(track_count);
    generate_order(order, options.shuffle);
}

void Player::load_track_without_mutex(int index)
{
    cur_track = index;
    int num = order[index];
    gme_track_info(emu, &track.metadata, num);
    track.length = get_track_length(track.metadata, options.default_duration);
    gme_start_track(emu, num);
    set_fade(track.length, options.fade_out_ms);
    track_changed(num, track.metadata, track.length);
}

void Player::load_track(int index)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    load_track_without_mutex(index);
}

bool Player::playing() const
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    return cur_track != -1;
}

void Player::start_or_resume()
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    SDL_PauseAudioDevice(dev_id, 0);
}

void Player::pause()
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    SDL_PauseAudioDevice(dev_id, 1);
}

std::optional<int> Player::get_next() const
{
    if (cur_track + 1 < track_count)
        return cur_track + 1;
    if (options.repeat)
        return cur_track;
    return std::nullopt;
}

std::optional<int> Player::get_prev() const
{
    if (cur_track - 1 >= 0)
        return cur_track - 1;
    if (options.repeat)
        return cur_track;
    return std::nullopt;
}

void Player::next()
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    auto next = get_next();
    if (next)
        load_track_without_mutex(next.value());
}

void Player::prev()
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    auto prev = get_prev();
    if (prev)
        load_track_without_mutex(prev.value());
}

void Player::seek(int ms)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    gme_seek(emu, ms);
    // fade disappears on seek for some reason
    set_fade(track.length, options.fade_out_ms);
}

void Player::set_volume(int value)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    volume = value;
}

std::vector<std::string> Player::track_names() const
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

    // automatically change some of the current song's attributes
    // return if no song playing
    if (cur_track == -1)
        return;

    if (options.silence_detection > 0)
        gme_ignore_silence(emu, true);
    gme_set_tempo(emu, options.tempo);
    set_fade(track.length, options.fade_out_ms);
    generate_order(order, options.shuffle);
}
