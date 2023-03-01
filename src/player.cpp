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

namespace gmplayer {

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
    if (format->track_ended()) {
        SDL_PauseAudioDevice(audio.dev_id, 1);
        track_ended();
        if (opts.autoplay)
            next();
    }
    auto res = format->play();
    if (!res) {
        SDL_PauseAudioDevice(audio.dev_id, 1);
        play_error(res.error());
    }
    auto &samples = res.value();
    // fill stream with silence. this is needed for MixAudio to work how we want.
    std::fill(stream.begin(), stream.end(), 0);
    // we could also use memcpy here, but then we wouldn't have volume control
    SDL_MixAudioFormat(stream.data(), samples.data(), audio.spec.format, samples.size(), opts.volume);
    mpris->set_position(pos * 1000);
    position_changed(pos);
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
    desired.callback = gmplayer::audio_callback;
    desired.userdata = nullptr;
    audio.dev_id     = SDL_OpenAudioDevice(nullptr, 0, &desired, &audio.spec, 0);
    audio.mutex      = SDLMutex(audio.dev_id);

    audio.id = object_handler.add(this);
    object_handler.change_cur_to(audio.id);
    format = std::make_unique<Default>();

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
        shuffled(List::File);
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

Error Player::add_file_internal(fs::path path)
{
    auto file = io::MappedFile::open(path, io::Access::Read);
    if (!file)
        return Error(file.error().default_error_condition(), path.filename().string());
    file_cache.push_back(std::move(file.value()));
    files.order.push_back(files.order.size());
    return Error{};
}

Error Player::add_file(fs::path path)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (auto err = add_file_internal(path); err)
        return err;
    playlist_changed(List::File);
    return Error{};
}

std::vector<Error> Player::add_files(std::span<std::filesystem::path> paths)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    std::vector<Error> errors;
    for (auto path : paths)
        if (auto err = add_file_internal(path); err)
            errors.push_back(err);
    playlist_changed(List::File);
    return errors;
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
    pause();
    track_cache.clear();
    files.current = fileno;
    auto res = read_file(current_file(), 44100);
    if (!res) {
        format = std::make_unique<Default>();
        tracks.regen(0);
        playlist_changed(List::Track);
        file_changed(fileno);
        return res.error();
    }
    format = std::move(res.value());
    for (int i = 0; i < format->track_count(); i++)
        track_cache.push_back(format->track_metadata(i, opts.default_duration));
    tracks.regen(track_cache.size());
    playlist_changed(List::Track);
    file_changed(fileno);
    return Error{};
}

Error Player::load_track(int trackno)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    tracks.current = trackno;
    auto num = tracks.order[tracks.current];
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

Error Player::load_pair(int file, int track)
{
    if (auto err = load_file(file); err)
        return err;
    return load_track(track);
}

void Player::save_playlist(List which, io::File &to)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    for (auto i : (which == List::Track ? tracks.order : files.order))
        fprintf(to.data(), "%s\n", file_cache[i].file_path().c_str());
}

void Player::clear()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    track_cache.clear();
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
    if (!format->track_ended()) {
        SDL_PauseAudioDevice(audio.dev_id, 0);
        mpris->set_playback_status(mpris::PlaybackStatus::Playing);
        played();
    }
}

void Player::pause()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    SDL_PauseAudioDevice(audio.dev_id, 1);
    mpris->set_playback_status(mpris::PlaybackStatus::Paused);
    paused();
}

void Player::play_pause()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (is_playing())
        pause();
    else
        start_or_resume();
}

Error Player::stop()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (files.current == -1 || tracks.current == -1)
        return Error{};
    if (auto err = load_file(0); err)
        return err;
    if (auto err = load_track(0); err)
        return err;
    pause();
    stopped();
    return Error{};
}

Error Player::seek(int ms)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (auto err = format->seek(std::clamp(ms, 0, length())); err)
        return err;
    seeked();
    position_changed(position());
    return Error{};
}

Error Player::seek_relative(int off) { return seek(position() + off); }

int Player::position()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    return format->position();
}

int Player::length() const
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    return tracks.current == -1 ? 0 : current_track().length + opts.fade_out;
}



Error Player::next()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (auto next = tracks.next(); next) {
        if (auto err = load_track(next.value()); err)
            return err;
    } else if (auto next = files.next(); next) {
        if (auto err = load_pair(next.value(), 0); err)
            return err;
    }
    return Error{};
}

Error Player::prev()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (auto prev = tracks.prev(); prev) {
        if (auto err = load_track(prev.value()); err)
            return err;
    } else if (auto prev = files.prev(); prev) {
        if (auto err = load_pair(prev.value(), tracks.order.size() - 1); err)
            return err;
    }
    return Error{};
}

bool Player::has_next() const
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    return tracks.next() || files.next();
}

bool Player::has_prev() const
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    return tracks.prev() || files.prev();
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
    shuffled(which);
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
    else
        for (auto i : tracks.order)
            names.push_back(track_cache[i].song);
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
    if (tracks.current != -1)
        format->set_fade(current_track().length, opts.fade_out);
    fade_changed(length());
}

void Player::set_tempo(double tempo)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    opts.tempo = tempo;
    format->set_tempo(tempo);
    mpris->set_rate(tempo);
    // tempo_changed(tempo);
}

void Player::set_silence_detection(bool value)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    opts.silence_detection = value;
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

} // namespace gmplayer
