#include "gui.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSlider>
#include <QToolButton>
#include "qtutils.hpp"
#include <QVBoxLayout>
#include "qtutils.hpp"
#include "player.hpp"
#include "io.hpp"
#include "appinfo.hpp"

namespace fs = std::filesystem;
using namespace gmplayer::literals;

namespace gui {

namespace {

QString format_duration(int ms, int max)
{
    return QString("%1:%2/%3:%4")
        .arg(ms  / 1000 / 60, 2, 10, QChar('0'))
        .arg(ms  / 1000 % 60, 2, 10, QChar('0'))
        .arg(max / 1000 / 60, 2, 10, QChar('0'))
        .arg(max / 1000 % 60, 2, 10, QChar('0'));
};

std::pair<QStringList, QStringList> load_recent()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "gmplayer", "gmplayer");
    settings.beginGroup("recent");
    QStringList files     = settings.value("recent_files").toStringList();
    QStringList playlists = settings.value("recent_playlists").toStringList();
    settings.endGroup();
    return {files, playlists};
}

void save_recent(const QStringList &files, const QStringList &playlists)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "gmplayer", "gmplayer");
    settings.beginGroup("recent");
    settings.setValue("recent_files", files);
    settings.setValue("recent_playlists", playlists);
    settings.endGroup();
}

QString load_last_dir()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "gmplayer", "gmplayer");
    settings.beginGroup("directories");
    QString last_dir = settings.value("last_visited", ".").toString();
    settings.endGroup();
    return last_dir;
}

void save_last_dir(const QString &dir)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "gmplayer", "gmplayer");
    settings.beginGroup("directories");
    settings.setValue("last_visited", dir);
    settings.endGroup();
}

void save_shortcuts(const std::map<QString, Shortcut> &shortcuts)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "gmplayer", "gmplayer");
    settings.beginGroup("shortcuts");
    for (auto [name, obj] : shortcuts)
        settings.setValue(name, obj.shortcut->key().toString());
    settings.endGroup();
}

void update_list(QListWidget *list, const auto &names)
{
    list->clear();
    for (auto &name : names)
        new QListWidgetItem(QString::fromStdString(name), list);
}

} // namespace



RecentList::RecentList(QMenu *menu, const QStringList &list)
    : menu(menu), names(std::move(list))
{
    for (auto &path : names) {
        auto filename = QFileInfo(path).fileName();
        auto *act = new QAction(filename, this);
        connect(act, &QAction::triggered, this, [=, this]() { emit clicked(path); });
        menu->addAction(act);
    }
}

void RecentList::add(const QString &name)
{
    auto it = std::remove(names.begin(), names.end(), name);
    if (it != names.end())
        names.erase(it);
    names.prepend(name);
    while (names.size() > 10)
        names.removeLast();
    menu->clear();
    for (auto &path : names) {
        auto filename = QFileInfo(path).fileName();
        auto *act = new QAction(filename, this);
        connect(act, &QAction::triggered, this, [=, this]() { emit clicked(path); });
        menu->addAction(act);
    }
}



SettingsWindow::SettingsWindow(gmplayer::Player *player, QWidget *parent)
{
    auto options = player->options();

    auto *fade              = make_checkbox(tr("&Enable fade-out"), options.fade_out != 0);
    auto *fade_secs         = make_spinbox(std::numeric_limits<int>::max(), options.fade_out / 1000, fade->isChecked());
    auto *default_duration  = make_spinbox(10_min / 1000, options.default_duration / 1000);
    auto *silence_detection = make_checkbox(tr("Do silence detection"), options.silence_detection);

    connect(fade, &QCheckBox::stateChanged, this, [=, this](int state) {
        fade_secs->setEnabled(state);
        if (!state)
            fade_secs->setValue(0);
    });

    auto *button_box = new QDialogButtonBox(QDialogButtonBox::Ok
                                          | QDialogButtonBox::Cancel);
    connect(button_box, &QDialogButtonBox::accepted, this, [=, this]() {
        player->set_fade(fade_secs->value());
        player->set_default_duration(default_duration->value());
        player->set_silence_detection(silence_detection->isChecked());
        accept();
    });

    connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    setLayout(make_layout<QVBoxLayout>(
        fade,
        label_pair("Fade seconds:", fade_secs),
        label_pair("Default duration:", default_duration),
        silence_detection,
        button_box
    ));
}



