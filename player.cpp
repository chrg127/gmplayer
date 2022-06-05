#include <cassert>
#include <cstdint>
#include <array>
#include <optional>
#include <mutex>
#include <utility>
#include <functional>
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

using SampleArray = std::array<short, SAMPLES * CHANNELS>;


std::mutex audio_mutex;

struct {
    std::vector<std::function<std::optional<SampleArray>(void)>> functions;
    int cur = -1;

    int register_function(auto &&fn)
    {
        functions.emplace_back(fn);
        return functions.size() - 1;
    }

    void change_cur_to(int id) { cur = id; }
    auto & operator[](std::size_t i) { return functions[i]; }
    auto & get() { return functions[cur]; }
} callback_handler;



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
        if (callback_handler.cur == -1)
            return;
        auto res = callback_handler.get()();
        if (!res)
            return;
        auto &samples = res.value();
        std::memcpy(stream, samples.data(), samples.size() * sizeof(short));
        // mix from one buffer into another
        // SDL_MixAudio(stream, (const u8 *) buf, sizeof(buf), SDL_MIX_MAXVOLUME);
    }
} // namespace



Player::Player(QObject *parent)
    : QObject(parent)
{
    std::lock_guard<std::mutex> lock(audio_mutex);
    SDL_AudioSpec desired, obtained;
    std::memset(&desired, 0, sizeof(desired));
    desired.freq       = 44100;
    desired.format     = AUDIO_S16SYS;
    desired.channels   = CHANNELS;
    desired.samples    = SAMPLES;
    desired.callback   = audio_callback;
    desired.userdata   = nullptr;
    dev_id = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    id = callback_handler.register_function([&]() -> std::optional<SampleArray> {
        if (!emu || gme_track_ended(emu))
            return std::nullopt;
        SampleArray buf;
        gme_play(emu, buf.size(), buf.data());
        emit position_changed(gme_tell(emu));
        return buf;
    });
    callback_handler.change_cur_to(id);
}

Player::~Player()
{
    std::lock_guard<std::mutex> lock(audio_mutex);
    gme_delete(emu);
    emu = nullptr;
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
    emit track_changed(track.metadata, track.length);
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
