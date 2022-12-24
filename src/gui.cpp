#include "gui.hpp"

#include <tuple>
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
#include <Mpris>
#include <MprisPlayer>
#include <fmt/core.h>
#include <gme/gme.h>    // gme_info_t
#include "qtutils.hpp"
#include "player.hpp"
#include "io.hpp"

namespace fs = std::filesystem;



namespace {

std::string format_duration(int ms, int max)
{
    int mins = ms / 1000 / 60;
    int secs = ms / 1000 % 60;
    int max_mins = max / 1000 / 60;
    int max_secs = max / 1000 % 60;
    return fmt::format("{:02}:{:02}/{:02}:{:02}", mins, secs, max_mins, max_secs);
};

PlayerOptions load_player_settings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "gmplayer", "gmplayer");
    settings.beginGroup("player");
    PlayerOptions options = {
        .fade_out_ms        = settings.value("fade_out_ms",                            0).toInt(),
        .autoplay           = settings.value("autoplay",                           false).toBool(),
        .track_repeat       = settings.value("track_repeat",                       false).toBool(),
        .track_shuffle      = settings.value("track_shuffle",                      false).toBool(),
        .file_repeat        = settings.value("file_repeat",                        false).toBool(),
        .file_shuffle       = settings.value("file_shuffle",                       false).toBool(),
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
    settings.setValue("track_shuffle",     options.track_shuffle);
    settings.setValue("file_repeat",       options.file_repeat);
    settings.setValue("file_shuffle",      options.file_shuffle);
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

} // namespace



PlayButton::PlayButton(QWidget *parent)
    : QToolButton(parent)
{
    setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    connect(this, &QToolButton::clicked, this, [&]() {
        set_state(state == State::Play ? State::Pause : State::Play);
        emit state == State::Pause ? pause() : play();
    });
}

void PlayButton::set_state(State state)
{
    this->state = state;
    setIcon(style()->standardIcon(state == State::Pause ? QStyle::SP_MediaPlay
                                                        : QStyle::SP_MediaPause));
}



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
    int length_ms = player->length();
    length_ms = length_ms == 0 ? 10_min : length_ms;

    auto *fade = new QCheckBox(tr("&Enable fade-out"));
    fade->setChecked(options.fade_out_ms != 0);

    auto *fade_secs = new QSpinBox;
    fade_secs->setMaximum(length_ms / 1000);
    fade_secs->setValue(options.fade_out_ms / 1000);
    fade_secs->setEnabled(fade->isChecked());

    connect(fade, &QCheckBox::stateChanged, this, [=, this](int state) {
        fade_secs->setEnabled(state);
        if (!state)
            fade_secs->setValue(0);
    });

    auto *default_duration = new QSpinBox;
    default_duration->setMaximum(10_min / 1000);
    default_duration->setValue(options.default_duration / 1000);

    auto *silence_detection = new QCheckBox(tr("Do silence detection"));
    silence_detection->setChecked(options.silence_detection == 1);

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
        connect(edit, &RecorderButton::released, this, [=, this] {
            edit->setText("...");
        });
        connect(edit, &RecorderButton::got_key_sequence, this, [=, this](const auto &seq) {
            edit->setText(seq.toString());
            obj.shortcut->setKey(seq);
        });
    }
    auto *button_box = new QDialogButtonBox(QDialogButtonBox::Ok
                                          | QDialogButtonBox::Cancel);
    connect(button_box, &QDialogButtonBox::accepted, this, [=, this]() { accept(); });
    connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    setLayout(make_layout<QVBoxLayout>(layout, button_box));
}



RecorderButton::RecorderButton(const QString &text, int key_count, QWidget *parent)
    : QPushButton(text, parent)
    , recorder(new KeyRecorder(this, key_count, this))
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



