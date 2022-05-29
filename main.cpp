#include <thread>
#include <QApplication>
#include <QDialog>
#include <QPushButton>
#include <SDL.h>
#include <fmt/core.h>
#include <gme/gme.h>
#include "mainwindow.hpp"

bool sdl_running = true;

void handle_sdl_events()
{
    for (SDL_Event ev; SDL_PollEvent(&ev); ) {
        switch (ev.type) {
        case SDL_QUIT:
            sdl_running = false;
        }
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow mw;
    SDL_Init(SDL_INIT_AUDIO);
    std::thread th([&]() {
        while (sdl_running) {
            handle_sdl_events();
            SDL_Delay(16);
        }
        mw.close();
    });
    mw.show();
    a.exec();
    sdl_running = false;
    th.join();
    SDL_Quit();
    return 0;
}
