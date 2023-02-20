#include <thread>
#include <QApplication>
#include <QDialog>
#include <QPushButton>
#include <QDebug>
#include <QStandardPaths>
#include <SDL.h>
#include <fmt/core.h>
#include <gme/gme.h>
#include "gui.hpp"
#include "player.hpp"

bool sdl_running = true;
bool got_sigint = false;

void handle_sdl_events()
{
    for (SDL_Event ev; SDL_PollEvent(&ev); ) {
        switch (ev.type) {
        case SDL_QUIT:
            got_sigint = true;
            sdl_running = false;
        }
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    SDL_Init(SDL_INIT_AUDIO);

    Player player;

    player.mpris_server().set_desktop_entry(QStandardPaths::locate(QStandardPaths::ApplicationsLocation, "gmplayer.desktop").toStdString());
    player.mpris_server().set_identity("gmplayer");
    player.mpris_server().set_supported_uri_schemes({"file"});
    player.mpris_server().set_supported_mime_types({"application/x-pkcs7-certificates", "application/octet-stream", "text/plain"});
    player.mpris_server().on_quit(    []                        { QApplication::quit(); });
    player.mpris_server().on_open_uri([] (std::string_view url) { qDebug() << "not opening uri, sorry"; });

    MainWindow mw{&player};

    std::thread th([&]() {
        while (sdl_running) {
            handle_sdl_events();
            SDL_Delay(16);
        }
        if (got_sigint)
            mw.close();
    });

    mw.show();
    a.exec();

    sdl_running = false;
    th.join();
    SDL_Quit();
    return 0;
}
