#include <thread>
#include <QApplication>
#include <QDialog>
#include <QPushButton>
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
    MainWindow mw;

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
