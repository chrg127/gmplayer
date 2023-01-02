#include "gui.hpp"

#include <tuple>
#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QSlider>
#include <QToolButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QMenuBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QSettings>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMessageBox>
#include <QShortcut>
#include <QKeySequence>
#include <QFileInfo>
#include <QMimeData>
#include <QDebug>
#include <QVariant>
#include <QVariantMap>
#include <QStandardPaths>
#include <Mpris>
#include <MprisPlayer>
#include <fmt/core.h>
#include <gme/gme.h>    // gme_info_t
#include "qtutils.hpp"
#include "player.hpp"
#include "io.hpp"

namespace fs = std::filesystem;

namespace {

QString format_duration(int ms, int max)
{
    int mins = ms / 1000 / 60;
    int secs = ms / 1000 % 60;
    int max_mins = max / 1000 / 60;
    int max_secs = max / 1000 % 60;
    return QString::fromStdString(fmt::format("{:02}:{:02}/{:02}:{:02}", mins, secs, max_mins, max_secs));
};

PlayerOptions load_player_settings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "gmplayer", "gmplayer");
    settings.beginGroup("player");
    PlayerOptions options = {
        .fade_out_ms        = settings.value("fade_out_ms",                            0).toInt(),
        .autoplay           = settings.value("autoplay",                           false).toBool(),
        .track_repeat       = settings.value("track_repeat",                       false).toBool(),
        .file_repeat        = settings.value("file_repeat",                        false).toBool(),
        .default_duration   = settings.value("default_duration",              int(3_min)).toInt(),
        .silence_detection  = settings.value("silence_detection",                      0).toInt(),
        .tempo              = settings.value("tempo",                                1.0).toDouble(),
        .volume             = settings.value("volume",            get_max_volume_value()).toInt()
    };
    settings.endGroup();
    return options;
}

void save_player_settings(PlayerOptions options)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "gmplayer", "gmplayer");
    settings.beginGroup("player");
    settings.setValue("fade_out_ms",       options.fade_out_ms);
    settings.setValue("autoplay",          options.autoplay);
    settings.setValue("track_repeat",      options.track_repeat);
    settings.setValue("file_repeat",       options.file_repeat);
    settings.setValue("default_duration",  options.default_duration);
    settings.setValue("silence_detection", options.silence_detection);
    settings.setValue("tempo",             options.tempo);
    settings.setValue("volume",            options.volume);
    settings.endGroup();
}

std::pair<QStringList, QStringList> load_recent_files()
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

void save_shortcuts(const std::map<QString, Shortcut> &shortcuts)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "gmplayer", "gmplayer");
    settings.beginGroup("shortcuts");
    for (auto [name, obj] : shortcuts)
        settings.setValue(name, obj.shortcut->key().toString());
    settings.endGroup();
}

