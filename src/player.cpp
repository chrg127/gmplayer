#include "player.hpp"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <span>
#include <SDL.h>
#include <gme/gme.h>
#include "random.hpp"
#include "io.hpp"

namespace fs = std::filesystem;

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
void audio_callback(void *, u8 *stream, int len)
{
    object_handler.get().audio_callback({stream, std::size_t(len)});
}

void Playlist::regen() { std::iota(order.begin(), order.end(), 0); }
void Playlist::regen(int size) { order.resize(size); regen(); }
void Playlist::shuffle() { std::shuffle(order.begin(), order.end(), rng::rng); }


void Player::audio_callback(std::span<u8> stream)
{
    if (!format)
        return;
    auto pos = format->position();
    // some songs don't have length information, hence the need for the second check.
    if (format->track_ended() || pos > length()) {
        SDL_PauseAudioDevice(audio.dev_id, 1);
        track_ended();
        if (opts.autoplay)
            next();
    } else {
        // fill stream with silence. this is needed for MixAudio to work how we want.
        std::fill(stream.begin(), stream.end(), 0);
        auto res = format->play();
        if (res) {
            auto &samples = res.value();
            // we could also use memcpy here, but then we wouldn't have volume control
            SDL_MixAudioFormat(stream.data(), samples.data(), audio.spec.format, samples.size(), opts.volume);
            mpris->set_position(pos * 1000);
            position_changed(pos);
        }
    }
}



Player::Player(PlayerOptions &&options)
{
    opts.autoplay          = options.autoplay;
    opts.silence_detection = options.silence_detection;
    opts.default_duration  = options.default_duration;
    opts.fade_out          = options.fade_out;
    opts.tempo             = options.tempo;
    opts.volume            = options.volume;
    files.repeat           = options.file_repeat;
    tracks.repeat          = options.track_repeat;

    SDL_AudioSpec desired;
    std::memset(&desired, 0, sizeof(desired));
    desired.freq     = 44100;
    desired.format   = AUDIO_S16SYS;
    desired.channels = CHANNELS;
    desired.samples  = SAMPLES;
    desired.callback = ::audio_callback;
    desired.userdata = nullptr;
    audio.dev_id     = SDL_OpenAudioDevice(nullptr, 0, &desired, &audio.spec, 0);
    audio.mutex      = SDLMutex(audio.dev_id);

    audio.id = object_handler.add(this);
    object_handler.change_cur_to(audio.id);
    format = std::make_unique<GME>();

    mpris = mpris::Server::make("gmplayer");
    mpris->set_maximum_rate(2.0);
    mpris->set_minimum_rate(0.5);
    mpris->set_rate(opts.tempo);
    mpris->set_volume(opts.volume);
    mpris->on_pause(           [=, this]                   { pause();               });
    mpris->on_play(            [=, this]                   { start_or_resume();     });
    mpris->on_play_pause(      [=, this]                   { play_pause();          });
    mpris->on_stop(            [=, this]                   { stop();                });
    mpris->on_next(            [=, this]                   { next();                });
    mpris->on_previous(        [=, this]                   { prev();                });
    mpris->on_seek(            [=, this] (int64_t offset)  { seek_relative(offset); });
    mpris->on_rate_changed(    [=, this] (double rate)     { set_tempo(rate);       });
    mpris->on_set_position(    [=, this] (int64_t pos)     { seek(pos);             });
    mpris->on_shuffle_changed( [=, this] (bool do_shuffle) {
        if (do_shuffle)
            files.shuffle();
        else
            files.regen();
        load_file(0);
    });
    mpris->on_volume_changed(  [=, this] (double vol) {
        set_volume(std::lerp(0.0, get_max_volume_value(), vol));
    });
    mpris->on_loop_status_changed([=, this] (mpris::LoopStatus status) {
        switch (status) {
        case mpris::LoopStatus::None:
            set_track_repeat(false);
            set_file_repeat(false);
            break;
        case mpris::LoopStatus::Track:
            set_track_repeat(true);
            set_file_repeat(true);
            break;
        case mpris::LoopStatus::Playlist:
            set_track_repeat(false);
            set_file_repeat(false);
            mpris->set_loop_status(mpris::LoopStatus::None);
            break;
        }
    });

    mpris->start_loop_async();
}

