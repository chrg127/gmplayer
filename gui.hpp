#pragma once

#include <QObject>
#include <QWidget>
#include <QMainWindow>
#include <QToolButton>

class QLabel;

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
    QLabel *title, *game, *system, *author, *comment;
    QLabel *duration_label;
    QString last_dir = ".";
    PlayButton *play_btn;

    QMenu *create_menu(const char *name, auto&&... actions);
    void edit_settings();
    void open_file();
public:
    explicit MainWindow(QWidget *parent = nullptr);
};
