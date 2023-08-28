#include <thread>
#include <SDL.h>
#include <fmt/core.h>
#include "player.hpp"
#include "mpris_server.hpp"
#include "config.hpp"
#include "types.hpp"
#include "io.hpp"

#if defined(INTERFACE_QT)
    #include <QApplication>
    #include <QDebug>
    #include <QStandardPaths>
    #include <QSettings>
    #include "gui.hpp"
    #include "qtutils.hpp"
#endif

namespace fs = std::filesystem;

Config config;

std::vector<fs::path> args_to_paths(int argc, char *argv[])
{
    std::vector<fs::path> files;
    for (int i = 1; i < argc; i++)
        files.push_back(fs::path{argv[i]});
    return files;
}

#if defined(INTERFACE_QT)

int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fmt::print("error: cannot initialize SDL: {}\n", SDL_GetError());
        return 1;
    }

    QApplication a(argc, argv);

    auto errors = config.load();
    if (errors.size() > 0) {
        std::string errors_str;
        for (auto e : errors)
            errors_str += e.message() + "\n";
        msgbox("Errors were found while parsing the configuration file.", QString::fromStdString(errors_str));
    }

    gmplayer::Player player;
    player.mpris_server().set_desktop_entry(QStandardPaths::locate(QStandardPaths::ApplicationsLocation, "gmplayer.desktop").toStdString());
    player.mpris_server().set_identity("gmplayer");
    player.mpris_server().set_supported_uri_schemes({"file"});
    player.mpris_server().set_supported_mime_types({"application/x-pkcs7-certificates", "application/octet-stream", "text/plain"});
    player.mpris_server().on_quit([] { QApplication::quit(); });

    auto main_window = gui::MainWindow(&player);

    bool sdl_running = true;
    std::thread sdl_thread([&]() {
        while (sdl_running) {
            for (SDL_Event ev; SDL_PollEvent(&ev); ) {
                switch (ev.type) {
                case SDL_QUIT:
                    main_window.close();
                    sdl_running = false;
                }
            }
            SDL_Delay(16);
        }
    });

    main_window.show();
    if (argc > 1) {
        if (argc == 2 && gmplayer::is_playlist(fs::path(argv[1]))) {
            main_window.open_playlist(fs::path(argv[1]));
        } else {
            auto files = args_to_paths(argc, argv);
            main_window.open_files(files, { gui::OpenFilesFlags::AddToRecent,
                                            gui::OpenFilesFlags::ClearAndPlay });
        }
    }

    a.exec();

    sdl_running = false;
    sdl_thread.join();
    config.save();
    SDL_Quit();
    return 0;
}

#elif defined(INTERFACE_CONSOLE)

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

    player.on_error([&] (gmplayer::Error error) {
        fmt::print("got error\n");
    });

    player.on_position_changed([&] (int pos) {
        fmt::print("pos = {}\r", pos);
        std::fflush(stdout);
    });

    player.on_playlist_changed([&] (gmplayer::Playlist::Type type) {
        if (type == gmplayer::Playlist::File && player.file_count() != 0) {
            player.load_pair(0, 0);
            player.start_or_resume();
        } else {
            fmt::print("Listening...\n");
        }
    });

    player.on_file_changed([&] (int id) {
        fmt::print("Playing file {}\n", player.file_info(id).path().string());
    });

    player.on_track_changed([&] (int id, const gmplayer::Metadata &metadata) {
        //fmt::print("Playing track {}\n", metadata.info[gmplayer::Metadata::Song]);
    });

    player.on_track_ended([&] {
        fmt::print("Track ended.\n");
    });

    player.on_paused([&] (void) {
        fmt::print("[Paused]\n");
    });

    player.on_played([&] (void) {
        fmt::print("[Resumed]\n");
    });

    player.on_seeked([&] (int pos) {
        fmt::print("Seeked to {}\n", pos);
    });

    player.on_volume_changed([&] (int value) {
        fmt::print("Volume: {}\n", value);
    });

    player.on_tempo_changed([&] (double) { });
    player.on_fade_changed([&] (int) { });
    player.on_repeat_changed([&] (bool, bool) { });
    player.on_shuffled([&] (gmplayer::Playlist::Type) { });
    player.on_cleared([&] (void) { });
    player.on_files_removed([&] (std::span<int>) { });
    player.on_samples_played([&] (std::span<i16>, std::span<f32>) { });
    player.on_channel_volume_changed([&] (int, int) { });

    auto files = args_to_paths(argc, argv);
    if (auto file_errors = player.add_files(files);!file_errors.empty())
        for (auto &e : file_errors)
            fmt::print("error: {}: {}\n", e.first.string(), e.second.message());

    for (bool running = true; running; ) {
        for (SDL_Event ev; SDL_PollEvent(&ev); ) {
            switch (ev.type) {
            case SDL_QUIT:
                running = false;
            }
        }
        SDL_Delay(16);
    }

    config.save();
    SDL_Quit();
    return 0;
}

#else

#error "You must specify an interface. Possible interfaces are 'qt' and 'console'."

#endif