ShortcutsWindow::ShortcutsWindow(const std::map<QString, Shortcut> &shortcuts)
{
    auto *layout = new QFormLayout;
    for (auto [_, obj] : shortcuts) {
        auto *edit = new RecorderButton(obj.shortcut->key().toString());
        layout->addRow(new QLabel(obj.display_name), edit);
        connect(edit, &RecorderButton::released,         this, [=, this] { edit->setText("..."); });
        connect(edit, &RecorderButton::got_key_sequence, this, [=, this](const auto &seq) {
            edit->setText(seq.toString());
            obj.shortcut->setKey(seq);
        });
    }
    auto *button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(button_box, &QDialogButtonBox::accepted, this, [=, this]() { accept(); });
    connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    setLayout(make_layout<QVBoxLayout>(layout, button_box));
}



RecorderButton::RecorderButton(const QString &text, int key_count, QWidget *parent)
    : QPushButton(text, parent), recorder(new KeyRecorder(this, key_count, this))
{
    connect(this, &QPushButton::released, this, [=, this] {
        grabKeyboard();
        grabMouse();
        recorder->start();
    });
    connect(recorder, &KeyRecorder::got_key_sequence, this, [=, this] (const auto &keys) {
        releaseMouse();
        releaseKeyboard();
        emit got_key_sequence(keys);
    });
}



AboutDialog::AboutDialog(QWidget *parent)
{
    auto *icon = new QLabel;
    icon->setPixmap(QPixmap((":/icons/gmplayer32.png")));
    auto *label = new QLabel(QString("<h2><b>gmplayer %1</b></h2>").arg(gmplayer::version));
    auto *about_label = new QLabel(gmplayer::about_text); about_label->setOpenExternalLinks(true);
    auto *lib_label   = new QLabel(gmplayer::lib_text);   lib_label->setOpenExternalLinks(true);
    auto *tabs = make_tabs(
        std::make_tuple(about_label, "About"),
        std::make_tuple(lib_label, "Libraries")
    );
    setWindowTitle("About gmplayer");
    auto *title_lt = make_layout<QHBoxLayout>(icon, label);
    title_lt->addStretch();
    setLayout(make_layout<QVBoxLayout>(title_lt, tabs));
}



