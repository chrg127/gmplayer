#include <thread>
#include <SDL.h>
#include <fmt/core.h>
#include <QApplication>
#include <QDebug>
#include <QStandardPaths>
#include <QSettings>
#include <QUrl>
#include "player.hpp"
#include "mpris_server.hpp"
#include "config.hpp"
#include "audio.hpp"
#include "io.hpp"
#include "gui.hpp"
#include "qtutils.hpp"

namespace fs = std::filesystem;

Config config;

std::vector<fs::path> args_to_paths(int argc, char *argv[])
{
    std::vector<fs::path> files;
    for (int i = 1; i < argc; i++)
        files.push_back(fs::path{argv[i]});
    return files;
}

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

    player.mpris_server().on_open_uri([&] (std::string_view uri) {
        main_window.open_url(QUrl(QString::fromStdString(std::string(uri))));
    });


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
