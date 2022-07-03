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
    QLabel *title, *game, *system, *author, *comment, *duration_label;
    QSlider *duration_slider, *volume;
    QString last_dir = ".";
    PlayButton *play_btn;
    QToolButton *stop, *prev_track, *next_track, *volume_btn;
    QListWidget *playlist;

    QMenu *create_menu(const char *name, auto&&... actions);
    void edit_settings();
    void open_file();
    void set_duration_label(int ms, int max);
    void set_enabled(bool val);
public:
    explicit MainWindow(QWidget *parent = nullptr);
};

class SettingsWindow : public QDialog {
    Q_OBJECT

    QCheckBox *fade, *autoplay;
    QSpinBox *fade_secs;
    PlayerOptions selected_options;

public:
    explicit SettingsWindow(const PlayerOptions &options, QWidget *parent = nullptr);
    PlayerOptions get() const { return selected_options; }
};
