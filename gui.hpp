#pragma once

#include <QObject>
#include <QWidget>
#include <QMainWindow>
#include <QToolButton>
#include <QDialog>
#include "player.hpp"

class QLabel;
class QSlider;
class QListWidget;
class QCheckBox;
class QSpinBox;
class QGroupBox;

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

class MainWindow : public QMainWindow {
    Q_OBJECT

    Player *player;
    QSlider *duration_slider, *volume;
    QString last_dir = ".";
    PlayButton *play_btn;
    QToolButton *stop, *prev_track, *next_track, *volume_btn;
    QListWidget *playlist;

    QGroupBox *settings_box;
    QGroupBox *playlist_settings_box;

    QMenu *create_menu(const char *name, auto&&... actions);
    void open_file();
    void set_enabled(bool val);
    void closeEvent(QCloseEvent *event);
public:
    explicit MainWindow(QWidget *parent = nullptr);
};