MainWindow::MainWindow(gmplayer::Player *player, QWidget *parent)
    : QMainWindow(parent), player{player}
{
    setWindowTitle("gmplayer");
    setWindowIcon(QIcon(":/icons/gmplayer64.png"));
    setAcceptDrops(true);

    auto options = player->options();

    // menus
    auto *file_menu = create_menu(this, "&File",
        std::make_tuple("Open file",     [this] {
            if (auto f = file_dialog("Open file", "Game music files (*.spc *.nsf)"); f)
                open_single_file(f.value());
        }),
        std::make_tuple("Open playlist", [this] {
            if (auto f = file_dialog("Open playlist", "Playlist files (*.playlist)"); f)
                open_playlist(f.value());
        })
    );

    create_menu(this, "&Edit",
        std::make_tuple("Settings",  &MainWindow::edit_settings),
        std::make_tuple("Shortcuts", &MainWindow::edit_shortcuts)
    );

    create_menu(this, "&About",
        std::make_tuple("About gmplayer", [=, this] {
            auto *wnd = new AboutDialog();
            wnd->open();
        }),
        std::make_tuple("About Qt", &QApplication::aboutQt)
    );

    // recent files, shortcuts, open dialog position
    auto [files, playlists] = load_recent();
    recent_files     = new RecentList(file_menu->addMenu(tr("&Recent files")), files);
    recent_playlists = new RecentList(file_menu->addMenu(tr("R&ecent playlists")), playlists);
    connect(recent_files,     &RecentList::clicked, this, &MainWindow::open_single_file);
    connect(recent_playlists, &RecentList::clicked, this, &MainWindow::open_playlist);
    load_shortcuts();
    last_file = load_last_dir();

    // duration slider
    duration_slider = new QSlider(Qt::Horizontal);
    duration_label = new QLabel("00:00 / 00:00");
    connect(duration_slider, &QSlider::sliderPressed,  this, [=, this]() {
        was_paused = !player->is_playing();
        player->pause();
    });
    connect(duration_slider, &QSlider::sliderReleased, this, [=, this]() {
        handle_error(player->seek(duration_slider->value()));
        if (!was_paused)
            player->start_or_resume();
    });
    connect(duration_slider, &QSlider::sliderMoved, this, [=, this] (int ms) {
        duration_label->setText(format_duration(ms, duration_slider->maximum()));
    });

    player->on_position_changed([=, this] (int ms) {
        duration_slider->setValue(ms);
        duration_label->setText(format_duration(ms, duration_slider->maximum()));
    });

    player->on_fade_changed([=, this] (int len) { duration_slider->setRange(0, len); });
    player->on_stopped(     [=, this]           { duration_slider->setValue(0);      });

    // buttons under duration slider
    auto make_btn = [&](auto icon, auto &&fn) {
        auto *b = new QToolButton(this);
        b->setIcon(style()->standardIcon(icon));
        QObject::connect(b, &QAbstractButton::clicked, this, fn);
        return b;
    };

    play_btn        = make_btn(QStyle::SP_MediaPlay,         [=, this] { player->play_pause();         });
    auto *stop_btn  = make_btn(QStyle::SP_MediaStop,         [=, this] { handle_error(player->stop()); });
    next_track      = make_btn(QStyle::SP_MediaSkipForward,  [=, this] { handle_error(player->next()); });
    prev_track      = make_btn(QStyle::SP_MediaSkipBackward, [=, this] { handle_error(player->prev()); });
    play_btn->setEnabled(false);
    stop_btn->setEnabled(false);
    next_track->setEnabled(false);
    prev_track->setEnabled(false);

    // volume slider and button
    auto *volume_slider = new QSlider(Qt::Horizontal);
    volume_slider->setRange(0, gmplayer::get_max_volume_value());
    volume_slider->setValue(options.volume);

    connect(volume_slider, &QSlider::sliderMoved, this, [=, this] (int value) { player->set_volume(value); });
    QToolButton *volume_btn = make_btn(
        options.volume == 0 ? QStyle::SP_MediaVolumeMuted : QStyle::SP_MediaVolume,
        [=, this, last = options.volume == 0 ? gmplayer::get_max_volume_value() : options.volume] () mutable {
            int vol = volume_slider->value();
            player->set_volume(vol != 0 ? last = vol, 0 : last);
        }
    );

    player->on_volume_changed([=, this] (int value) {
        volume_slider->setValue(value);
        volume_btn->setIcon(style()->standardIcon(value == 0 ? QStyle::SP_MediaVolumeMuted
                                                             : QStyle::SP_MediaVolume));
    });

    // tempo (i.e. speedup up/slowing the song)
    auto *tempo = make_combo(options.tempo == 0.5 ? 0
                           : options.tempo == 1.0 ? 1
                           : options.tempo == 2.0 ? 2 : 1,
                           std::make_tuple("2x",   2.0),
                           std::make_tuple("1x",   1.0),
                           std::make_tuple("0.5x", 0.5));
    connect(tempo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=, this] (int i) {
        player->set_tempo(tempo->currentData().toDouble());
    });

    // track information
    title   = new QLabel;
    game    = new QLabel;
    system  = new QLabel;
    author  = new QLabel;
    comment = new QLabel;
    dumper  = new QLabel;

    // track and file playlist
    auto *tracklist     = new QListWidget;
    connect(tracklist, &QListWidget::itemActivated, this, [=, this] { handle_error(player->load_track(tracklist->currentRow())); });
    auto *track_shuffle = make_button("Shuffle",    this, [=, this] { player->shuffle(gmplayer::Player::List::Track); });
    auto *track_up      = make_button("Up",         this, [=, this] { tracklist->setCurrentRow(player->move(gmplayer::Player::List::Track, tracklist->currentRow(), -1)); });
    auto *track_down    = make_button("Down",       this, [=, this] { tracklist->setCurrentRow(player->move(gmplayer::Player::List::Track, tracklist->currentRow(), +1)); });

    auto *filelist     = new QListWidget;
    connect(filelist, &QListWidget::itemActivated,  this, [=, this] { handle_error(player->load_pair(filelist->currentRow(), 0)); });
    auto *file_shuffle = make_button("Shuffle",     this, [=, this] { player->shuffle(gmplayer::Player::List::File); });
    auto *file_up      = make_button("Up",          this, [=, this] { filelist->setCurrentRow(player->move(gmplayer::Player::List::File, filelist->currentRow(), -1)); });
    auto *file_down    = make_button("Down",        this, [=, this] { filelist->setCurrentRow(player->move(gmplayer::Player::List::File, filelist->currentRow(), +1)); });

    player->on_playlist_changed([=, this] (gmplayer::Player::List which) {
        update_list(which == gmplayer::Player::List::Track ? tracklist : filelist, player->names(which));
    });

    player->on_track_changed([=, this](int trackno, const gmplayer::Metadata &metadata) {
        title   ->setText(metadata.song.data());
        game    ->setText(metadata.game.data());
        author  ->setText(metadata.author.data());
        system  ->setText(metadata.system.data());
        comment ->setText(metadata.comment.data());
        dumper  ->setText(metadata.dumper.data());
        play_btn->setEnabled(true);
        duration_slider->setEnabled(true);
        duration_slider->setRange(0, player->length());
        update_next_prev_track();
        tracklist->setCurrentRow(trackno);
        player->start_or_resume();
    });

    player->on_file_changed([=, this] (int fileno) {
        stop_btn->setEnabled(true);
        filelist->setCurrentRow(fileno);
    });

    // context menu for file playlist, which lets the user edit the playlist
    filelist->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(filelist, &QWidget::customContextMenuRequested, this, [=, this] (const QPoint &p) {
        QMenu menu;
        menu.addAction("Add to playlist...", [=, this] {
            auto filename = file_dialog(tr("Open file"), "Game music files (*.spc *.nsf)");
            if (!filename)
                return;
            else if (auto err = player->add_file(filename.value().toStdString()); err)
                msgbox(QString("Couldn't open file %1 (%2)")
                    .arg(filename.value())
                    .arg(QString::fromStdString(err.code.message())));
            else
                update_next_prev_track();
        });
        menu.addAction("Remove from playlist", [=, this] {
            player->remove_file(filelist->currentRow());
            update_next_prev_track();
        });
        menu.addAction("Save playlist", [=, this]() {
            auto filename = save_dialog(tr("Save playlist"), "Playlist files (*.playlist)");
            auto path = fs::path(filename.toStdString());
            if (auto f = io::File::open(path, io::Access::Write); f)
                player->save_playlist(gmplayer::Player::List::File, f.value());
            else
                msgbox(QString("Couldn't open file %1. (%2)")
                    .arg(QString::fromStdString(path.filename().string()))
                    .arg(QString::fromStdString(f.error().message())));
        });
        menu.exec(filelist->mapToGlobal(p));
    });

    // playlist settings
    auto *autoplay     = make_checkbox("Autoplay",     options.autoplay,     this, [=, this] (int state) { player->set_autoplay(state); });
    auto *repeat_track = make_checkbox("Repeat track", options.track_repeat, this, [=, this] (int state) { player->set_track_repeat(state); });
    auto *repeat_file  = make_checkbox("Repeat file",  options.file_repeat,  this, [=, this] (int state) { player->set_file_repeat(state); });

    player->on_repeat_changed([=, this] (bool, bool) { update_next_prev_track(); });

    player->on_played(     [=, this] { play_btn->setEnabled(true); play_btn->setIcon(style()->standardIcon(QStyle::SP_MediaPause)); });
    player->on_paused(     [=, this] {                             play_btn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));  });
    player->on_track_ended([=, this] {                             play_btn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));  });
    player->on_shuffled(   [=, this] (gmplayer::Player::List l) {
        if (l == gmplayer::Player::List::Track)
            player->load_track(0);
        else
            player->load_pair(0, 0);
    });

    // disable everything
    add_to_enable(
        tracklist, track_shuffle, track_up, track_down,
        filelist, file_shuffle, file_up, file_down
    );
    for (auto &w : to_enable)
        w->setEnabled(false);

    // create the gui
    auto *center = new QWidget(this);
    setCentralWidget(center);
    center->setLayout(
        make_layout<QVBoxLayout>(
            make_layout<QHBoxLayout>(
                make_layout<QVBoxLayout>(
                    new QLabel("Track playlist"),
                    tracklist,
                    make_layout<QHBoxLayout>(track_shuffle, track_up, track_down)
                ),
                make_layout<QVBoxLayout>(
                    new QLabel("File playlist"),
                    filelist,
                    make_layout<QHBoxLayout>(file_shuffle, file_up, file_down)
                )
            ),
            make_layout<QHBoxLayout>(
                make_groupbox<QFormLayout>("Track info",
                    std::make_tuple(new QLabel(tr("Title:")),   title),
                    std::make_tuple(new QLabel(tr("Game:")),    game),
                    std::make_tuple(new QLabel(tr("System:")),  system),
                    std::make_tuple(new QLabel(tr("Author:")),  author),
                    std::make_tuple(new QLabel(tr("Comment:")), comment),
                    std::make_tuple(new QLabel(tr("Dumper:")),  dumper)
                ),
                make_groupbox<QVBoxLayout>("Playlist settings",
                    autoplay, repeat_track, repeat_file
                )
            ),
            make_layout<QHBoxLayout>(
                duration_slider,
                duration_label
            ),
            make_layout<QHBoxLayout>(
                prev_track,
                play_btn,
                next_track,
                stop_btn,
                new QLabel(tr("Tempo:")),
                tempo,
                volume_btn,
                volume_slider
            ),
            new QWidget
        )
    );
}