Player::~Player()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    SDL_CloseAudioDevice(audio.dev_id);
}

bool Player::no_file_loaded() const
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    return files.current == -1 || tracks.current == -1;
}

Error Player::add_file_internal(fs::path path)
{
    auto file = io::MappedFile::open(path, io::Access::Read);
    if (!file)
        return file.error().default_error_condition();
    file_cache.push_back(std::move(file.value()));
    files.order.push_back(files.order.size());
    return Error{};
}

OpenPlaylistResult Player::open_file_playlist(fs::path path)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    auto file = io::File::open(path, io::Access::Read);
    if (!file)
        return { .pl_error = file.error().default_error_condition() };
    track_cache.clear();
    file_cache.clear();
    tracks.clear();
    files.clear();
    OpenPlaylistResult r;
    for (std::string line; file.value().get_line(line); ) {
        auto p = fs::path(line);
        if (p.is_relative())
            p = path.parent_path() / p;
        if (auto err = add_file_internal(p); err)
            r.errors.push_back({line, Error{err}});
    }
    mpris->set_shuffle(false);
    playlist_changed(List::File);
    return r;
}

Error Player::add_file(fs::path path)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (auto err = add_file_internal(path); err)
        return err;
    playlist_changed(List::File);
    return Error{};
}

void Player::remove_file(int fileno)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    // if (fileno == files.current)
    //     return make_err(Error::RemoveFile);
    files.remove(fileno);
    playlist_changed(List::File);
}

Error Player::load_file(int fileno)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    files.current = fileno;
    auto res      = read_file(current_file(), 44100);
    if (!res)
        return res.error();
    format = std::move(res.value());
    track_cache.clear();
    for (int i = 0; i < format->track_count(); i++)
        track_cache.push_back(format->track_metadata(i));
    tracks.regen(track_cache.size());
    playlist_changed(List::Track);
    file_changed(fileno);
    return Error{};
}

Error Player::load_track(int trackno)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    tracks.current = trackno;
    auto num       = tracks.order[tracks.current];
    if (auto err = format->start_track(num); err)
        return err;
    auto &metadata = track_cache[num];
    format->set_fade(metadata.length, opts.fade_out);
    format->set_tempo(opts.tempo);
    mpris->set_metadata({
        { mpris::Field::TrackId, std::string("/") + std::to_string(files.current)
                                                  + std::to_string(tracks.current) },
        { mpris::Field::Length,              metadata.length                       },
        { mpris::Field::Title,   std::string(metadata.song)                        },
        { mpris::Field::Album,   std::string(metadata.game)                        },
        { mpris::Field::Artist,  std::string(metadata.author)                      }
    });
    track_changed(trackno, metadata);
    return Error{};
}

// std::error_condition Player::load_m3u()
// {
//     auto err = format->load_m3u(current_file().file_path().replace_extension("m3u"));
//     return err ?  make_err(Error::LoadM3U) : std::error_condition{};
// }

void Player::save_playlist(List which, io::File &to)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    for (auto i : (which == List::Track ? tracks.order : files.order))
        fprintf(to.data(), "%s\n", file_cache[i].file_path().c_str());
}

void Player::clear()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    file_cache.clear();
    tracks.clear();
    files.clear();
    mpris->set_shuffle(false);
    playlist_changed(List::Track);
    playlist_changed(List::File);
}

const io::MappedFile &Player::current_file()  const { std::lock_guard<SDLMutex> lock(audio.mutex); return  file_cache[ files.order[ files.current]]; }
const       Metadata &Player::current_track() const { std::lock_guard<SDLMutex> lock(audio.mutex); return track_cache[tracks.order[tracks.current]]; }

bool Player::is_playing() const
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    return SDL_GetAudioDeviceStatus(audio.dev_id) == SDL_AUDIO_PLAYING;
}

void Player::start_or_resume()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (no_file_loaded())
        return;
    SDL_PauseAudioDevice(audio.dev_id, 0);
    mpris->set_playback_status(mpris::PlaybackStatus::Playing);
    played();
}

void Player::pause()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (no_file_loaded())
        return;
    SDL_PauseAudioDevice(audio.dev_id, 1);
    mpris->set_playback_status(mpris::PlaybackStatus::Paused);
    paused();
}

