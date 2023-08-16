#include "player.hpp"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <span>
#include <SDL.h>
#include "random.hpp"
#include "io.hpp"
#include "mpris_server.hpp"
#include "math.hpp"
#include "config.hpp"

namespace fs = std::filesystem;

namespace gmplayer {

void Playlist::regen()         { std::iota(order.begin(), order.end(), 0); }
void Playlist::regen(int size) { order.resize(size); regen(); }
void Playlist::shuffle()       { std::shuffle(order.begin(), order.end(), rng::rng); }

Player::Player()
{
    audio.spec.freq     = 44100;
    audio.spec.format   = AUDIO_F32;//*/ AUDIO_S16SYS;
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
    mpris->set_rate(config.get<float>("tempo"));
    mpris->set_volume(config.get<int>("volume"));
    mpris->on_pause(           [=, this]                   { pause();               });
    mpris->on_play(            [=, this]                   { start_or_resume();     });
    mpris->on_play_pause(      [=, this]                   { play_pause();          });
    mpris->on_stop(            [=, this]                   { stop();                });
    mpris->on_next(            [=, this]                   { next();                });
    mpris->on_previous(        [=, this]                   { prev();                });
    mpris->on_seek(            [=, this] (int64_t offset)  { seek_relative(offset); });
    mpris->on_rate_changed(    [=, this] (double rate)     { config.set<float>("tempo", rate); });
    mpris->on_set_position(    [=, this] (int64_t pos)     { seek(pos);             });
    mpris->on_shuffle_changed( [=, this] (bool do_shuffle) {
        if (do_shuffle)
            files.shuffle();
        else
            files.regen();
        shuffled(Playlist::Type::File);
    });
    mpris->on_volume_changed(  [=, this] (double vol) {
        config.set<int>("volume", std::lerp(0.0, MAX_VOLUME_VALUE, vol));
    });
    mpris->on_loop_status_changed([=, this] (mpris::LoopStatus status) {
        config.set<bool>("repeat_track", status == mpris::LoopStatus::Track);
        config.set<bool>("repeat_file",  status == mpris::LoopStatus::Track);
        if (status == mpris::LoopStatus::Playlist)
            mpris->set_loop_status(mpris::LoopStatus::None);
    });

    mpris->start_loop_async();

    config.when_set("fade", [&](const conf::Value &v) {
        auto fade = v.as<int>();
        if (tracks.current != -1) {
            // reset song to start position
            // (this is due to the modified fade applying to the song)
            seek(0);
            format->set_fade(length(), fade);
        }
        fade_changed(length());
    });

    options.autoplay = config.get<bool>("autoplay");
    config.when_set("autoplay", [&](const conf::Value &v) {
        std::lock_guard<SDLMutex> lock(audio.mutex);
        options.autoplay = v.as<bool>();
    });

    config.when_set("tempo", [&](const conf::Value &v) {
        float tempo = v.as<float>();
        format->set_tempo(tempo);
        mpris->set_rate(tempo);
    });

    tracks.repeat = config.get<bool>("repeat_track");
    config.when_set("repeat_track", [&](const conf::Value &v) {
        tracks.repeat = v.as<bool>();
        mpris->set_loop_status(tracks.repeat ? mpris::LoopStatus::Track : mpris::LoopStatus::None);
        repeat_changed(tracks.repeat, files.repeat);
    });

    files.repeat = config.get<bool>("repeat_track");
    config.when_set("repeat_file", [&](const conf::Value &v) {
        tracks.repeat = v.as<bool>();
        mpris->set_loop_status(tracks.repeat ? mpris::LoopStatus::Track : mpris::LoopStatus::None);
        repeat_changed(tracks.repeat, files.repeat);
    });

    options.volume = config.get<int>("volume");
    config.when_set("volume", [&](const conf::Value &v) {
        std::lock_guard<SDLMutex> lock(audio.mutex);
        options.volume = v.as<int>();
        mpris->set_volume(double(options.volume) / double(MAX_VOLUME_VALUE));
        volume_changed(options.volume);
    });
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
        pause();
        track_ended();
        if (options.autoplay)
            next();
        return;
    }
    std::fill(stream.begin(), stream.end(), 0); // fill stream with silence
    std::array<i16,   NUM_FRAMES * NUM_CHANNELS * NUM_VOICES> separated = {};
    std::array<i16,   NUM_FRAMES * NUM_CHANNELS>              mixed     = {};
    std::array<float, NUM_FRAMES * NUM_CHANNELS>              samples   = {};
    auto multi = format->is_multi_channel();
    auto err = multi ? format->play(separated) : format->play(mixed);
    if (err)
        fmt::print("got error while playing: {}\n", err.details);
    auto maxvol = 1.0f / float(MAX_VOLUME_VALUE);
    if (multi) {
        for (auto f = 0u; f < NUM_FRAMES; f += 2) {
            for (auto t = 0u; t < NUM_VOICES; t++) {
                for (auto i = 0u; i < NUM_CHANNELS*2; i++) {
                    auto vol = float(effects.volume[t]);
                    samples[f*2 + i] += separated[f*FRAME_SIZE + t*NUM_CHANNELS*2 + i] / 32768.f * vol * maxvol;
                }
            }
        }
    } else
        for (auto i = 0u; i < samples.size(); i++)
            samples[i] = mixed[i] / 32768.f;
    SDL_MixAudioFormat(
        stream.data(), (const u8 *) samples.data(), audio.spec.format,
        samples.size() * sizeof(f32), options.volume
    );
    samples_played(separated, samples);
}

std::vector<Player::AddFileError> Player::add_file(std::filesystem::path path)
{
    auto paths = std::array{path};
    return add_files(paths);
}

