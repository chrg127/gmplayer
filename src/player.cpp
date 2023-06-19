#include "player.hpp"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <span>
#include <SDL.h>
#include "format.hpp"
#include "random.hpp"
#include "io.hpp"
#include "mpris_server.hpp"

namespace fs = std::filesystem;

namespace gmplayer {

void Playlist::regen()         { std::iota(order.begin(), order.end(), 0); }
void Playlist::regen(int size) { order.resize(size); regen(); }
void Playlist::shuffle()       { std::shuffle(order.begin(), order.end(), rng::rng); }

Player::Player(PlayerOptions &&options)
{
    opts.fade_out           = options.fade_out;
    opts.autoplay           = options.autoplay;
    files.repeat            = options.file_repeat;
    tracks.repeat           = options.track_repeat;
    opts.default_duration   = options.default_duration;
    opts.tempo              = options.tempo;
    opts.volume             = options.volume;

    audio.spec.freq     = 44100;
    audio.spec.format   = /*AUDIO_F32;*/ AUDIO_S16SYS;
    audio.spec.channels = NUM_CHANNELS;
    audio.spec.samples  = NUM_FRAMES;
    audio.spec.userdata = this;
    audio.spec.callback = [] (void *userdata, u8 *stream, int length) {
        ((Player *) userdata)->audio_callback({stream, std::size_t(length)});
    };
    audio.dev_id = SDL_OpenAudioDevice(nullptr, 0, &audio.spec, &audio.spec, 0);
    audio.mutex  = SDLMutex(audio.dev_id);

    format = make_default_format();

    mpris = mpris::make_server("gmplayer");
    mpris->set_maximum_rate(4.0);
    mpris->set_minimum_rate(0.25);
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

void Player::audio_callback(std::span<u8> stream)
{
    auto pos = format->position();
    mpris->set_position(pos * 1000);
    position_changed(pos);
    if (format->track_ended()) {
        SDL_PauseAudioDevice(audio.dev_id, 1);
        track_ended();
        if (opts.autoplay)
            next();
        return;
    }
    std::fill(stream.begin(), stream.end(), 0); // fill stream with silence
    std::array<i16, NUM_FRAMES * NUM_CHANNELS * NUM_VOICES> separated = {};
    std::array<i16, NUM_FRAMES * NUM_CHANNELS>              mixed     = {};
    auto multi = format->is_multi_channel();
    auto err = multi ? format->play(separated) : format->play(mixed);
    if (err) {
        error(err);
        return;
    }
    if (multi) {
        for (auto f = 0u; f < NUM_FRAMES; f += 2) {
            for (auto t = 0u; t < NUM_VOICES; t++) {
                mixed[f*2 + 0] += separated[f*FRAME_SIZE + t*NUM_CHANNELS*2 + 0];
                mixed[f*2 + 1] += separated[f*FRAME_SIZE + t*NUM_CHANNELS*2 + 1];
                mixed[f*2 + 2] += separated[f*FRAME_SIZE + t*NUM_CHANNELS*2 + 2];
                mixed[f*2 + 3] += separated[f*FRAME_SIZE + t*NUM_CHANNELS*2 + 3];
            }
        }
    }
    auto &samples = mixed;
    SDL_MixAudioFormat(stream.data(), (const u8 *) samples.data(), audio.spec.format, samples.size() * sizeof(i16), opts.volume);
    samples_played(separated, mixed);
}

Error Player::add_file_internal(fs::path path)
{
    auto file = io::MappedFile::open(path, io::Access::Read);
    if (!file)
        return Error(file.error().default_error_condition(), path.filename().string());
    file_cache.push_back(std::move(file.value()));
    files.order.push_back(file_cache.size() - 1);
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

std::pair<std::vector<Error>, int> Player::add_files(std::span<std::filesystem::path> paths)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    std::vector<Error> errors;
    for (auto path : paths)
        if (auto err = add_file_internal(path); err)
            errors.push_back(err);
    playlist_changed(List::File);
    return std::make_pair(errors, files.order.size());
}

void Player::remove_file(int fileno)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    files.remove(fileno);
    file_removed(fileno);
    playlist_changed(List::File);
}

bool Player::load_file(int fileno)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    pause();
    track_cache.clear();
    files.current = fileno;
    auto res = read_file(current_file(), 44100, opts.default_duration);
    if (!res) {
        format = make_default_format();
        error(res.error());
    } else {
        format = std::move(res.value());
        for (int i = 0; i < format->track_count(); i++)
            track_cache.push_back(format->track_metadata(i));
    }
    tracks.regen(track_cache.size());
    playlist_changed(List::Track);
    file_changed(fileno);
    return !bool(res);
}

