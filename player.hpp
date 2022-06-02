#pragma once

#include <QObject>
#include <mutex>
#include <SDL_audio.h> // SDL_AudioDeviceID

class QString;
class Music_Emu;
class gme_info_t;

struct TrackInfo {
    gme_info_t *metadata;
    int length;
};

class Player : public QObject {
    Q_OBJECT

    // emulator and audio device objects.
    // a mutex is needed because audio plays in another thread.
    Music_Emu *emu           = nullptr;
    SDL_AudioDeviceID dev_id = 0;

    // file information:
    int track_count = 0;

    // current track information:
    TrackInfo track = {
        .metadata = nullptr,
        .length = 0
    };
    int cur_track = 0;

    void load_track(int num);
    void use_file(const char *filename);

public:
    Player(QObject *parent = nullptr);
    ~Player();

    void use_file(const QString &filename);
    void start_or_resume();
    void pause();
    void stop();
    void next();
    void prev();
    TrackInfo track_info() const { return track; }
};
