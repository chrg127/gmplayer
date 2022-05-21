#include <utility>
#include <vector>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <gme/gme.h>
#include <fmt/core.h>

using u8 = uint8_t;
using u32 = uint32_t;

const int FREQUENCY = 44100;
const int SAMPLES   = 4096;
const int CHANNELS  = 2;

Music_Emu *emu;

void audio_callback(void *, u8 *stream, int)
{
    short buf[SAMPLES * CHANNELS];
    gme_play(emu, std::size(buf), buf);
    std::memcpy(stream, buf, sizeof(buf));
    // mix from one buffer into another
    // SDL_MixAudio(stream, (const u8 *) buf, sizeof(buf), SDL_MIX_MAXVOLUME);
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

int main(int argc, char *argv[])
{
    auto print_exit = [](gme_err_t err) {
        if (err) {
            fmt::print("{}\n", err);
            std::exit(1);
        }
    };

    if (argc < 2) {
        fmt::print(stderr, "usage: {} FILE\n", argv[0]);
        return 1;
    }

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fmt::print("error: couldn't initialize SDL: {}\n", SDL_GetError());
        return 1;
    }

    print_exit(gme_open_file(argv[1], &emu, 44100));
    gme_info_t *info;
    print_exit(gme_track_info(emu, &info, 0));
    print_info(info);
    print_exit(gme_start_track(emu, 0));

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
    while (gme_tell(emu) < info->length)
        fmt::print("{} / {} (elapsed / length)\r", gme_tell(emu) / 1000, info->length / 1000);

    gme_delete(emu);
    SDL_CloseAudio();
    SDL_Quit();

    return 0;
}