PlaylistWidget::PlaylistWidget(const QString &name, QWidget *parent)
    : QWidget(parent)
{
    list = new QListWidget;
    shuffle = new QPushButton("Shuffle");
    connect(list, &QListWidget::itemActivated, this, &PlaylistWidget::item_activated);
    connect(shuffle, &QPushButton::released, this, &PlaylistWidget::shuffle_selected);
    setLayout(make_layout<QVBoxLayout>(new QLabel(name), list, shuffle));
}

void PlaylistWidget::update_names(int n, std::vector<std::string> &&names)
{
    list->clear();
    for (auto &name : names)
        new QListWidgetItem(QString::fromStdString(name), list);
    list->setCurrentRow(n);
}

void PlaylistWidget::setup_context_menu(std::function<void(const QPoint &)> fn)
{
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list, &QWidget::customContextMenuRequested, this, fn);
}

void PlaylistWidget::set_current(int n) { list->setCurrentRow(n); }
int  PlaylistWidget::current()          { return list->currentRow(); }
QPoint PlaylistWidget::map_point(const QPoint &p) { return list->mapToGlobal(p); }

QVariantMap make_metadata(auto&&... args)
{
    QVariantMap map;
    (map.insert(Mpris::metadataToString(std::get<0>(args)), QVariant(std::get<1>(args))), ...);
    return map;
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

    mpris = new MprisPlayer(this);
    mpris->setCanGoNext(true);
    mpris->setServiceName("fuckyou");
    mpris->setCanQuit(true);
    mpris->setCanRaise(false);
    mpris->setCanSetFullscreen(false);
    // mpris->setDesktopEntry();
    mpris->setHasTrackList(true);
    mpris->setIdentity("gmplayer");
    mpris->setSupportedUriSchemes(QStringList{"file"});
    // mpris->setSupportedMimeTypes();
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
    mpris->setRate(1.0);
    mpris->setShuffle(false);
    mpris->setVolume(1.0);

    connect(mpris, &MprisPlayer::pauseRequested, this, [=, this] { pause(); });
    connect(mpris, &MprisPlayer::playRequested,  this, [=, this] { start_or_resume(); });
    connect(mpris, &MprisPlayer::playPauseRequested,  this, [=, this] {
        fmt::print("play pause requested\n");
        if (player->is_playing())
            pause();
        else
            start_or_resume();
    });

    auto *file_menu = create_menu(this, "&File",
        std::make_tuple("Open file", [=, this] () {
            // auto filename = QString("test_files/rudra.spc");
            auto filename = QFileDialog::getOpenFileName(
                this,
                tr("Open file"),
                last_file,
                "Game music files (*.spc *.nsf)"
            );
            if (!filename.isEmpty()) {
                last_file = filename;
                open_single_file(filename);
            }
        }),
        std::make_tuple("Open playlist", [=, this](){
            // auto filename = QString("test_files/a.playlist");
            auto filename = QFileDialog::getOpenFileName(
                this,
                tr("Open playlist file"),
                last_playlist,
                "Playlist files (*.playlist)"
            );
            if (!filename.isEmpty()) {
                last_playlist = filename;
                open_playlist(filename);
            }
        })
    );

    auto [files, playlists] = load_recent_files();
    recent_files     = new RecentList(file_menu->addMenu(tr("&Recent files")), files);
    recent_playlists = new RecentList(file_menu->addMenu(tr("R&ecent playlists")), playlists);
    connect(recent_files,     &RecentList::clicked, this, [=, this](const QString &name) { open_single_file(name); });
    connect(recent_playlists, &RecentList::clicked, this, [=, this](const QString &name) { open_playlist(name); });

    create_menu(this, "&Edit",
        std::make_tuple("Settings",  &MainWindow::edit_settings),
        std::make_tuple("Shortcuts", &MainWindow::edit_shortcuts)
    );

    create_menu(this, "&About");

    // duration slider
    duration_label  = new QLabel("00:00 / 00:00");
    duration_slider = new QSlider(Qt::Horizontal);

    connect(duration_slider, &QSlider::sliderPressed,  this, [=, this]() {
        was_paused = !player->is_playing();
        pause();
    });
    connect(duration_slider, &QSlider::sliderReleased, this, [=, this]() {
        if (!was_paused)
            start_or_resume();
    });
    connect(duration_slider, &QSlider::sliderMoved, this, [=, this](int ms) {
        player->seek(ms);
        auto str = format_duration(ms, duration_slider->maximum());
        duration_label->setText(QString::fromStdString(str));
    });

    player->on_position_changed([=, this](int ms) {
        duration_slider->setValue(ms);
        auto str = format_duration(ms, duration_slider->maximum());
        duration_label->setText(QString::fromStdString(str));
    });

    // buttons under duration slider
    auto make_btn = [&](auto icon, auto &&fn) {
        auto *b = new QToolButton(this);
        b->setIcon(style()->standardIcon(icon));
        QObject::connect(b, &QAbstractButton::clicked, this, fn);
        return b;
    };

    prev_track = make_btn(QStyle::SP_MediaSkipBackward, [=, this]() { player->prev(); });
    next_track = make_btn(QStyle::SP_MediaSkipForward,  [=, this]() { player->next(); });
    stop_btn   = make_btn(QStyle::SP_MediaStop,         &MainWindow::stop);

    play_btn = new PlayButton;
    connect(play_btn, &PlayButton::play,  this, [=, this]() { start_or_resume(); });
    connect(play_btn, &PlayButton::pause, this, [=, this]() { pause(); });

    player->on_track_ended([=, this]() {
        play_btn->set_state(PlayButton::State::Pause);
    });

    tempo = new QComboBox;
    tempo->addItem("2x",     2.0);
    tempo->addItem("Normal", 1.0);
    tempo->addItem("0.5x",   0.5);
    tempo->setCurrentIndex(options.tempo == 2.0 ? 2
                         : options.tempo == 1.0 ? 1
                         : options.tempo == 0.5 ? 0
                         : 1);
    connect(tempo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=, this] (int i) {
        player->set_tempo(tempo->currentData().toDouble());
    });

    volume = new QSlider(Qt::Horizontal);
    volume->setRange(0, get_max_volume_value());
    volume->setValue(options.volume);
    volume_btn = make_btn(QStyle::SP_MediaVolume,
        [=, this, last_volume = options.volume] () mutable {
        if (volume->value() != 0) {
            last_volume = volume->value();
            volume->setValue(0);
            volume_btn->setIcon(style()->standardIcon(QStyle::SP_MediaVolumeMuted));
        } else {
            volume->setValue(last_volume);
            volume_btn->setIcon(style()->standardIcon(QStyle::SP_MediaVolume));
        }
    });
    connect(volume, &QSlider::valueChanged, this, [=, this]() {
        player->set_volume(volume->value());
        volume_btn->setIcon(style()->standardIcon(
            volume->value() == 0 ? QStyle::SP_MediaVolumeMuted
                                 : QStyle::SP_MediaVolume
        ));
    });

    // track information
    auto *title   = new QLabel;
    auto *game    = new QLabel;
    auto *system  = new QLabel;
    auto *author  = new QLabel;
    auto *comment = new QLabel;

    // track playlist
    tracklist = new PlaylistWidget("Track playlist");
    connect(tracklist, &PlaylistWidget::item_activated, this, [&]() {
        player->load_track(tracklist->current());
    });
    connect(tracklist, &PlaylistWidget::shuffle_selected, this, [&]() {
        player->shuffle_tracks();
        tracklist->update_names(0, player->track_names());
    });

    player->on_track_changed([=, this](int num, gme_info_t *info, int length) {
        title   ->setText(info->song);
        game    ->setText(info->game);
        author  ->setText(info->author);
        system  ->setText(info->system);
        comment ->setText(info->comment);
        duration_slider->setRange(0, player->effective_length());
        update_next_prev_track();
        tracklist->set_current(num);

        mpris->setMetadata(make_metadata(
            std::tuple{ Mpris::Metadata::Title,  QString(info->song) },
            std::tuple{ Mpris::Metadata::Album,  QString(info->game) },
            std::tuple{ Mpris::Metadata::Artist, QString(info->author) }
        ));

        start_or_resume();
    });

    // file playlist
    filelist = new PlaylistWidget("File playlist");
    connect(filelist, &PlaylistWidget::item_activated, this, [&]() {
        player->load_file(filelist->current());
    });
    connect(filelist, &PlaylistWidget::shuffle_selected, this, [&]() {
        player->shuffle_files();
        filelist->update_names(0, player->file_names());
        player->load_file(filelist->current());
    });

    player->on_file_changed([&](int fileno) {
        tracklist->update_names(player->load_track(0), player->track_names());
        filelist->set_current(fileno);
    });

    // context menu for file playlist, which lets the user edit the playlist
    filelist->setup_context_menu([&](const QPoint &p) {
        QPoint pos = filelist->map_point(p);
        QMenu menu;
        menu.addAction("Add to playlist...",    [&]() {
            auto filename = QFileDialog::getOpenFileName(
                this,
                tr("Open file"),
                last_file,
                "Game music files (*.spc *.nsf)"
            );
            if (filename.isEmpty())
                return;
            last_file = filename;
            auto err = player->add_file(filename.toStdString());
            if (err != std::error_code()) {
                msgbox(QString("Couldn't open file %1 (%2)")
                    .arg(filename)
                    .arg(QString::fromStdString(err.message())));
                return;
            }
            filelist->update_names(0, player->file_names());
            update_next_prev_track();
        });
        menu.addAction("Remove from playlist",  [&]() {
            if (!player->remove_file(filelist->current())) {
                msgbox("Cannot remove currently playing file!");
                return;
            }
            filelist->update_names(0, player->file_names());
            update_next_prev_track();
        });
        menu.addAction("Save playlist",         [&]() {
            auto filename = QFileDialog::getSaveFileName(
                this,
                tr("Save playlist"),
                last_playlist,
                "Playlist files (*.playlist)"
            );
            if (auto f = io::File::open(fs::path(filename.toStdString()), io::Access::Write); f)
                player->save_file_playlist(f.value());
            else {
                msgbox(QString("Couldn't open file %1. (%2)")
                    .arg(filename)
                    .arg(QString::fromStdString(f.error().message())));
                return;
            }
        });
        menu.exec(pos);
    });

    // playlist settings
    autoplay     = make_checkbox("Autoplay",     options.autoplay,     this, [=, this] (int state) {
        player->set_autoplay(state);
    });
    repeat_track = make_checkbox("Repeat track", options.track_repeat, this, [=, this] (int state) {
        player->set_track_repeat(state);
        update_next_prev_track();
    });
    repeat_file  = make_checkbox("Repeat file", options.file_repeat, this, [=, this] (int state) {
        player->set_file_repeat(state);
        update_next_prev_track();
    });

    // load shortcuts only after everything has been constructed
    load_shortcuts();

    // disable everything
    add_to_enable(
        duration_slider, play_btn, prev_track, next_track, stop_btn, tempo,
        volume, volume_btn, tracklist->playlist(), tracklist->shuffle_btn(),
        filelist->playlist(), filelist->shuffle_btn(),
        autoplay, repeat_track, repeat_file
    );
    for (auto &w : to_enable)
        w->setEnabled(false);

    // create the gui
    center->setLayout(
        make_layout<QVBoxLayout>(
            make_layout<QHBoxLayout>(
                tracklist, filelist
            ),
            make_layout<QHBoxLayout>(
                make_groupbox<QFormLayout>("Track info",
                    std::make_tuple(new QLabel(tr("Title:")),   title),
                    std::make_tuple(new QLabel(tr("Game:")),    game),
                    std::make_tuple(new QLabel(tr("System:")),  system),
                    std::make_tuple(new QLabel(tr("Author:")),  author),
                    std::make_tuple(new QLabel(tr("Comment:")), comment)
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
                volume
            ),
            new QWidget
        )
    );
}

