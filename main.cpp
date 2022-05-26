#include <thread>
#include <QApplication>
#include <QDialog>
#include <QPushButton>
#include <SDL.h>
#include <fmt/core.h>
#include <gme/gme.h>

bool running = true;

bool handle_sdl_events()
{
    for (SDL_Event ev; SDL_PollEvent(&ev); ) {
        switch (ev.type) {
        case SDL_QUIT:
            running = false;
            return true;
        }
    }
    return false;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QDialog d;
    QPushButton b("hello", &d);
    SDL_Init(SDL_INIT_AUDIO);
    std::thread th([&]() {
        while (running) {
            if (handle_sdl_events()) {
                d.close();
            }
        }
    });
    d.show();
    Music_Emu *emu;
    gme_open_file("ynbarracks.spc", &emu, 44100);
    a.exec();
    running = false;
    th.join();
    SDL_Quit();
    return 0;
}