void Player::play_pause()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (no_file_loaded())
        return;
    if (is_playing())
        pause();
    else
        start_or_resume();
}

void Player::stop()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (no_file_loaded())
        return;
    load_file(0);
    load_track(0);
    pause();
    stopped();
}

Error Player::seek(int ms)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (no_file_loaded())
        if (auto err = format->seek(std::clamp(ms, 0, length())); err)
            return err;
    return Error{};
}

void Player::seek_relative(int off) { seek(position() + off); }

int Player::position()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    return no_file_loaded() ? 0 : format->position();
}

int Player::length() const
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    return no_file_loaded() ? 0 : current_track().length + opts.fade_out;
}



void Player::next()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (no_file_loaded())
        return;
    if (auto next = tracks.next(); next)
        load_track(next.value());
    else if (auto next = files.next(); next)
        load_file(next.value());
}

void Player::prev()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (no_file_loaded())
        return;
    if (auto prev = tracks.prev(); prev)
        load_track(prev.value());
    else if (auto prev = files.prev(); prev) {
        load_file (prev.value());
        load_track(tracks.order.size() - 1);
    }
}

bool Player::has_next() const
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    return !no_file_loaded() && (tracks.next() || files.next());
}

bool Player::has_prev() const
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    return !no_file_loaded() && (tracks.prev() || files.prev());
}

void Player::shuffle(List which)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (which == List::Track)
        tracks.shuffle();
    else {
        files.shuffle();
        mpris->set_shuffle(true);
    }
    playlist_changed(which);
}

int Player::move(List which, int n, int pos)
{
    auto r = which == List::Track ? tracks.move(n, pos) : files.move(n, pos);
    playlist_changed(which);
    return r;
}

std::vector<std::string> Player::names(List which) const
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    std::vector<std::string> names;
    if (which == List::File)
        for (auto i : files.order)
            names.push_back(file_cache[i].file_path().stem().string());
    else {
        for (int i = 0; i < track_cache.size(); i++) {
            auto &track = track_cache[i];
            names.push_back(!track.song.empty() ? std::string(track.song)
                                                : std::string("Track ") + std::to_string(i));
        }
    }
    return names;
}



PlayerOptions Player::options()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    return PlayerOptions {
        .fade_out          = opts.fade_out,
        .autoplay          = opts.autoplay,
        .track_repeat      = tracks.repeat,
        .file_repeat       = files.repeat,
        .default_duration  = opts.default_duration,
        .silence_detection = opts.silence_detection,
        .tempo             = opts.tempo,
        .volume            = opts.volume,
    };
}

void Player::set_fade(int secs)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    opts.fade_out = secs * 1000;
    if (!no_file_loaded() && opts.fade_out != 0) {
        format->set_fade(current_track().length, opts.fade_out);
        fade_changed(length());
    }
}

void Player::set_tempo(double tempo)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    opts.tempo = tempo;
    if (!no_file_loaded()) {
        format->set_tempo(tempo);
        mpris->set_rate(tempo);
    }
    tempo_changed(tempo);
}

void Player::set_silence_detection(bool value)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    opts.silence_detection = value;
    if (!no_file_loaded())
        format->ignore_silence(opts.silence_detection);
}

void Player::set_default_duration(int secs)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    opts.default_duration = secs * 1000;
}

void Player::set_autoplay(bool value)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    opts.autoplay = value;
}

void Player::set_track_repeat(bool value)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    tracks.repeat = value;
    mpris->set_loop_status(tracks.repeat ? mpris::LoopStatus::Track : mpris::LoopStatus::None);
    repeat_changed(tracks.repeat, files.repeat);
}

void Player::set_file_repeat(bool value)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    files.repeat = value;
    mpris->set_loop_status(tracks.repeat ? mpris::LoopStatus::Track : mpris::LoopStatus::None);
    repeat_changed(tracks.repeat, files.repeat);
}

void Player::set_volume(int value)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    opts.volume = value;
    mpris->set_volume(double(opts.volume) / double(get_max_volume_value()));
    volume_changed(opts.volume);
}

void Player::set_volume_relative(int offset)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    opts.volume += offset;
    volume_changed(opts.volume);
}
