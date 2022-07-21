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

    bool was_paused = false;
    Player *player;
    QSlider *duration_slider, *volume;
    QLabel *duration_label;
    QString last_dir = ".";
    PlayButton *play_btn;
    QToolButton *stop, *prev_track, *next_track, *volume_btn;
    QListWidget *playlist;
    QCheckBox *autoplay, *repeat, *shuffle;

    QMenu *create_menu(const char *name, auto&&... actions);
    void open_file(QString filename);
    void set_duration_label(int ms, int max);
    void edit_settings();
    void set_enabled(bool val);
    void closeEvent(QCloseEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
public:
    explicit MainWindow(QWidget *parent = nullptr);
};

class SettingsWindow : public QDialog {
    Q_OBJECT
    Player *player;
public:
    explicit SettingsWindow(Player *player, QWidget *parent = nullptr);
};
