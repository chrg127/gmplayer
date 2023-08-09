#include <thread>
#include <QApplication>
#include <QDebug>
#include <QStandardPaths>
#include <QSettings>
#include <SDL.h>
#include <fmt/core.h>
#include "gui.hpp"
#include "player.hpp"
#include "mpris_server.hpp"
#include "qtutils.hpp"
#include "config.hpp"

Config config;

#define USE_QT 1

#if USE_QT

int main(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_AUDIO);
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

    auto mw = gui::MainWindow(&player);

    bool sdl_running = true;
    std::thread sdl_thread([&]() {
        while (sdl_running) {
            for (SDL_Event ev; SDL_PollEvent(&ev); ) {
                switch (ev.type) {
                case SDL_QUIT:
                    mw.close();
                    sdl_running = false;
                }
            }
            SDL_Delay(16);
        }
    });

    mw.show();
    if (argc > 1) {
        if (argc == 2 && gmplayer::is_playlist(std::filesystem::path(argv[1]))) {
            mw.open_playlist(std::filesystem::path(argv[1]));
        } else {
            std::vector<std::filesystem::path> filenames;
            for (int i = 1; i < argc; i++)
                filenames.push_back(std::filesystem::path(argv[i]));
            mw.open_files(filenames, { gui::OpenFilesFlags::AddToRecent, gui::OpenFilesFlags::ClearAndPlay });
        }
    }

    a.exec();

    sdl_running = false;
    sdl_thread.join();
    config.save();
    SDL_Quit();
    return 0;
}

#else

// a second main to test features directly without involving qt stuff.
int main()
{
    SDL_Init(SDL_INIT_AUDIO);

    auto [options, errors] = conf::parse_or_create("gmplayer", defaults);
    std::string errors_str;
    for (auto e : errors)
        errors_str += e.message() + "\n";
    if (errors.size() > 0)
        msgbox("Errors were found while parsing the configuration file.", QString::fromStdString(errors_str));

    gmplayer::Player player;
    player.set_options({
        .fade_out         = options["fade"]            .as<int>(),
        .autoplay         = options["autoplay"]        .as<bool>(),
        .track_repeat     = options["repeat_track"]    .as<bool>(),
        .file_repeat      = options["repeat_file"]     .as<bool>(),
        .default_duration = options["default_duration"].as<int>(),
        .tempo            = options["tempo"]           .as<float>(),
        .volume           = options["volume"]          .as<int>()
    });

    player.mpris_server().set_desktop_entry(QStandardPaths::locate(QStandardPaths::ApplicationsLocation, "gmplayer.desktop").toStdString());
    player.mpris_server().set_identity("gmplayer");
    player.mpris_server().set_supported_uri_schemes({"file"});
    player.mpris_server().set_supported_mime_types({"application/x-pkcs7-certificates", "application/octet-stream", "text/plain"});
    player.mpris_server().on_quit([] { QApplication::quit(); });

    player.on_error([&] (auto err) { printf("%s\n", err.code.message().c_str()); });

    auto err = player.add_file(std::filesystem::path{"test_files/smb3.nsf"});
    if (err) {
        printf("%s\n", err.message().c_str());
        return 1;
    }

    player.load_file(0);
    player.load_track(0);
    // player.seek(1_sec);
    player.start_or_resume();

    bool sdl_running = true;
    while (sdl_running) {
        for (SDL_Event ev; SDL_PollEvent(&ev); ) {
            switch (ev.type) {
            case SDL_QUIT:
                sdl_running = false;
            }
        }
        SDL_Delay(16);
    }

    SDL_Quit();
    return 0;
}

#endif
