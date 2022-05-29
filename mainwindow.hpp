#pragma once

#include <QObject>
#include <QWidget>
#include <QMainWindow>

class QLabel;

class MediaControls : public QWidget {
    Q_OBJECT
public:
    explicit MediaControls(QWidget *parent = nullptr);
signals:
    void play();
    void pause();
    void stop();
    void next();
    void prev();
    void change_volume(int volume);
};

class MainWindow : public QMainWindow {
    Q_OBJECT
    QLabel *title, *game, *system, *author, *comment;
    MediaControls *controls;
public:
    explicit MainWindow(QWidget *parent = nullptr);
    QMenu *create_menu(const char *name, auto... actions);
    void edit_settings();
    void open_file();
};
