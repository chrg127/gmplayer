#pragma once

#include <map>
#include <span>
#include <string>
#include <QObject>
#include <QWidget>
#include <QMainWindow>
#include <QToolButton>
#include <QDialog>
#include <QPushButton>
#include "player.hpp"
#include "keyrecorder.hpp"

class QLabel;
class QSlider;
class QListWidget;
class QCheckBox;
class QSpinBox;
class QGroupBox;
class QShortcut;
class QComboBox;

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

struct Shortcut {
    QShortcut *shortcut;
    QString name;
    QString display_name;
};

class Playlist : public QWidget {
    Q_OBJECT
    QListWidget *playlist;
    QCheckBox *autoplay, *repeat, *shuffle;
public:
    Playlist(const char *name, QWidget *parent = nullptr);
    void update(std::span<std::string> names, int start_track);
    void set_enabled(bool enabled);
    void set_checked(bool autoplay, bool repeat, bool shuffle);
    void set_current(int index);
    // void add(const QString &name);
    // void clear();
signals:
    void item_activated(int index);
    void autoplay_clicked(bool state);
    void repeat_clicked(bool state);
    void shuffle_clicked(bool state);
};

class MainWindow : public QMainWindow {
    Q_OBJECT

    bool was_paused = false;
    Player *player;
    QSlider *duration_slider, *volume;
    QLabel *duration_label;
    QString last_dir = ".";
    PlayButton *play_btn;
    QToolButton *stop_btn, *prev_track, *next_track, *volume_btn;
    QComboBox *tempo;
    Playlist *playlist;
    Playlist *file_playlist;
    std::map<QString, Shortcut> shortcuts;

    QMenu *create_menu(const char *name, auto&&... actions);
    void load_shortcuts();
    void open_file(QString filename);
    void start_or_resume();
    void pause();
    void stop();
    void edit_settings();
    void edit_shortcuts();
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

class ShortcutsWindow : public QDialog {
    Q_OBJECT
public:
    explicit ShortcutsWindow(const std::map<QString, Shortcut> &shortcuts);
};

// a button that when clicked records a key sequence.
struct RecorderButton : public QPushButton {
    Q_OBJECT
    KeyRecorder *recorder;
public:
    RecorderButton(const QString &text, int key_count = 1, QWidget *parent = nullptr);
signals:
    void started();
    void got_key_sequence(const QKeySequence &keySequence);
};