void MainWindow::update_next_prev_track()
{
    next_track->setEnabled(player->has_next());
    prev_track->setEnabled(player->has_prev());
}

std::optional<QString> MainWindow::file_dialog(const QString &window_name, const QString &desc)
{
    auto filename = QFileDialog::getOpenFileName(this, window_name, last_file, desc);
    if (filename.isEmpty())
        return std::nullopt;
    last_file = filename;
    return filename;
}

QString MainWindow::save_dialog(const QString &window_name, const QString &desc)
{
    return QFileDialog::getSaveFileName(this, window_name, last_file, desc);
}

void MainWindow::load_shortcuts()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "gmplayer", "gmplayer");
    auto add_shortcut = [&](const QString &name, const QString &display_name,
                            const QString &key, auto &&fn) {
        auto value = settings.value(name, key).toString();
        auto *shortcut = new QShortcut(QKeySequence(value), this);
        connect(shortcut, &QShortcut::activated, this, fn);
        shortcuts[name] = {
            .shortcut = shortcut,
            .name = name,
            .display_name = display_name
        };
    };
    settings.beginGroup("shortcuts");
    add_shortcut("play",  "Play/Pause",     "Ctrl+Space",   [=, this] { player->play_pause(); });
    add_shortcut("next",  "Next",           "Ctrl+Right",   [=, this] { handle_error(player->next());    });
    add_shortcut("prev",  "Previous",       "Ctrl+Left",    [=, this] { handle_error(player->prev());    });
    add_shortcut("stop",  "Stop",           "Ctrl+S",       [=, this] { handle_error(player->stop()); });
    add_shortcut("seekf", "Seek forward",   "Right",        [=, this] { handle_error(player->seek_relative(1_sec)); });
    add_shortcut("seekb", "Seek backwards", "Left",         [=, this] { handle_error(player->seek_relative(-1_sec)); });
    add_shortcut("volup", "Volume up",      "0",            [=, this] { player->set_volume_relative( 2); });
    add_shortcut("voldw", "Volume down",    "9",            [=, this] { player->set_volume_relative(-2); });
    settings.endGroup();
}

