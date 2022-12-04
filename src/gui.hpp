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
#include <QStringList>
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
class QMenu;

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

class RecentList : public QObject {
    Q_OBJECT
    QStringList names;
    QMenu *menu;
public:
    RecentList(QMenu *menu, const QStringList &list);
    QStringList &filenames() { return names; }
    void add(const QString &name);
signals:
    void clicked(const QString &filename);
};

class MainWindow : public QMainWindow {
    Q_OBJECT

    Player *player;
    QSlider *duration_slider, *volume;
    QLabel *duration_label;
    QString last_file = ".";
    QString last_playlist = ".";
    PlayButton *play_btn;
    QToolButton *stop_btn, *prev_track, *next_track, *volume_btn;
    QComboBox *tempo;
    QListWidget *track_playlist, *file_playlist;
    QCheckBox *autoplay, *repeat_track, *repeat_file, *shuffle_tracks, *shuffle_files;
    std::map<QString, Shortcut> shortcuts;
    bool was_paused = false;
    RecentList *recent_files, *recent_playlists;

    void open_playlist(const QString &filename);
    void open_single_file(QString filename);
    void finish_opening();
    void load_shortcuts();
    void start_or_resume();
    void pause();
    void stop();
    void edit_settings();
    void edit_shortcuts();
    // listeners to events sent by qt
    void closeEvent(QCloseEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
public:
    explicit MainWindow(QWidget *parent = nullptr);
};