bool Player::load_track(int trackno)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    tracks.current = trackno;
    auto num = tracks.order[tracks.current];
    if (auto err = format->start_track(num); err) {
        error(err);
        return true;
    }
    auto &metadata = track_cache[num];
    format->set_fade(metadata.length, opts.fade_out);
    format->set_tempo(opts.tempo);
    mpris->set_metadata({
        { mpris::Field::TrackId, std::string("/") + std::to_string(files.current)
                                                  + std::to_string(tracks.current) },
        { mpris::Field::Length,              metadata.length                       },
        { mpris::Field::Title,   std::string(metadata.info[Metadata::Song])        },
        { mpris::Field::Album,   std::string(metadata.info[Metadata::Game])        },
        { mpris::Field::Artist,  std::string(metadata.info[Metadata::Author])      }
    });
    track_changed(trackno, metadata);
    return false;
}

void Player::load_pair(int file, int track)
{
    if (load_file(file))
        return;
    load_track(track);
}

void Player::save_playlist(List which, io::File &to)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    for (auto i : (which == List::Track ? tracks.order : files.order))
        fprintf(to.data(), "%s\n", file_cache[i].path().c_str());
}

void Player::clear()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    pause();
    format = make_default_format();
    track_cache.clear();
    file_cache.clear();
    tracks.clear();
    files.clear();
    mpris->set_shuffle(false);
    playlist_changed(List::Track);
    playlist_changed(List::File);
    cleared();
}

const io::MappedFile &Player::current_file()  const { std::lock_guard<SDLMutex> lock(audio.mutex); return  file_cache[ files.order[ files.current]]; }
const       Metadata &Player::current_track() const { std::lock_guard<SDLMutex> lock(audio.mutex); return track_cache[tracks.order[tracks.current]]; }

bool Player::is_multi_channel() const { return format->is_multi_channel(); }

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

void Player::stop()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (files.current == -1 || tracks.current == -1)
        return;
    load_pair(0, 0);
    pause();
}

void Player::seek(int ms)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (auto err = format->seek(std::clamp(ms, 0, length())); err) {
        pause();
        error(err);
        return;
    }
    seeked();
    position_changed(position());
}

void Player::seek_relative(int off) { return seek(position() + off); }

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



void Player::next()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (auto next = tracks.next(); next) {
        load_track(next.value());
    } else if (auto next = files.next(); next) {
        load_pair(next.value(), 0);
    }
}

void Player::prev()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (auto prev = tracks.prev(); prev) {
        load_track(prev.value());
    } else if (auto prev = files.prev(); prev) {
        load_pair(prev.value(), tracks.order.size() - 1);
    }
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
            names.push_back(file_cache[i].path().stem().string());
    else
        for (auto i : tracks.order)
            names.push_back(track_cache[i].info[Metadata::Song]);
    return names;
}



std::vector<std::string> Player::channel_names()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    std::vector<std::string> names;
    for (int i = 0; i < format->channel_count(); i++)
        names.push_back(format->channel_name(i));
    return names;
}

void Player::mute_channel(int index, bool mute)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    format->mute_channel(index, mute);
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
    tempo_changed(tempo);
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

mpris::Server &Player::mpris_server() { return *mpris; }

} // namespace gmplayer