void MainWindow::open_playlist(const QString &filename)
{
    auto res = player->open_file_playlist(filename.toUtf8().constData());
    if (res.pl_error != std::error_code{}) {
        msgbox(QString("Couldn't open playlist %1 (%2).")
            .arg(filename)
            .arg(QString::fromStdString(res.pl_error.message())));
        return;
    }
    if (res.errors.size() != 0) {
        QString text;
        for (auto &e : res.errors)
            text += QString("%1: %2\n").arg(QString::fromStdString(e.first))
                                       .arg(QString::fromStdString(e.second.code.message()));
        msgbox("Errors were found while opening the playlist.",
               "Check the details for the errors.", text);
    }
    recent_playlists->add(filename);
    for (auto &w : to_enable)
        w->setEnabled(true);
    handle_error(player->load_pair(0, 0));
}

void MainWindow::open_single_file(const QString &filename)
{
    player->clear();
    auto path = fs::path(filename.toStdString());
    auto err = player->add_file(path);
    if (err) {
        msgbox(QString("Couldn't open file %1 (%2)")
            .arg(QString::fromStdString(path.filename().string()))
            .arg(QString::fromStdString(err.code.message())));
        return;
    }
    recent_files->add(filename);
    for (auto &w : to_enable)
        w->setEnabled(true);
    handle_error(player->load_pair(0, 0));
}

