#include <utility>
#include <vector>
#include <mutex>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <gme/gme.h>
#include <fmt/core.h>

using u8  = uint8_t;
using u32 = uint32_t;

const int FREQUENCY = 44100;
const int SAMPLES   = 4096;
const int CHANNELS  = 2;

inline constexpr std::size_t operator"" _s(unsigned long long secs) { return secs * 1000ull; }

Music_Emu *emu;
std::mutex emu_mutex;

// for debugging
void play_siren(Music_Emu *, long count, short *out)
{
    static double a, a2;
    while (count--)
        *out++ = 0x2000 * std::sin(a += 0.1 + 0.05 * std::sin(a2 += 0.00005));
}

void audio_callback(void *, u8 *stream, int)
{
    short buf[SAMPLES * CHANNELS];
    {
        std::lock_guard<std::mutex> lock(emu_mutex);
        if (!emu || gme_track_ended(emu))
            return;
        gme_play(emu, std::size(buf), buf);
        std::memcpy(stream, buf, sizeof(buf));
        // mix from one buffer into another
        // SDL_MixAudio(stream, (const u8 *) buf, sizeof(buf), SDL_MIX_MAXVOLUME);
    }
}

void print_info(gme_info_t *info)
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

int get_track_length(gme_info_t *info)
{
    if (info->length > 0)
        return info->length;
    if (info->loop_length > 0)
        return info->intro_length + info->loop_length * 2;
    return 150_s; // 2.5 minutes
}

void print_exit(gme_err_t err)
{
    if (err) {
        fmt::print("{}\n", err);
        std::exit(1);
    }
};

int load_track(int num)
{
    gme_info_t *info;
    print_exit(gme_track_info(emu, &info, num));
    print_info(info);
    print_exit(gme_start_track(emu, num));
    int length = get_track_length(info);
    gme_set_fade(emu, length - 6_s);
    return length;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fmt::print(stderr, "usage: {} FILE\n", argv[0]);
        return 1;
    }

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fmt::print("error: couldn't initialize SDL: {}\n", SDL_GetError());
        return 1;
    }

    print_exit(gme_open_file(argv[1], &emu, 44100));
    int track_count = gme_track_count(emu);
    fmt::print("track count: {}\n", track_count);
    int cur_track = 0;
    int length = load_track(cur_track);

    SDL_AudioSpec spec;
    std::memset(&spec, 0, sizeof(spec));
    spec.freq       = 44100;
    spec.format     = AUDIO_S16SYS;
    spec.channels   = CHANNELS;
    spec.samples    = SAMPLES;
    spec.callback   = audio_callback;
    spec.userdata   = nullptr;

    if (SDL_OpenAudio(&spec, nullptr) < 0) {
        fmt::print(stderr, "couldn't open audio: {}\n", SDL_GetError());
        return 1;
    }

    SDL_PauseAudio(0);

    for (bool running = true; running && cur_track != track_count; ) {
        for (SDL_Event ev; SDL_PollEvent(&ev); ) {
            switch (ev.type) {
            case SDL_QUIT:
                running = false;
            }
        }

        {
            std::lock_guard<std::mutex> lock(emu_mutex);
            if (gme_track_ended(emu) && ++cur_track != track_count) {
                fmt::print("\nplaying track {}\n", cur_track);
                length = load_track(cur_track);
            }
        }

        try {
            fmt::print("\r{} / {} (elapsed / length)", gme_tell(emu) / 1000, length / 1000);
        } catch (...) { } // ignore exceptions from libfmt
    }

    fmt::print("\n");

    {
        std::lock_guard<std::mutex> lock(emu_mutex);
        gme_delete(emu);
        emu = nullptr;
    }
    SDL_CloseAudio();
    SDL_Quit();

    return 0;
}
