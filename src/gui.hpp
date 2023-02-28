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
#include "keyrecorder.hpp"
#include "error.hpp"

namespace gmplayer { class Player; }
class QShortcut;
class QMenu;
class QToolButton;
class QSlider;
class QLabel;

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
    QStringList names;
    QMenu *menu;
public:
    RecentList(QMenu *menu, const QStringList &list);
    const QStringList &filenames() { return names; }
    void add(const QString &name);
signals:
    void clicked(const QString &filename);
};

struct AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget *parent = nullptr);
};

class MainWindow : public QMainWindow {
    Q_OBJECT

    gmplayer::Player *player              = nullptr;
    QString last_file                     = ".";
    bool was_paused                       = false;
    std::map<QString, Shortcut> shortcuts = {};
    QToolButton *prev_track               = nullptr,
                *next_track               = nullptr,
                *play_btn                 = nullptr;
    RecentList *recent_files              = nullptr,
               *recent_playlists          = nullptr;
    QLabel *title                         = nullptr,
           *game                          = nullptr,
           *system                        = nullptr,
           *author                        = nullptr,
           *comment                       = nullptr,
           *dumper                        = nullptr,
           *duration_label                = nullptr;
    QSlider *duration_slider              = nullptr;
    std::vector<QWidget *> to_enable      = {};

    void update_next_prev_track();
    std::optional<QString> file_dialog(const QString &window_name, const QString &desc);
    QString save_dialog(const QString &window_name, const QString &desc);
    void load_shortcuts();
    void open_playlist(const QString &filename);
    void open_single_file(const QString &filename);
    void edit_settings();
    void edit_shortcuts();
    void handle_error(gmplayer::Error error);
    void closeEvent(QCloseEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    void add_to_enable(auto... objects) { (to_enable.push_back(objects), ...); }

public:
    MainWindow(gmplayer::Player *player, QWidget *parent = nullptr);
};

} // namespace gui
