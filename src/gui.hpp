#pragma once

#include <map>
#include <span>
#include <string>
#include <optional>
#include <QObject>
#include <QWidget>
#include <QMainWindow>
#include <QDialog>
#include <QPushButton>
#include <QStringList>
#include <QGraphicsView>
#include "keyrecorder.hpp"
#include "error.hpp"
#include "player.hpp"
#include "common.hpp"

namespace gmplayer {
    class Player;
    class PlayerOptions;
}
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
    explicit SettingsWindow(gmplayer::Player *player, QWidget *parent = nullptr);
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
    Playlist *filelist;
public:
    PlaylistTab(gmplayer::Player *player, const gmplayer::PlayerOptions &options, QWidget *parent = nullptr);
    int current_file() const { return filelist->current(); }
    void setup_context_menu(auto &&fn) { filelist->setup_context_menu(fn); }
};

class Visualizer : public QGraphicsView {
    Q_OBJECT
    QGraphicsScene *scene;
    std::span<i16> data;
    int channel, channel_size, num_channels;
    QString name;
    void showEvent(QShowEvent *) override;
    void resizeEvent(QResizeEvent *ev) override;
public:
    Visualizer(std::span<i16> data, int channel, int channel_size, int num_channels, QWidget *parent = nullptr);
    void set_name(const QString &name) { this->name = name; }
public slots:
    void render();
};

class VisualizerTab : public QWidget {
    Q_OBJECT
    std::array<i16, gmplayer::SAMPLES_SIZE * 8> single_data = {};
    std::array<i16, gmplayer::SAMPLES_SIZE>     full_data   = {};
    Visualizer *full;
    std::array<Visualizer *, 8> single;
public:
    VisualizerTab(gmplayer::Player *player, QWidget *parent = nullptr);
signals:
    void updated();
};

class ChannelWidget : public QWidget {
    Q_OBJECT
    int index;
    QLabel *label;
public:
    ChannelWidget(int index, gmplayer::Player *player, QWidget *parent = nullptr);
    void set_name(const QString &name);
    void reset();
};

class CurrentlyPlayingTab : public QWidget {
    Q_OBJECT
public:
    CurrentlyPlayingTab(gmplayer::Player *player, QWidget *parent = nullptr);
};

class Controls : public QWidget {
    Q_OBJECT
    enum class SliderHistory {
        DontKnow,
        WasPaused,
        WasPlaying,
    } history = SliderHistory::DontKnow;
public:
    Controls(gmplayer::Player *player, const gmplayer::PlayerOptions &options, QWidget *parent = nullptr);
};

DEFINE_OPTION_ENUM(OpenFilesFlags,
    AddToRecent = 1 << 1,
    ClearAndPlay = 1 << 2,
)

class MainWindow : public QMainWindow {
    Q_OBJECT

    gmplayer::Player *player              = nullptr;
    QString last_file                     = ".";
    std::map<QString, Shortcut> shortcuts = {};
    RecentList *recent_files              = nullptr,
               *recent_playlists          = nullptr;

    void update_next_prev_track();
    std::optional<std::filesystem::path> file_dialog(const QString &window_name, const QString &filter);
    std::vector<std::filesystem::path> multiple_file_dialog(const QString &window_name, const QString &filter);
    QString save_dialog(const QString &window_name, const QString &filter);
    void load_shortcuts();
    std::optional<QString> add_files(std::span<std::filesystem::path> paths);
    void open_playlist(std::filesystem::path file_path);
    void open_file(std::filesystem::path filename);
    void edit_settings();
    void edit_shortcuts();
    QString format_error(gmplayer::ErrType type);
    void closeEvent(QCloseEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

public:
    MainWindow(gmplayer::Player *player, QWidget *parent = nullptr);
    void open_files(std::span<std::filesystem::path> paths, OpenFilesFlags flags = OpenFilesFlags::None);
};

} // namespace gui