void MainWindow::load_shortcuts()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "gmplayer", "gmplayer");
    auto add_shortcut = [&](const QString &name,
                            const QString &display_name,
                            const QString &key,
                            auto &&fn) {
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
    add_shortcut("play", "Play/Pause",      "Ctrl+Space",   [=, this] { player->is_playing() ? pause() : start_or_resume(); });
    add_shortcut("next",  "Next",           "Ctrl+Right",   [=, this] { if (player->can_play()) player->next();    });
    add_shortcut("prev",  "Previous",       "Ctrl+Left",    [=, this] { if (player->can_play()) player->prev();    });
    add_shortcut("stop",  "Stop",           "Ctrl+S",       &MainWindow::stop);
    add_shortcut("seekf", "Seek forward",   "Right",        [=, this] { if (player->can_play()) player->seek(player->position() + 1_sec); });
    add_shortcut("seekb", "Seek backwards", "Left",         [=, this] { if (player->can_play()) player->seek(player->position() - 1_sec); });
    add_shortcut("volup", "Volume up",      "0",            [=, this] { volume->setValue(volume->value() + 2); });
    add_shortcut("voldw", "Volume down",    "9",            [=, this] { volume->setValue(volume->value() - 2); });
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
        QMessageBox box;
        box.setText("Errors were found while opening the playlist.");
        box.setInformativeText("Check the details for the errors.");
        QString text;
        text += "Files not opened:\n";
        for (auto &file : res.not_opened)
            text += QString::fromStdString(file) + "\n";
        text += "Errors found:\n";
        for (auto &err : res.errors)
            text += QString::fromStdString(err) + "\n";
        box.setDetailedText(text);
        box.exec();
    }
    recent_playlists->add(filename);
    finish_opening();
}