QVariantMap make_metadata(auto&&... args)
{
    QVariantMap map;
    (map.insert(Mpris::metadataToString(std::get<0>(args)), QVariant(std::get<1>(args))), ...);
    return map;
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



SettingsWindow::SettingsWindow(Player *player, QWidget *parent)
{
    auto options = player->get_options();
    int length_ms = player->length() == 0 ? 10_min : player->length();

    auto *fade = make_checkbox(tr("&Enable fade-out"), options.fade_out_ms != 0);
    auto *fade_secs = make_spinbox(length_ms / 1000, options.fade_out_ms / 1000, fade->isChecked());
    auto *default_duration = make_spinbox(10_min / 1000, options.default_duration / 1000);
    auto *silence_detection = make_checkbox(tr("Do silence detection"), options.silence_detection == 1);

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



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("gmplayer");
    auto *center = new QWidget(this);
    setCentralWidget(center);
    setAcceptDrops(true);

    player = new Player;
    PlayerOptions options = load_player_settings();
    player->get_options() = options;

    auto *mpris = new MprisPlayer(this);
    mpris->setServiceName("gmplayer");
    mpris->setCanQuit(true);
    mpris->setCanRaise(false);
    mpris->setCanSetFullscreen(false);
    mpris->setFullscreen(false);
    mpris->setDesktopEntry(
        QStandardPaths::locate(QStandardPaths::ApplicationsLocation, "gmplayer.desktop")
    );
    mpris->setHasTrackList(false);
    mpris->setIdentity("gmplayer");
    mpris->setSupportedUriSchemes(QStringList{"file"});
    mpris->setSupportedMimeTypes(
        QStringList{"application/x-pkcs7-certificates", "application/octet-stream", "text/plain"}
    );
    mpris->setCanControl(true);
    mpris->setCanGoNext(true);
    mpris->setCanGoPrevious(true);
    mpris->setCanPause(true);
    mpris->setCanPlay(true);
    mpris->setCanSeek(true);
    mpris->setLoopStatus(Mpris::LoopStatus::None);
    mpris->setMaximumRate(2.0);
    mpris->setMinimumRate(0.5);
    mpris->setMetadata(make_metadata());
    mpris->setPlaybackStatus(Mpris::PlaybackStatus::Stopped);
    mpris->setPosition(0);
    mpris->setRate(options.tempo);
    mpris->setShuffle(false);
    mpris->setVolume(options.volume);

    connect(mpris, &MprisPlayer::quitRequested,      this, [=, this] { QApplication::quit(); });
    connect(mpris, &MprisPlayer::pauseRequested,     this, [=, this] { player->pause(); });
    connect(mpris, &MprisPlayer::playRequested,      this, [=, this] { player->start_or_resume(); });
    connect(mpris, &MprisPlayer::playPauseRequested, this, [=, this] { player->play_pause(); });
    connect(mpris, &MprisPlayer::stopRequested,      this, [=, this] { player->stop(); });
    connect(mpris, &MprisPlayer::nextRequested,      this, [=, this] { player->next(); });
    connect(mpris, &MprisPlayer::previousRequested,  this, [=, this] { player->prev(); });
    connect(mpris, &MprisPlayer::seekRequested,      this, [=, this] (auto offset) { player->seek_relative(offset); });
    connect(mpris, &MprisPlayer::rateRequested,      this, [=, this] (double rate) { player->set_tempo(rate); });
    connect(mpris, &MprisPlayer::shuffleRequested,   this, [=, this] (bool do_shuffle) {
        player->shuffle_files(do_shuffle);
        player->load_file(0);
    });
    connect(mpris, &MprisPlayer::volumeRequested,    this, [=, this] (double vol) {
        player->set_volume(std::lerp(0.0, get_max_volume_value(), vol < 0.0 ? 0.0 : vol));
    });
    connect(mpris, &MprisPlayer::openUriRequested, this, [=, this] (const QUrl &url) {
        open_single_file(url.toLocalFile());
    });
    connect(mpris, &MprisPlayer::setPositionRequested, this, [=, this] (const auto &id, qlonglong pos) {
        player->seek(pos);
    });

    connect(mpris, &MprisPlayer::loopStatusRequested, this, [=, this] (Mpris::LoopStatus status) {
        switch (status) {
        case Mpris::LoopStatus::None:
            player->set_autoplay(false);
            player->set_track_repeat(false);
            player->set_file_repeat(false);
            break;
        case Mpris::LoopStatus::Track:
            player->set_autoplay(true);
            player->set_track_repeat(true);
            player->set_file_repeat(true);
            break;
        case Mpris::LoopStatus::Playlist:
            player->set_autoplay(true);
            player->set_track_repeat(false);
            player->set_file_repeat(false);
            break;
        }
    });

    // create menus
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

    create_menu(this, "&About");

    // set up recent files
    auto [files, playlists] = load_recent_files();
    recent_files     = new RecentList(file_menu->addMenu(tr("&Recent files")), files);
    recent_playlists = new RecentList(file_menu->addMenu(tr("R&ecent playlists")), playlists);
    connect(recent_files,     &RecentList::clicked, this, &MainWindow::open_single_file);
    connect(recent_playlists, &RecentList::clicked, this, &MainWindow::open_playlist);

    // set up shortcuts
    load_shortcuts();

    // duration slider
    auto *duration_slider = new QSlider(Qt::Horizontal);
    auto *duration_label = new QLabel("00:00 / 00:00");
    connect(duration_slider, &QSlider::sliderPressed,  this, [=, this]() {
        was_paused = !player->is_playing();
        player->pause();
    });
    connect(duration_slider, &QSlider::sliderReleased, this, [=, this]() {
        player->seek(duration_slider->value());
        if (!was_paused)
            player->start_or_resume();
    });
    connect(duration_slider, &QSlider::sliderMoved, this, [=, this] (int ms) {
        duration_label->setText(format_duration(ms, duration_slider->maximum()));
    });

    player->on_position_changed([=, this](int ms) {
        duration_slider->setValue(ms);
        duration_label->setText(format_duration(ms, duration_slider->maximum()));
        // for some reason mpris uses microseconds instead of milliseconds
        mpris->setPosition(ms * 1000);
    });

    // buttons under duration slider
    auto make_btn = [&](auto icon, auto &&fn) {
        auto *b = new QToolButton(this);
        b->setIcon(style()->standardIcon(icon));
        QObject::connect(b, &QAbstractButton::clicked, this, fn);
        return b;
    };

    auto *play_btn  = make_btn(QStyle::SP_MediaPlay,         [=, this] { player->play_pause(); });
    auto *stop_btn  = make_btn(QStyle::SP_MediaStop,         [=, this] { player->stop();       });
    next_track      = make_btn(QStyle::SP_MediaSkipForward,  [=, this] { player->next();       });
    prev_track      = make_btn(QStyle::SP_MediaSkipBackward, [=, this] { player->prev();       });

    // volume slider and button
    auto *volume_slider = new QSlider(Qt::Horizontal);
    volume_slider->setRange(0, get_max_volume_value());
    volume_slider->setValue(options.volume);

    connect(volume_slider, &QSlider::sliderMoved, this, [=, this] (int value) { player->set_volume(value); });
    QToolButton *volume_btn = make_btn(
        options.volume == 0 ? QStyle::SP_MediaVolumeMuted : QStyle::SP_MediaVolume,
        [=, this, last = options.volume == 0 ? get_max_volume_value() : options.volume] () mutable {
            int vol = volume_slider->value();
            player->set_volume(vol != 0 ? last = vol, 0 : last);
        }
    );

    player->on_volume_changed([=, this] (int value) {
        volume_slider->setValue(value);
        volume_btn->setIcon(style()->standardIcon(value == 0 ? QStyle::SP_MediaVolumeMuted
                                                             : QStyle::SP_MediaVolume));
        mpris->setVolume(double(value) / double(get_max_volume_value()));
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

    player->on_tempo_changed([=, this] (double value) { mpris->setRate(value); });

    // track information
    auto *title   = new QLabel;
    auto *game    = new QLabel;
    auto *system  = new QLabel;
    auto *author  = new QLabel;
    auto *comment = new QLabel;
    auto *dumper  = new QLabel;

    // track playlist
    auto *tracklist     = new QListWidget;
    auto *track_shuffle = new QPushButton("Shuffle");
    connect(tracklist,     &QListWidget::itemActivated, this, [=, this] { player->load_track(tracklist->currentRow()); });
    connect(track_shuffle, &QPushButton::released,      this, [=, this] { player->shuffle_tracks(true); player->load_track(0); });

    // file playlist
    auto *filelist     = new QListWidget;
    auto *file_shuffle = new QPushButton("Shuffle");
    connect(filelist,     &QListWidget::itemActivated,  this, [=, this] { player->load_file(filelist->currentRow()); });
    connect(file_shuffle, &QPushButton::released,       this, [=, this] { player->shuffle_files(true); player->load_file(0); });

    player->on_file_order_changed( [=, this] (const auto &names, bool shuffled) {
        update_list(filelist, names);
        mpris->setShuffle(shuffled);
    });
    player->on_track_order_changed([=, this] (const auto &names, bool _) {
        update_list(tracklist, names);
    });

    player->on_track_changed([=, this](int trackno, gme_info_t *info, int length) {
        title   ->setText(info->song);
        game    ->setText(info->game);
        author  ->setText(info->author);
        system  ->setText(info->system);
        comment ->setText(info->comment);
        dumper  ->setText(info->dumper);
        auto len = player->effective_length();
        duration_slider->setRange(0, len);
        update_next_prev_track();
        tracklist->setCurrentRow(trackno);
        mpris->setMetadata(make_metadata(
            std::tuple{ Mpris::Metadata::TrackId, QString("/%1%2").arg(player->current_file()).arg(trackno) },
            std::tuple{ Mpris::Metadata::Length,  len },
            std::tuple{ Mpris::Metadata::Title,   QString(info->song) },
            std::tuple{ Mpris::Metadata::Album,   QString(info->game) },
            std::tuple{ Mpris::Metadata::Artist,  QString(info->author) }
        ));
        player->start_or_resume();
    });

    player->on_file_changed([=, this] (int fileno) {
        filelist->setCurrentRow(fileno);
        player->load_track(0);
    });

    // context menu for file playlist, which lets the user edit the playlist
    filelist->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(filelist, &QWidget::customContextMenuRequested, this, [=, this] (const QPoint &p) {
        QMenu menu;
        menu.addAction("Add to playlist...", [=, this] {
            auto filename = file_dialog(tr("Open file"), "Game music files (*.spc *.nsf)");
            if (!filename)
                return;
            else if (auto err = player->add_file(filename.value().toStdString()); err != std::error_code())
                msgbox(QString("Couldn't open file %1 (%2)")
                    .arg(filename.value())
                    .arg(QString::fromStdString(err.message())));
            else
                update_next_prev_track();
        });
        menu.addAction("Remove from playlist", [=, this] {
            if (!player->remove_file(filelist->currentRow()))
                msgbox("Cannot remove currently playing file!");
            else
                update_next_prev_track();
        });
        menu.addAction("Save playlist", [=, this]() {
            auto filename = save_dialog(tr("Save playlist"), "Playlist files (*.playlist)");
            auto path = fs::path(filename.toStdString());
            if (auto f = io::File::open(path, io::Access::Write); f)
                player->save_file_playlist(f.value());
            else
                msgbox(QString("Couldn't open file %1. (%2)")
                    .arg(QString::fromStdString(path.filename()))
                    .arg(QString::fromStdString(f.error().message())));
        });
        menu.exec(filelist->mapToGlobal(p));
    });

    // playlist settings
    auto *autoplay     = make_checkbox("Autoplay",     options.autoplay,     this, [=, this] (int state) { player->set_autoplay(state); });
    auto *repeat_track = make_checkbox("Repeat track", options.track_repeat, this, [=, this] (int state) { player->set_track_repeat(state); });
    auto *repeat_file  = make_checkbox("Repeat file",  options.file_repeat,  this, [=, this] (int state) { player->set_file_repeat(state); });

    player->on_repeat_changed([=, this] (bool autoplay, bool repeat_track, bool repeat_file) {
        if (!autoplay)
            mpris->setLoopStatus(Mpris::LoopStatus::Track);
        else if (repeat_track)
            mpris->setLoopStatus(Mpris::LoopStatus::Track);
        else
            mpris->setLoopStatus(Mpris::LoopStatus::Playlist);
        update_next_prev_track();
    });

    player->on_played([=, this] {
        play_btn->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
        mpris->setPlaybackStatus(Mpris::PlaybackStatus::Playing);
    });

    player->on_paused([=, this] {
        play_btn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        mpris->setPlaybackStatus(Mpris::PlaybackStatus::Paused);
    });

    player->on_stopped    ([=, this] { duration_slider->setValue(0); });
    player->on_track_ended([=, this] { play_btn->setIcon(style()->standardIcon(QStyle::SP_MediaPause)); });

    // disable everything
    add_to_enable(
        duration_slider, play_btn, prev_track, next_track, stop_btn, tempo,
        volume_slider, volume_btn, tracklist, track_shuffle, filelist,
        file_shuffle, autoplay, repeat_track, repeat_file
    );
    for (auto &w : to_enable)
        w->setEnabled(false);

    // create the gui
    center->setLayout(
        make_layout<QVBoxLayout>(
            make_layout<QHBoxLayout>(
                make_layout<QVBoxLayout>(new QLabel("Track playlist"), tracklist, track_shuffle),
                make_layout<QVBoxLayout>(new QLabel("File playlist"), filelist,  file_shuffle)
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
    next_track->setEnabled(player->get_next_track() || player->get_next_file());
    prev_track->setEnabled(player->get_prev_track() || player->get_prev_file());
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
    add_shortcut("next",  "Next",           "Ctrl+Right",   [=, this] { player->next();    });
    add_shortcut("prev",  "Previous",       "Ctrl+Left",    [=, this] { player->prev();    });
    add_shortcut("stop",  "Stop",           "Ctrl+S",       [=, this] { player->stop(); });
    add_shortcut("seekf", "Seek forward",   "Right",        [=, this] { player->seek_relative(1_sec); });
    add_shortcut("seekb", "Seek backwards", "Left",         [=, this] { player->seek_relative(1_sec); });
    add_shortcut("volup", "Volume up",      "0",            [=, this] { player->set_volume_relative( 2); });
    add_shortcut("voldw", "Volume down",    "9",            [=, this] { player->set_volume_relative(-2); });
    settings.endGroup();
}

void MainWindow::open_playlist(const QString &filename)
{
    auto res = player->open_file_playlist(filename.toUtf8().constData());
    if (res.pl_error != std::error_code{}) {
        msgbox(QString("Couldn't open file %1 (%2).")
            .arg(filename)
            .arg(QString::fromStdString(res.pl_error.message())));
        return;
    }
    if (res.not_opened.size() != 0) {
        QString text;
        text += "Files not opened:\n";
        for (auto &file : res.not_opened)
            text += QString::fromStdString(file) + "\n";
        text += "Errors found:\n";
        for (auto &err : res.errors)
            text += QString::fromStdString(err.message()) + "\n";
        msgbox("Errors were found while opening the playlist.",
               "Check the details for the errors.", text);
    }
    recent_playlists->add(filename);
    player->load_file(0);
    for (auto &w : to_enable)
        w->setEnabled(true);
}

void MainWindow::open_single_file(const QString &filename)
{
    player->clear_file_playlist();
    auto path = fs::path(filename.toStdString());
    auto err = player->add_file(path);
    if (err != std::error_condition{}) {
        msgbox(QString("Couldn't open file %1 (%2)")
            .arg(QString::fromStdString(path.filename()))
            .arg(QString::fromStdString(err.message())));
        return;
    }
    recent_files->add(filename);
    player->load_file(0);
    for (auto &w : to_enable)
        w->setEnabled(true);
}

void MainWindow::edit_settings()
{
    auto *wnd = new SettingsWindow(player, this);
    wnd->open();
    connect(wnd, &QDialog::finished, this, [=, this](int result) {
        // reset song to start position
        // (this is due to the modified fade applying to the song)
        if (result == QDialog::Accepted)
            player->seek(0);
    });
}

void MainWindow::edit_shortcuts()
{
    auto *wnd = new ShortcutsWindow(shortcuts);
    wnd->open();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    save_player_settings(player->get_options());
    save_recent(recent_files->filenames(), recent_playlists->filenames());
    save_shortcuts(shortcuts);
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
