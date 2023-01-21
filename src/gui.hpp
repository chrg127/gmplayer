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

class QShortcut;
class QMenu;
class MprisPlayer;

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

// keeps track of recently opened files
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

struct AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget *parent = nullptr);
};

class MainWindow : public QMainWindow {
    Q_OBJECT

    Player player;
    QString last_file = ".";
    std::map<QString, Shortcut> shortcuts;
    bool was_paused = false;

    // widgets
    QToolButton *prev_track, *next_track;
    RecentList *recent_files, *recent_playlists;
    std::vector<QWidget *> to_enable;

    void update_next_prev_track();
    std::optional<QString> file_dialog(const QString &window_name, const QString &desc);
    QString save_dialog(const QString &window_name, const QString &desc);
    void load_shortcuts();
    void open_playlist(const QString &filename);
    void open_single_file(const QString &filename);
    void edit_settings();
    void edit_shortcuts();

    // listeners to events sent by qt
    void closeEvent(QCloseEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

    void add_to_enable(auto... objects) { (to_enable.push_back(objects), ...); }

public:
    explicit MainWindow(QWidget *parent = nullptr);
};
