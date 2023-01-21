#include "player.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <array>
#include <optional>
#include <mutex>
#include <utility>
#include <functional>
#include <algorithm>
#include <filesystem>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <gme/gme.h>
#include "random.hpp"
#include "io.hpp"

namespace fs = std::filesystem;

const int FREQUENCY = 44100;
const int SAMPLES   = 2048;
const int CHANNELS  = 2;



namespace {
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
        if (shuffle)
            std::shuffle(buf.begin(), buf.end(), rng::rng);
    }

    std::error_condition validate(const io::MappedFile &f)
    {
        gme_type_t type;
        auto err = gme_identify_file(f.filename().c_str(), &type);
        if (type == nullptr)
            return std::error_condition(1, gme_error_category);
        auto header = gme_identify_header(f.data());
        if (strcmp(header, "") == 0)
            return std::error_condition(2, gme_error_category);
        return std::error_condition{};
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



std::string GMEErrorCategory::message(int n) const
{
    switch (n) {
    case 0: return "Success";
    case 1: return "Invalid music file type";
    case 2: return "Invalid music file header";
    default: return "Unknown error";
    }
}



void Player::audio_callback(void *, u8 *stream, int len)
{
    if (!emu)
        return;
    // some songs don't have length information, hence the need for the second check.
    if (gme_track_ended(emu) || gme_tell(emu) > track.length + 1_sec/2) {
        SDL_PauseAudioDevice(dev_id, 1);
        track_ended();
        if (options.autoplay)
            next();
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





OpenPlaylistResult Player::open_file_playlist(fs::path path)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    clear_file_playlist();
    auto file = io::File::open(path, io::Access::Read);
    if (!file)
        return { .pl_error = file.error().default_error_condition() };

    OpenPlaylistResult r = { .pl_error = std::error_condition{} };

    auto try_open_file = [&](std::string_view name) -> std::optional<io::MappedFile> {
        for (auto p : { fs::path(name), path.parent_path() / name }) {
            auto f = io::MappedFile::open(p);
            if (!f)
                r.errors.push_back(f.error().default_error_condition());
            else if (auto err = validate(f.value()); err)
                r.errors.push_back(err);
            else
                return std::move(f.value());
        }
        return std::nullopt;
    };

    for (std::string line; file.value().get_line(line); ) {
        if (auto contents = try_open_file(line); contents)
            files.cache.push_back(std::move(contents.value()));
        else
            r.not_opened.push_back(line);
    }

    files.order.resize(files.cache.size());
    generate_order(files.order, false);
    file_order_changed(file_names(), false);
    return r;
}

std::error_condition Player::add_file(fs::path path)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    auto file = io::MappedFile::open(path);
    if (!file)
        return file.error().default_error_condition();
    if (auto err = validate(file.value()); err)
        return err;
    files.cache.push_back(std::move(file.value()));
    files.order.resize(files.cache.size());
    generate_order(files.order, false);
    file_order_changed(file_names(), false);
    return std::error_condition{};
}

bool Player::remove_file(int fileno)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    if (fileno == files.current)
        return false;
    files.cache.erase(files.cache.begin() + fileno);
    files.order.resize(files.cache.size());
    generate_order(files.order, false);
    file_order_changed(file_names(), false);
    return true;
}

void Player::save_file_playlist(io::File &to)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    for (auto i : files.order)
        fprintf(to.data(), "%s\n", files.cache[i].file_path().c_str());
}

void Player::clear_file_playlist()
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    files.cache.clear();
    files.order.clear();
    files.current = -1;
    file_order_changed(file_names(), false);
}

int Player::load_file(int fileno)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    files.current = fileno;
    int num = files.order[fileno];
    auto &file = files.cache[num];
    auto err = gme_open_data(file.data(), file.size(), &emu, 44100);
    if (err)
        load_file_error(file.filename(), std::error_condition(3, gme_error_category), err);
    // when loading a file, try to see if there's a m3u file too
    // m3u files must have the same name as the file, but with extension m3u
    // if there are any errors, ignore them (m3u loading is not important)
    auto m3u_path = file.file_path().replace_extension("m3u");
#ifdef DEBUG
    err = gme_load_m3u(emu, m3u_path.c_str());
    if (err)
        fprintf(stderr, "warning: m3u: %s\n", err);
#else
    gme_load_m3u(emu, m3u_path.c_str());
#endif
    tracks.count = gme_track_count(emu);
    tracks.order.resize(tracks.count);
    generate_order(tracks.order, false);
    track_order_changed(track_names(), false);
    file_changed(fileno);
    return num;
}