std::vector<Player::AddFileError> Player::add_files(std::span<fs::path> paths)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    std::vector<Player::AddFileError> errors;
    for (const auto &p : paths)
        io::MappedFile::open(p, io::Access::Read)
            .map([&](auto &&file) {
                file_cache.push_back(std::move(file));
                files.order.push_back(file_cache.size() - 1);
            }).or_else([&](auto err) {
                errors.push_back(std::make_pair(p.filename(), err));
            });
    playlist_changed(Playlist::File);
    return errors;
}

void Player::remove_file(int id)
{
    auto ids = std::array{id};
    remove_files(ids);
}

void Player::remove_files(std::span<int> ids)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    for (auto id : ids)
        files.remove(id);
    files_removed(ids);
    playlist_changed(Playlist::File);
}

void Player::load_file(int id)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    auto res = read_file(file_cache[files.order[id]], 44100, config.get<int>("default_duration"));
    if (!res) {
        error(res.error());
        return;
    }
    format = std::move(res.value());
    files.current = id;
    track_cache.clear();
    for (int i = 0; i < format->track_count(); i++)
        track_cache.push_back(format->track_metadata(i));
    tracks.regen(track_cache.size());
    playlist_changed(Playlist::Track);
    file_changed(id);
}

void Player::load_track(int id)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    tracks.current = id;
    auto num = tracks.order[id];
    if (auto err = format->start_track(num); err) {
        error(err);
        return;
    }
    auto &metadata = track_cache[num];
    format->set_fade(metadata.length, config.get<int>("fade"));
    format->set_tempo(config.get<float>("tempo"));
    mpris->set_metadata({
        { mpris::Field::TrackId, fmt::format("/{}{}", files.current, tracks.current)    },
        { mpris::Field::Length,  metadata.length                                        },
        { mpris::Field::Title,   std::string(metadata.info[Metadata::Song])             },
        { mpris::Field::Album,   std::string(metadata.info[Metadata::Game])             },
        { mpris::Field::Artist,  std::string(metadata.info[Metadata::Author])           }
    });
    track_changed(id, metadata);
}

void Player::load_pair(int file, int track)
{
    load_file(file);
    if (files.current == file)
        load_track(track);
}

void Player::clear()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    pause();
    format = make_default_format();
    track_cache.clear();
    tracks.clear();
    playlist_changed(Playlist::Track);
    file_cache.clear();
    files.clear();
    playlist_changed(Playlist::File);
    mpris->set_shuffle(false);
    cleared();
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
    }
    seeked();
    position_changed(position());
}

void Player::seek_relative(int off) { seek(position() + off); }

void Player::next()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
         if (auto next = tracks.next(); next) load_track(next.value());
    else if (auto next = files.next();  next) load_pair(next.value(), 0);
}

void Player::prev()
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
         if (auto prev = tracks.prev(); prev) load_track(prev.value());
    else if (auto prev = files.prev();  prev) load_pair(prev.value(), tracks.order.size() - 1);
}

void Player::shuffle(Playlist::Type which)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    if (which == Playlist::Track)
        tracks.shuffle();
    else {
        files.shuffle();
        mpris->set_shuffle(true);
    }
    playlist_changed(which);
    shuffled(which);
}

int Player::move(Playlist::Type which, int n, int pos)
{
    auto r = which == Playlist::Track ? tracks.move(n, pos) : files.move(n, pos);
    playlist_changed(which);
    return r;
}

bool Player::is_playing() const
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    return SDL_GetAudioDeviceStatus(audio.dev_id) == SDL_AUDIO_PLAYING;
}

int Player::position() const
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    return format->position();
}

int Player::length() const
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    return tracks.current == -1 ? 0 : track_info(current_track()).length
                                    + config.get<int>("fade");
}

bool Player::is_multi_channel() const { return format->is_multi_channel(); }

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

int Player::current_track() const { std::lock_guard<SDLMutex> lock(audio.mutex); return tracks.current; }
int Player::current_file()  const { std::lock_guard<SDLMutex> lock(audio.mutex); return  files.current; }

int Player::current_of(Playlist::Type type) const
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    return (type == Playlist::Track ? tracks : files).current;
}

int Player::track_count() const { std::lock_guard<SDLMutex> lock(audio.mutex); return tracks.order.size(); }
int Player::file_count()  const { std::lock_guard<SDLMutex> lock(audio.mutex); return  files.order.size(); }

int Player::count_of(Playlist::Type type) const
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    return (type == Playlist::Track ? tracks : files).order.size();
}

const Metadata &       Player::track_info(int id) const { std::lock_guard<SDLMutex> lock(audio.mutex); return track_cache[tracks.order[id]]; }
const io::MappedFile & Player::file_info(int id)  const { std::lock_guard<SDLMutex> lock(audio.mutex); return  file_cache[ files.order[id]]; }

const std::vector<Metadata> Player::file_tracks(int i) const
{
    auto format = gmplayer::read_file(file_cache[files.order[i]], 44100, config.get<int>("default_duration"));
    if (!format)
        return {};
    std::vector<Metadata> v;
    for (int i = 0; i < format.value()->track_count(); i++)
        v.push_back(format.value()->track_metadata(i));
    return v;
}

void Player::loop_tracks(std::function<void(int, const Metadata &)> fn) const
{
    for (auto i : tracks.order)
        fn(i, track_info(i));
}

void Player::loop_files(std::function<void(int, const io::MappedFile &)> fn) const
{
    for (auto i : files.order)
        fn(i, file_info(i));
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

void Player::set_channel_volume(int index, int value)
{
    std::lock_guard<SDLMutex> lock(audio.mutex);
    effects.volume[index] = value;
    channel_volume_changed(index, value);
}

mpris::Server &Player::mpris_server() { return *mpris; }

} // namespace gmplayer
