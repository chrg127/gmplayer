#include <SDL.h>
#include <fmt/core.h>
#include "player.hpp"
#include "mpris_server.hpp"
#include "config.hpp"
#include "types.hpp"
#include "io.hpp"
#include "terminal.hpp"
#include "math.hpp"

namespace fs = std::filesystem;
using namespace gmplayer::literals;

Config config;

std::vector<fs::path> args_to_paths(int argc, char *argv[])
{
    std::vector<fs::path> files;
    for (int i = 1; i < argc; i++)
        files.push_back(fs::path{argv[i]});
    return files;
}

std::string format_position(int ms, int max)
{
    return fmt::format("{:02}:{:02}/{:02}:{:02}",
                       ms  / 1000 / 60, ms  / 1000 % 60,
                       max / 1000 / 60, max / 1000 % 60);
};

std::string make_slider(int pos, int length, int term_width)
{
    std::string s(term_width, '-');
    s[math::map(pos, 0, length, 0, term_width)] = '+';
    return s;
}

int tempo_to_int(double value) { return math::map(std::log2(value), -2.0, 2.0, 0.0, 100.0); }
double int_to_tempo(int value) { return std::exp2(math::map(double(value), 0.0, 100.0, -2.0, 2.0)); }

struct Status {
    bool paused;
    int tempo;
    int volume;
    bool autoplay;
    bool repeat_file;
    bool repeat_track;
    int position;
    int length;
};

void print_file_info(const io::MappedFile &f)
{
    fmt::print("\r\e[9A"
               "\e[KFile: {}\n"
               "\n\n\n\n\n\n\n\n",
               f.name());
    std::fflush(stdout);
}

void print_metadata(const gmplayer::Metadata &m)
{
    using enum gmplayer::Metadata::Field;
    fmt::print("\r\e[8A"
               "\e[KSong: {}\n"
               "\e[KAuthor: {}\n"
               "\e[KGame: {}\n"
               "\e[KSystem: {}\n"
               "\e[KComment: {}\n"
               "\e[KDumper: {}\n"
               "\n\n",
               m.info[Song], m.info[Author], m.info[Game],
               m.info[System], m.info[Comment], m.info[Dumper]);
    std::fflush(stdout);
}

void update_status(const Status &status) {
    auto [width, _] = get_terminal_size();
    fmt::print("\r\e[2A"
               "\e[K{}{} Tempo: {:.03}x Volume: {} [{}] Autoplay [{}] Repeat file [{}] Repeat track\n"
               "\e[K[{}]\n",
               status.paused ? "(Paused) " : "",
               format_position(status.position, status.length),
               int_to_tempo(status.tempo),
               status.volume,
               status.autoplay     ? "X" : " ",
               status.repeat_file  ? "X" : " ",
               status.repeat_track ? "X" : " ",
               make_slider(status.position, status.length, width - 2));
    std::fflush(stdout);
}

