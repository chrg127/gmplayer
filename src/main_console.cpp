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

struct Status {
    bool paused;
    float tempo;
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
               "File: {}\n"
               "\n\n\n\n\n\n\n\n",
               f.name());
    std::fflush(stdout);
}

void print_metadata(const gmplayer::Metadata &m)
{
    using enum gmplayer::Metadata::Field;
    fmt::print("\r\e[8A"
               "Song: {}\n"
               "Author: {}\n"
               "Game: {}\n"
               "System: {}\n"
               "Comment: {}\n"
               "Dumper: {}\n"
               "\n\n",
               m.info[Song], m.info[Author], m.info[Game],
               m.info[System], m.info[Comment], m.info[Dumper]);
    std::fflush(stdout);
}

void update_status(const Status &status) {
    auto [width, _] = get_terminal_size();
    fmt::print("\r\e[2A"
               "{}{} Tempo: {} Volume: {} [{}] Autoplay [{}] Repeat file [{}] Repeat track\n"
               "[{}]\n",
               status.paused ? "(Paused) " : "",
               format_position(status.position, status.length),
               status.tempo, status.volume,
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

    bool running = true;
    Status status = {
        .paused = true,
        .tempo = config.get<float>("tempo"),
        .volume = config.get<int>("volume"),
        .autoplay = config.get<bool>("autoplay"),
        .repeat_file = config.get<bool>("repeat_file"),
        .repeat_track = config.get<bool>("repeat_file"),
        .position = 0,
        .length = 0,
    };
    Terminal term;

    player.on_error([&] (gmplayer::Error error) {
        fmt::print("got error\n");
        running = false;
    });

    player.on_playlist_changed([&] (gmplayer::Playlist::Type type) {
        if (type == gmplayer::Playlist::File && player.file_count() != 0) {
            player.load_pair(0, 0);
            player.start_or_resume();
        } else {
            // fmt::print("Listening...\n");
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

    player.on_volume_changed([&] (int value) {
        status.volume = value;
        update_status(status);
    });

    fmt::print("\n\n\n\n\n\n\n\n\n");
    std::fflush(stdout);
    auto files = args_to_paths(argc, argv);
    if (auto file_errors = player.add_files(files);!file_errors.empty())
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
            case '9':
                config.set<int>("volume", config.get<int>("volume") - 2);
                break;
            case '0':
                config.set<int>("volume", config.get<int>("volume") + 2);
                break;
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