void MainWindow::open_single_file(QString filename)
{
    player->clear_file_playlist();
    auto err = player->add_file(filename.toStdString());
    if (err != std::error_code()) {
        msgbox(QString("Couldn't open file %1 (%2)")
            .arg(filename)
            .arg(QString::fromStdString(err.message())));
        return;
    }
    recent_files->add(filename);
    finish_opening();
}

void MainWindow::finish_opening()
{
    filelist ->update_names(player->load_file(0), player->file_names());
    tracklist->update_names(player->load_track(0), player->track_names());
    for (auto &w : to_enable)
        w->setEnabled(true);
    update_next_prev_track();
    play_btn->set_state(PlayButton::State::Play);
}

void MainWindow::start_or_resume()
{
    if (player->can_play()) {
        player->start_or_resume();
        play_btn->set_state(PlayButton::State::Play);
        mpris->setPlaybackStatus(Mpris::PlaybackStatus::Playing);
    }
}

void MainWindow::pause()
{
    if (player->can_play()) {
        player->pause();
        play_btn->set_state(PlayButton::State::Pause);
        mpris->setPlaybackStatus(Mpris::PlaybackStatus::Paused);
    }
}

void MainWindow::stop()
{
    if (player->can_play()) {
        // load track also calls on_track_changed() callback, which causes a
        // call to start_or_resume, so put pause after load.
        player->load_file(0);
        player->load_track(0);
        pause();
        duration_slider->setValue(0);
    }
}

void MainWindow::edit_settings()
{
    auto *wnd = new SettingsWindow(player, this);
    wnd->open();
    connect(wnd, &QDialog::finished, this, [=, this](int result) {
        if (result == QDialog::Accepted && player->can_play()) {
            duration_slider->setRange(0, player->effective_length());
            player->seek(0);
        }
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
    if (event->mimeData()->hasFormat("text/plain")) {
        event->acceptProposedAction();
    }
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