void MainWindow::edit_settings()
{
    auto *wnd = new SettingsWindow(player, this);
    wnd->open();
    connect(wnd, &QDialog::finished, this, [=, this](int result) {
        // reset song to start position
        // (this is due to the modified fade applying to the song)
        if (result == QDialog::Accepted)
            handle_error(player->seek(0));
    });
}

void MainWindow::edit_shortcuts()
{
    auto *wnd = new ShortcutsWindow(shortcuts);
    wnd->open();
}

void MainWindow::handle_error(gmplayer::Error error)
{
    if (!error)
        return;
    auto format_error = [&]() {
        switch (static_cast<gmplayer::ErrType>(error.code.value())) {
        case gmplayer::ErrType::Seek:      return tr("Got an error while seeking.");
        case gmplayer::ErrType::LoadFile:  return tr("Got an error while loading file '%1'")
                                            .arg(QString::fromStdString(player->current_file().filename()));
        case gmplayer::ErrType::LoadTrack: return tr("Got an error while loading track '%1' of file '%2'")
                                            .arg(QString::fromStdString(player->current_track().song));
        case gmplayer::ErrType::Play:      return tr("Got an error while playing.");
        case gmplayer::ErrType::Header:    return tr("Header of file %1 is invalid.")
                                            .arg(QString::fromStdString(player->current_file().filename()));
        case gmplayer::ErrType::FileType:  return tr("File %1 has an invalid file type.")
                                            .arg(QString::fromStdString(player->current_file().filename()));
        }
        return QString("");
    };
    msgbox(format_error(), tr("Check the details for the error."), QString::fromStdString(error.details.data()));
    switch (static_cast<gmplayer::ErrType>(error.code.value())) {
    case gmplayer::ErrType::LoadFile:
    case gmplayer::ErrType::Header:
    case gmplayer::ErrType::FileType:
        duration_slider->setRange(0, 0);
        duration_label->setText("00:00 / 00:00");
        title   ->setText("");
        game    ->setText("");
        author  ->setText("");
        system  ->setText("");
        comment ->setText("");
        dumper  ->setText("");
        update_next_prev_track();
    case gmplayer::ErrType::LoadTrack:
    case gmplayer::ErrType::Seek:
    case gmplayer::ErrType::Play:
        play_btn->setEnabled(false);
        play_btn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        player->pause();
        duration_slider->setEnabled(false);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    save_recent(recent_files->filenames(), recent_playlists->filenames());
    save_shortcuts(shortcuts);
    save_last_dir(last_file);
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("text/plain"))
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    auto mime = event->mimeData();
    if (!mime->hasUrls()) {
        msgbox("Invalid file: The dropped file may not be a music file");
        return;
    }
    auto url = mime->urls()[0];
    if (!url.isLocalFile()) {
        msgbox("Invalid file: The dropped file may not be a music file");
        return;
    }
    open_single_file(url.toLocalFile());
    event->acceptProposedAction();
}

} // namespace gui