int Player::load_track(int trackno)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    tracks.current = trackno;
    int num = tracks.order[trackno];
    auto filename = files.cache[files.order[files.current]].filename();
    if (auto err = gme_track_info(emu, &track.metadata, num); err)
        load_track_error(filename, "", num, std::error_condition(4, gme_error_category), err);
    track.length = get_track_length(track.metadata, options.default_duration);
    if (auto err = gme_start_track(emu, num); err)
        load_track_error(filename, track.metadata->song, num, std::error_condition(4, gme_error_category), err);
    if (track.length < options.fade_out_ms)
        options.fade_out_ms = track.length;
    if (options.fade_out_ms != 0)
        gme_set_fade(emu, track.length - options.fade_out_ms);
    gme_set_tempo(emu, options.tempo);
    track_changed(trackno, track.metadata, track.length);
    return num;
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
    played();
}

void Player::pause()
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    SDL_PauseAudioDevice(dev_id, 1);
    paused();
}

void Player::play_pause()
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    if (is_playing())
        pause();
    else
        start_or_resume();
}

void Player::stop()
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    load_file(0);
    load_track(0);
    pause();
    stopped();
}

void Player::next()
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    if (auto next = get_next_track(); next)
        load_track(next.value());
    else if (auto next = get_next_file(); next)
        load_file(next.value());
}

void Player::prev()
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    if (auto prev = get_prev_track(); prev)
        load_track(prev.value());
    else if (auto prev = get_prev_file(); prev) {
        load_file (prev.value());
        load_track(tracks.count - 1);
    }
}

void Player::seek(int ms)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    int len = effective_length();
    ms = ms < 0   ? 0 : ms > len ? len : ms;
    if (auto err = gme_seek(emu, ms); err)
        seek_error(std::error_condition(5, gme_error_category), err);
    // fade disappears on seek for some reason
    if (options.fade_out_ms != 0)
        gme_set_fade(emu, track.length - options.fade_out_ms);
}

void Player::seek_relative(int off) { seek(position() + off); }

int Player::current_track() const
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    return tracks.current;
}

int Player::current_file() const
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    return files.current;
}

std::optional<int> Player::get_next_file() const
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    if (options.file_repeat)                    return files.current;
    if (files.current + 1 < files.cache.size()) return files.current + 1;
    return std::nullopt;
}

std::optional<int> Player::get_prev_file() const
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    if (options.file_repeat)     return files.current;
    if (files.current - 1 >= 0)  return files.current - 1;
    return std::nullopt;
}

std::optional<int> Player::get_next_track() const
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    if (options.track_repeat)              return tracks.current;
    if (tracks.current + 1 < tracks.count) return tracks.current + 1;
    return std::nullopt;
}

std::optional<int> Player::get_prev_track() const
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    if (options.track_repeat)    return tracks.current;
    if (tracks.current - 1 >= 0) return tracks.current - 1;
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

std::vector<std::string> Player::file_names() const
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    std::vector<std::string> names;
    for (auto i : files.order)
        names.push_back(files.cache[i].file_path().stem().string());
    return names;
}

std::vector<std::string> Player::track_names() const
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    std::vector<std::string> names;
    for (auto i : tracks.order) {
        gme_info_t *info;
        gme_track_info(emu, &info, i);
        names.push_back(info->song[0] ? info->song
                                      : std::string("Track ") + std::to_string(i));
    }
    return names;
}

void Player::shuffle_tracks(bool do_shuffle)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    generate_order(tracks.order, do_shuffle);
    track_order_changed(track_names(), false);
}

void Player::shuffle_files(bool do_shuffle)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    generate_order(files.order, do_shuffle);
    file_order_changed(file_names(), do_shuffle);
}

int Player::move_track(int n, int where, int min, int max)
{
    if (n < min || n > max)
        return n;
    std::swap(tracks.order[n], tracks.order[n + where]);
    track_order_changed(track_names(), false);
    return n + where;
}

int Player::move_file(int n, int where, int min, int max)
{
    if (n < min || n > max)
        return n;
    std::swap(files.order[n], files.order[n + where]);
    file_order_changed(file_names(), false);
    return n + where;
}

int Player::move_track_up(int trackno)   { return move_track(trackno, -1, 1, tracks.order.size() - 1); }
int Player::move_track_down(int trackno) { return move_track(trackno, +1, 0, tracks.order.size() - 2); }
int Player::move_file_up(int fileno)     { return move_file(  fileno, -1, 1, files.order.size() - 1); }
int Player::move_file_down(int fileno)   { return move_file(  fileno, +1, 0, files.order.size() - 2); }



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
    tempo_changed(tempo);
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

void Player::set_autoplay(bool value)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    options.autoplay = value;
    repeat_changed(options.autoplay, options.track_repeat, options.file_repeat);
}

void Player::set_track_repeat(bool value)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    options.track_repeat = value;
    repeat_changed(options.autoplay, options.track_repeat, options.file_repeat);
}

void Player::set_file_repeat(bool value)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    options.file_repeat = value;
    repeat_changed(options.autoplay, options.track_repeat, options.file_repeat);
}

void Player::set_volume(int value)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    options.volume = value;
    volume_changed(options.volume);
}

void Player::set_volume_relative(int offset)
{
    std::lock_guard<SDLMutex> lock(audio_mutex);
    options.volume += offset;
    volume_changed(options.volume);
}
