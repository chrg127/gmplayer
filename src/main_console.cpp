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
    int tempo;
    int volume;
    bool autoplay;
    bool repeat_file;
    bool repeat_track;
    int position;
    int length;
};

const int FILE_INFO_HEIGHT = 10;
const int TRACK_INFO_HEIGHT = 8;
const int STATUS_HEIGHT = 2;

std::string make_space(int newlines) { return std::string(newlines, '\n'); }

void print_file_info(const io::MappedFile &f, int num_tracks)
{
    fmt::print("\r\e[{}A"
               "\e[KFile: {}\n"
               "\e[KNumber of tracks: {}\n"
               "{}",
               FILE_INFO_HEIGHT,
               f.name(), num_tracks,
               make_space(TRACK_INFO_HEIGHT));
    std::fflush(stdout);
}

void print_metadata(const gmplayer::Metadata &m)
{
    using enum gmplayer::Metadata::Field;
    fmt::print("\r\e[{}A"
               "\e[KSong: {}\n"
               "\e[KAuthor: {}\n"
               "\e[KGame: {}\n"
               "\e[KSystem: {}\n"
               "\e[KComment: {}\n"
               "\e[KDumper: {}\n"
               "{}",
               TRACK_INFO_HEIGHT,
               m.info[Song], m.info[Author], m.info[Game],
               m.info[System], m.info[Comment], m.info[Dumper],
               make_space(STATUS_HEIGHT));
    std::fflush(stdout);
}

int percent_of(int x, int max) { return x * 100 / max; }

void update_status(const Status &status) {
    auto [width, _] = get_terminal_size();
    fmt::print("\r\e[{}A"
               "\e[K{}{} Tempo: {:.03}x Volume: {}\% [{}] Autoplay [{}] Repeat file [{}] Repeat track\n"
               "\e[K[{}]\n",
               STATUS_HEIGHT,
               status.paused ? "(Paused) " : "",
               format_position(status.position, status.length),
               gmplayer::int_to_tempo(status.tempo),
               percent_of(status.volume, MAX_VOLUME_VALUE),
               status.autoplay     ? "X" : " ",
               status.repeat_file  ? "X" : " ",
               status.repeat_track ? "X" : " ",
               make_slider(status.position, status.length, width - 2));
    std::fflush(stdout);
}

auto get_files(int argc, char *argv[])
{
    if (auto p = fs::path(argv[1]); gmplayer::is_playlist(p)) {
        auto contents = gmplayer::open_playlist(p);
        if (!contents) {
            fmt::print("{}\n", contents.error().message());
            return std::vector<fs::path>{};
        }
        return contents.value();
    }
    return args_to_paths(argc, argv);
}

int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fmt::print("error: cannot initialize SDL: {}\n", SDL_GetError());
        return 1;
    }

    auto errors = config.load();
    if (errors.size() > 0) {
        fmt::print("Errors were found while parsing the configuration file:\n");
        for (auto e : errors)
            fmt::print("{}\n", e.message());
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
        .tempo        = config.get<int>("tempo"),
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
        if (player.is_playing())
            update_status(status);
    });

    config.when_set("tempo", [&] (const conf::Value &value) {
        status.tempo = value.as<int>();
        if (player.is_playing())
            update_status(status);
    });

    config.when_set("autoplay", [&] (const conf::Value &value) {
        status.autoplay = value.as<bool>();
        if (player.is_playing())
            update_status(status);
    });

    config.when_set("repeat_file", [&] (const conf::Value &value) {
        status.repeat_file = value.as<bool>();
        if (player.is_playing())
            update_status(status);
    });

    config.when_set("repeat_track", [&] (const conf::Value &value) {
        status.repeat_track = value.as<bool>();
        if (player.is_playing())
            update_status(status);
    });

    player.on_error([&] (gmplayer::Error error) {
        fmt::print("got error: {}\n", int(error.type()));
        running = false;
    });

    player.on_playlist_changed([&] (gmplayer::Playlist::Type type) {
        if (type == gmplayer::Playlist::Type::File && player.file_count() > 0) {
            player.load_pair(0, 0);
            player.start_or_resume();
        }
    });

    player.on_shuffled([&] (gmplayer::Playlist::Type type) {
        if (player.is_playing())
            if (type == gmplayer::Playlist::Track)
                player.load_track(0);
            else
                player.load_pair(0, 0);
    });

    player.on_position_changed([&] (int pos) {
        status.position = pos;
        if (player.is_playing())
            update_status(status);
    });

    player.on_file_changed([&] (int id) {
        print_file_info(player.file_info(id), player.track_count());
    });

    player.on_track_changed([&] (int id, const gmplayer::Metadata &metadata) {
        status.length = metadata.length;
        print_metadata(metadata);
        update_status(status);
        player.start_or_resume();
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
        fmt::print("\e[A\n\n\n\n\n\n\n\n\n\n");
        std::fflush(stdout);
    });

    fmt::print("Listening...\n");
    if (argc > 1) {
        auto files = get_files(argc, argv);
        if (auto file_errors = player.add_files(files); !file_errors.empty())
            for (auto &e : file_errors)
                fmt::print("error: {}: {}\n", e.first.string(), e.second.message());
    }

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
            case 'r':
                config.set<bool>("repeat_file", !config.get<bool>("repeat_file"));
                break;
            case 't':
                config.set<bool>("repeat_track", !config.get<bool>("repeat_track"));
                break;
            case 's':
                player.shuffle(gmplayer::Playlist::File);
                break;
            case 'd':
                player.shuffle(gmplayer::Playlist::Track);
                break;
            case '7': {
                if (auto tempo = config.get<int>("tempo") - 1; tempo >= 0)
                    config.set<int>("tempo", tempo);
                break;
            }
            case '8':{
                if (auto tempo = config.get<int>("tempo") + 1; tempo <= MAX_TEMPO_VALUE)
                    config.set<int>("tempo", tempo);
                break;
            }
            case '9': {
                if (auto volume = config.get<int>("volume") - 1; volume >= 0)
                    config.set<int>("volume", volume);
                break;
            }
            case '0': {
                if (auto volume = config.get<int>("volume") + 1; volume <= MAX_VOLUME_VALUE)
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
