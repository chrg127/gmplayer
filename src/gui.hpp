#pragma once

#include <map>
#include <span>
#include <string>
#include <optional>
#include <filesystem>
#include <QObject>
#include <QWidget>
#include <QMainWindow>
#include <QDialog>
#include <QPushButton>
#include <QStringList>
#include <QGraphicsView>
#include "common.hpp"
#include "keyrecorder.hpp"
#include "error.hpp"
#include "const.hpp"
#include "player.hpp"
#include "flags.hpp"
#include "conf.hpp"

class QShortcut;
class QMenu;
class QToolButton;
class QSlider;
class QLabel;
class QListWidget;
class QGraphicsScene;
class QGraphicsView;

namespace gui {

class SettingsWindow : public QDialog {
    Q_OBJECT
public:
    SettingsWindow(gmplayer::Player *player, QWidget *parent = nullptr);
};

struct Shortcut {
    QShortcut *shortcut;
    QString display_name;
    std::string key;
};

class ShortcutsWindow : public QDialog {
    Q_OBJECT
public:
    explicit ShortcutsWindow(const std::vector<Shortcut> &shortcuts);
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

// keeps track of recently opened files
class RecentList : public QObject {
    Q_OBJECT
    std::vector<std::filesystem::path> paths;
    QMenu *menu;
public:
    RecentList(QMenu *menu, std::vector<std::filesystem::path> &&paths);
    std::span<std::filesystem::path> filenames() { return paths; }
    void add(std::filesystem::path path);
    void regen();
signals:
    void clicked(std::filesystem::path path);
};

struct AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget *parent = nullptr);
};

class Playlist : public QWidget {
    Q_OBJECT
    QListWidget *list;
public:
    explicit Playlist(gmplayer::Player::List type, gmplayer::Player *player, QWidget *parent = nullptr);
    QListWidget *get_list() { return list; }
    void set_current(int n);
    int current() const;
    void setup_context_menu(auto &&fn);
signals:
    void context_menu(const QPoint &p);
};

class PlaylistTab : public QWidget {
    Q_OBJECT
    Playlist *filelist, *tracklist;
public:
    PlaylistTab(gmplayer::Player *player, QWidget *parent = nullptr);
    int current_file()  const { return filelist->current(); }
    int current_track() const { return tracklist->current(); }

    void setup_context_menu(gmplayer::Player::List which, auto &&fn)
    {
        (which == gmplayer::Player::List::Track ? tracklist : filelist)->setup_context_menu(fn);
    }
};

class VolumeWidget : public QWidget {
    Q_OBJECT
    QSlider *slider;
    QToolButton *mute;
public:
    explicit VolumeWidget(int start_value, int min, int max, int tick_interval = 0, QWidget *parent = nullptr);
    void set_value(int value);
signals:
    void volume_changed(int value);
};

class ChannelWidget : public QWidget {
    Q_OBJECT
    int index;
    QLabel *label;
    QSlider *volume;
public:
    ChannelWidget(int index, gmplayer::Player *player, QWidget *parent = nullptr);
    void set_name(const QString &name);
    void reset();
    void enable_volume(bool enable);
};

class VoicesTab : public QWidget {
    Q_OBJECT
public:
    VoicesTab(gmplayer::Player *player, QWidget *parent = nullptr);
};

class Controls : public QWidget {
    Q_OBJECT
    enum class SliderHistory {
        DontKnow,
        WasPaused,
        WasPlaying,
    } history = SliderHistory::DontKnow;
    std::string status_format_string;
    gmplayer::Metadata metadata;
    QLabel *status;

public:
    Controls(gmplayer::Player *player, QWidget *parent = nullptr);
    void set_status_format_string(std::string &&s);
    std::string get_status_format_string() { return status_format_string; }
};

class Details : public QDialog {
    Q_OBJECT
    std::vector<gmplayer::Metadata> ms;
public:
    Details(const gmplayer::Metadata &metadata, QWidget *parent = nullptr);
    Details(std::span<const gmplayer::Metadata> metadata, QWidget *parent = nullptr);
};

enum class OpenFilesFlags {
    AddToRecent, ClearAndPlay
};

class MainWindow : public QMainWindow {
    Q_OBJECT

    gmplayer::Player *player              = nullptr;
    QString last_file                     = ".";
    std::vector<Shortcut> shortcuts = {};
    RecentList *recent_files              = nullptr,
               *recent_playlists          = nullptr;
    Controls *controls = nullptr;

    void update_next_prev_track();
    std::optional<std::filesystem::path> file_dialog(const QString &window_name, const QString &filter);
    std::vector<std::filesystem::path> multiple_file_dialog(const QString &window_name, const QString &filter);
    QString save_dialog(const QString &window_name, const QString &filter);
    void load_shortcuts();
    std::optional<QString> add_files(std::span<std::filesystem::path> paths);
    void open_file(std::filesystem::path filename);
    void edit_settings();
    void edit_shortcuts();
    QString format_error(gmplayer::ErrType type);
    void closeEvent(QCloseEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

public:
    MainWindow(gmplayer::Player *player, QWidget *parent = nullptr);
    void open_playlist(std::filesystem::path file_path);
    void open_files(std::span<std::filesystem::path> paths, Flags<OpenFilesFlags> flags = {});
};

} // namespace gui
