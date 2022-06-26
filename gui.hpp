#pragma once

#include <QObject>
#include <QWidget>
#include <QMainWindow>
#include <QToolButton>
#include "player.hpp"

class QLabel;
class QSlider;

class PlayButton : public QToolButton {
    Q_OBJECT
public:
    enum class State { Play, Pause } state = State::Pause;
    PlayButton(QWidget *parent = nullptr);
    void set_state(State state);
signals:
    void play();
    void pause();
};

class VolumeButton : public QToolButton {
    Q_OBJECT
public:
    int last_volume;
    QSlider *slider;
    VolumeButton(QSlider *volume_slider, QWidget *parent);
};

class MainWindow : public QMainWindow {
    Q_OBJECT

    Player *player;
    QLabel *title, *game, *system, *author, *comment, *duration_label;
    QSlider *duration_slider, *volume;
    QString last_dir = ".";
    PlayButton *play_btn;
    VolumeButton *volume_btn;
    QToolButton *stop, *prev_track, *next_track;

    QMenu *create_menu(const char *name, auto&&... actions);
    void edit_settings();
    void open_file();
    void set_duration_label(int ms, int max);
    void set_enabled(bool val);
public:
    explicit MainWindow(QWidget *parent = nullptr);
};
