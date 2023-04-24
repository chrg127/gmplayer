#include <thread>
#include <QApplication>
#include <QDebug>
#include <QStandardPaths>
#include <QSettings>
#include <SDL.h>
#include "gui.hpp"
#include "player.hpp"

using namespace gmplayer::literals;

gmplayer::PlayerOptions load_player_options()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "gmplayer", "gmplayer");
    settings.beginGroup("player");
    gmplayer::PlayerOptions options = {
        .fade_out           = settings.value("fade_out",                               0).toInt(),
        .autoplay           = settings.value("autoplay",                           false).toBool(),
        .track_repeat       = settings.value("track_repeat",                       false).toBool(),
        .file_repeat        = settings.value("file_repeat",                        false).toBool(),
        .default_duration   = settings.value("default_duration",              int(3_min)).toInt(),
        .tempo              = settings.value("tempo",                                1.0).toDouble(),
        .volume             = settings.value("volume",  gmplayer::get_max_volume_value()).toInt()
    };
    settings.endGroup();
    return options;
}

void save_player_options(const gmplayer::PlayerOptions &options)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "gmplayer", "gmplayer");
    settings.beginGroup("player");
    settings.setValue("fade_out",          options.fade_out);
    settings.setValue("autoplay",          options.autoplay);
    settings.setValue("track_repeat",      options.track_repeat);
    settings.setValue("file_repeat",       options.file_repeat);
    settings.setValue("default_duration",  options.default_duration);
    settings.setValue("tempo",             options.tempo);
    settings.setValue("volume",            options.volume);
    settings.endGroup();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    SDL_Init(SDL_INIT_AUDIO);

    gmplayer::Player player{load_player_options()};

    player.mpris_server().set_desktop_entry(QStandardPaths::locate(QStandardPaths::ApplicationsLocation, "gmplayer.desktop").toStdString());
    player.mpris_server().set_identity("gmplayer");
    player.mpris_server().set_supported_uri_schemes({"file"});
    player.mpris_server().set_supported_mime_types({"application/x-pkcs7-certificates", "application/octet-stream", "text/plain"});
    player.mpris_server().on_quit([] { QApplication::quit(); });

    gui::MainWindow mw{&player};

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
            mw.open_files(filenames, gui::OpenFilesFlags::AddToRecent | gui::OpenFilesFlags::ClearAndPlay);
        }
    }

    a.exec();

    sdl_running = false;
    sdl_thread.join();
    save_player_options(player.options());
    SDL_Quit();
    return 0;
}
