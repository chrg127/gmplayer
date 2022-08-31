#include "player.hpp"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <array>
#include <optional>
#include <mutex>
#include <utility>
#include <functional>
#include <algorithm>
#include <filesystem>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <fmt/core.h>
#include <gme/gme.h>
#include "random.hpp"
#include "io.hpp"

namespace fs = std::filesystem;

const int FREQUENCY = 44100;
const int SAMPLES   = 2048;
const int CHANNELS  = 2;



namespace {
    void dump_info(gme_info_t *info)
    {
        fmt::print(stderr,
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

    std::optional<io::MappedFile> try_open_file(fs::path playlist_path, std::string_view filename)
    {
        for (auto p : { fs::path(filename), playlist_path.parent_path() / filename })
            if (auto contents = io::MappedFile::open(p); contents)
                return contents;
        return std::nullopt;
    }
}

// this must be in the global scope for the friend declaration inside Player to work.
void audio_callback(void *unused, u8 *stream, int stream_length)
{
    object_handler.get().audio_callback(unused, stream, stream_length);
}



// private functions

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
    SDL_MixAudioFormat(stream, (const u8 *) buf, obtained.format, sizeof(buf), options.volume);
    position_changed(gme_tell(emu));
}

int Player::load_track_without_mutex(int index)
{
    tracks.current = index;
    int num = tracks.order[index];
    gme_track_info(emu, &track.metadata, num);
    track.length = get_track_length(track.metadata, options.default_duration);
    gme_start_track(emu, num);
    if (track.length < options.fade_out_ms)
        options.fade_out_ms = track.length;
    if (options.fade_out_ms != 0)
        gme_set_fade(emu, track.length - options.fade_out_ms);
    gme_set_tempo(emu, options.tempo);
    track_changed(num, track.metadata, track.length);
    return num;
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



void Player::open_file_playlist(fs::path filename)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    clear_file_playlist();
    auto file = io::File::open(filename, io::Access::Read);
    if (!file) {
        fmt::print(stderr, "can't open file {}\n", filename.c_str());
        return;
    }
    for (std::string line; file.value().get_line(line); ) {
        if (auto contents = try_open_file(filename, line); contents)
            files.cache.push_back(std::move(contents.value()));
        else
            fmt::print(stderr, "can't open file {}\n", line);
    }
}

bool Player::add_file(fs::path path)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    if (auto file = io::MappedFile::open(path); file) {
        files.cache.push_back(std::move(file.value()));
        return true;
    }
    return false;
}

void Player::clear_file_playlist()
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    files.cache.clear();
    files.order.clear();
    files.current = -1;
}

int Player::load_file(int fileno)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    if (files.current == fileno)
        return 0;
    files.current = fileno;
    auto &file = files.cache[fileno];
    gme_open_data(file.data(), file.size(), &emu, 44100);
    // when loading a file, try to see if there's a m3u file too
    // m3u files must have the same name as the file, but with extension m3u
    // if there are any errors, ignore them (m3u loading is not important)
    auto m3u_path = file.file_path().replace_extension("m3u");
#ifdef DEBUG
    err = gme_load_m3u(emu, m3u_path.c_str());
    if (err)
        fmt::print(stderr, "warning: m3u: {}\n", err);
#else
    gme_load_m3u(emu, m3u_path.c_str());
#endif
    tracks.count = gme_track_count(emu);
    tracks.order.resize(tracks.count);
    generate_order(tracks.order, options.shuffle);
    return 0;
}

int Player::load_track(int index)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    return load_track_without_mutex(index);
}

bool Player::can_play() const
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    return files.current != -1 && tracks.current != -1;
}

bool Player::is_playing() const
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    auto status = SDL_GetAudioDeviceStatus(dev_id);
    return status == SDL_AUDIO_PLAYING;
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
    int len = effective_length();
    if (ms < 0)
        ms = 0;
    if (ms > len)
        ms = len;
    gme_seek(emu, ms);
    // fade disappears on seek for some reason
    if (options.fade_out_ms != 0)
        gme_set_fade(emu, track.length - options.fade_out_ms);
}



std::optional<int> Player::get_next() const
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    if (tracks.current + 1 < tracks.count)
        return tracks.current + 1;
    if (options.repeat)
        return tracks.current;
    return std::nullopt;
}

std::optional<int> Player::get_prev() const
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    if (tracks.current - 1 >= 0)
        return tracks.current - 1;
    if (options.repeat)
        return tracks.current;
    return std::nullopt;
}

int Player::position()
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    return gme_tell(emu);
}

int Player::length() const
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    return track.length;
}

int Player::effective_length() const
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    // the fade seems like it lasts for 8 seconds
    return options.fade_out_ms == 0
        ? track.length
        : std::min<int>(track.length, track.length - options.fade_out_ms + 8_sec);
}

void Player::file_names(std::function<void(const std::string &)> f) const
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    for (auto &file : files.cache)
        f(file.file_path().stem().string());
}

void Player::track_names(std::function<void(const std::string &)> f) const
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    for (int i = 0; i < tracks.count; i++) {
        gme_info_t *info;
        gme_track_info(emu, &info, i);
        f(info->song[0] == '\0' ? fmt::format("Track {}", i) : info->song);
    }
}



PlayerOptions & Player::get_options()
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    return options;
}

void Player::set_fade(int secs)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    int ms = secs * 1000;
    options.fade_out_ms = ms;
    if (tracks.current != -1 && options.fade_out_ms != 0)
        gme_set_fade(emu, track.length - options.fade_out_ms);
}

void Player::set_tempo(double tempo)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    options.tempo = tempo;
    if (tracks.current != -1)
        gme_set_tempo(emu, tempo);
}

void Player::set_silence_detection(bool ignore)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    options.silence_detection = ignore ? 0 : 1;
    if (tracks.current != -1)
        gme_ignore_silence(emu, options.silence_detection);
}

void Player::set_default_duration(int secs)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    options.default_duration = secs * 1000;
}

void Player::set_autoplay(bool autoplay)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    options.autoplay_next = autoplay;
}

void Player::set_repeat(bool repeat)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    options.repeat = repeat;
}

void Player::set_shuffle(bool shuffle)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    options.shuffle = shuffle;
    generate_order(tracks.order, options.shuffle);
}

void Player::set_volume(int value)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    options.volume = value;
}
