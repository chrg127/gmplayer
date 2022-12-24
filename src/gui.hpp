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
class MprisPlayer;

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

class SettingsWindow : public QDialog {
    Q_OBJECT
    Player *player;
public:
    explicit SettingsWindow(Player *player, QWidget *parent = nullptr);
};

struct Shortcut {
    QShortcut *shortcut;
    QString name;
    QString display_name;
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

class PlaylistWidget : public QWidget {
    Q_OBJECT

    QListWidget *list;
    QPushButton *shuffle;
public:
    explicit PlaylistWidget(const QString &name, QWidget *parent = nullptr);
    void update_names(int n, std::vector<std::string> &&names);
    void setup_context_menu(std::function<void(const QPoint &)> fn);
    void set_current(int n);
    int current();
    QPoint map_point(const QPoint &p);
    QListWidget *playlist()    const { return list; }
    QPushButton *shuffle_btn() const { return shuffle; }
signals:
    void item_activated();
    void shuffle_selected();
};

class MainWindow : public QMainWindow {
    Q_OBJECT

    Player *player;
    QString last_file = ".";
    QString last_playlist = ".";
    std::map<QString, Shortcut> shortcuts;
    bool was_paused = false;
    // widgets
    QSlider *duration_slider, *volume;
    QLabel  *duration_label;
    PlayButton *play_btn;
    QToolButton *stop_btn, *prev_track, *next_track, *volume_btn;
    QComboBox *tempo;
    PlaylistWidget *tracklist, *filelist;
    QCheckBox *autoplay, *repeat_track, *repeat_file;
    RecentList *recent_files, *recent_playlists;
    std::vector<QWidget *> to_enable;
    MprisPlayer *mpris;

    void load_shortcuts();
    void open_playlist(const QString &filename);
    void open_single_file(QString filename);
    void finish_opening();
    void start_or_resume();
    void pause();
    void stop();
    void edit_settings();
    void edit_shortcuts();
    // listeners to events sent by qt
    void closeEvent(QCloseEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

    void add_to_enable(auto... objects)
    {
        (to_enable.push_back(objects), ...);
    }

    void update_next_prev_track()
    {
        next_track->setEnabled(bool(player->get_next()));
        prev_track->setEnabled(player->get_prev_track() || bool(player->get_prev_file()));
    }

public:
    explicit MainWindow(QWidget *parent = nullptr);
};