int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fmt::print("error: cannot initialize SDL: {}\n", SDL_GetError());
        return 1;
    }

    auto errors = config.load();
    if (errors.size() > 0) {
        std::string errors_str;
        for (auto e : errors)
            errors_str += e.message() + "\n";
        fmt::print("Errors were found while parsing the configuration file.", errors_str);
    }

    gmplayer::Player player;
    player.mpris_server().set_identity("gmplayer");
    player.mpris_server().set_supported_uri_schemes({"file"});
    player.mpris_server().set_supported_mime_types({"application/x-pkcs7-certificates", "application/octet-stream", "text/plain"});
    player.mpris_server().on_quit([] { });
    player.mpris_server().on_open_uri([&] (std::string_view uri) {
        if (uri.size() < 7 || uri.substr(0, 7) != "file://") {
            fmt::print(stderr, "error: only local files are supported\n");
            return;
        }
        player.add_file(fs::path(uri.substr(7)));
    });

    bool running = true;
    Status status = {
        .paused       = true,
        .tempo        = tempo_to_int(config.get<float>("tempo")),
        .volume       = config.get<int>("volume"),
        .autoplay     = config.get<bool>("autoplay"),
        .repeat_file  = config.get<bool>("repeat_file"),
        .repeat_track = config.get<bool>("repeat_track"),
        .position     = 0,
        .length       = 0,
    };
    Terminal term;

    config.when_set("volume", [&] (const conf::Value &value) {
        status.volume = value.as<int>();
        update_status(status);
    });

    config.when_set("tempo", [&] (const conf::Value &value) {
        status.tempo = tempo_to_int(value.as<float>());
        update_status(status);
    });

    config.when_set("autoplay", [&] (const conf::Value &value) {
        status.autoplay = value.as<bool>();
        update_status(status);
    });

    config.when_set("repeat_file", [&] (const conf::Value &value) {
        status.repeat_file = value.as<bool>();
        update_status(status);
    });

    config.when_set("repeat_track", [&] (const conf::Value &value) {
        status.repeat_track = value.as<bool>();
        update_status(status);
    });

    player.on_error([&] (gmplayer::Error error) {
        fmt::print("got error\n");
        running = false;
    });

    player.on_playlist_changed([&] (gmplayer::Playlist::Type type) {
        if (type == gmplayer::Playlist::Type::File) {
            player.load_pair(0, 0);
            player.start_or_resume();
        }
    });

    player.on_position_changed([&] (int pos) {
        status.position = pos;
        update_status(status);
    });

    player.on_file_changed([&] (int id) {
        print_file_info(player.file_info(id));
    });

    player.on_track_changed([&] (int id, const gmplayer::Metadata &metadata) {
        status.length = metadata.length;
        print_metadata(metadata);
        update_status(status);
    });

    player.on_track_ended([&] {
        // fmt::print("Track ended.\n");
    });

    player.on_paused([&] (void) {
        status.paused = true;
        update_status(status);
    });

    player.on_played([&] (void) {
        status.paused = false;
        update_status(status);
    });

    player.on_first_file_load([&] {
        fmt::print("\e[A\n\n\n\n\n\n\n\n\n");
        std::fflush(stdout);
    });

    fmt::print("Listening...\n");
    auto files = args_to_paths(argc, argv);
    if (auto file_errors = player.add_files(files); !file_errors.empty())
        for (auto &e : file_errors)
            fmt::print("error: {}: {}\n", e.first.string(), e.second.message());

    while (running) {
        for (SDL_Event ev; SDL_PollEvent(&ev); ) {
            switch (ev.type) {
            case SDL_QUIT:
                running = false;
                break;
            }
        }

        if (auto [has_input, c] = term.get_input(); has_input) {
            switch (c) {
            case 'h':
                player.seek_relative(-1_sec);
                break;
            case 'l':
                player.seek_relative(1_sec);
                break;
            case 'j':
                player.next();
                break;
            case 'k':
                player.prev();
                break;
            case 'a':
                config.set<bool>("autoplay", !config.get<bool>("autoplay"));
                break;
            case 's':
                config.set<bool>("repeat_file", !config.get<bool>("repeat_file"));
                break;
            case 'd':
                config.set<bool>("repeat_track", !config.get<bool>("repeat_track"));
                break;
            // case '7': {
            //     auto tempo = status.tempo - 1;
            //     if (tempo >= 0)
            //         config.set<float>("tempo", int_to_tempo(tempo));
            //     break;
            // }
            // case '8':{
            //     auto tempo = status.tempo + 1;
            //     if (tempo <= 100)
            //         config.set<float>("tempo", int_to_tempo(tempo));
            //     break;
            // }
            case '9': {
                auto volume = config.get<int>("volume") - 1;
                if (volume >= 0)
                    config.set<int>("volume", volume);
                break;
            }
            case '0': {
                auto volume = config.get<int>("volume") + 1;
                if (volume <= MAX_VOLUME_VALUE)
                    config.set<int>("volume", volume);
                break;
            }
            case ' ':
                player.play_pause();
                break;
            case 'q':
                running = false;
                break;
            }
        }

        SDL_Delay(16);
    }

    config.save();
    SDL_Quit();
    return 0;
}
