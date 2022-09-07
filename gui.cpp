#include "gui.hpp"

#include <tuple>
#include <QWidget>
#include <QMenuBar>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QStatusBar>
#include <QSizePolicy>
#include <QTextEdit>
#include <QComboBox>
#include <QSlider>
#include <QStyle>
#include <QToolButton>
#include <QFileDialog>
#include <QListWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QSettings>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDebug>
#include <QMessageBox>
#include <QShortcut>
#include <QKeySequence>
#include <QFileInfo>
#include <fmt/core.h>
#include <gme/gme.h>    // gme_info_t
#include "qtutils.hpp"
#include "player.hpp"



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
    recent_files = new RecentList(file_menu->addMenu(tr("&Recent files")), files);
    connect(recent_files, &RecentList::clicked, this, [=, this](const QString &name) { open_single_file(name); });
    recent_playlists = new RecentList(file_menu->addMenu(tr("R&ecent playlists")), playlists);
    connect(recent_playlists, &RecentList::clicked, this, [=, this](const QString &name) { open_playlist(name); });

    create_menu(this, "&Edit",
        std::make_tuple("Settings",  &MainWindow::edit_settings),
        std::make_tuple("Shortcuts", &MainWindow::edit_shortcuts)
    );

    create_menu(this, "&About");

    // duration slider
    duration_label  = new QLabel("00:00 / 00:00");
    duration_slider = new QSlider(Qt::Horizontal);
    duration_slider->setEnabled(false);

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
    auto make_btn = [=, this](auto icon, auto f) {
        auto *b = new QToolButton(this);
        b->setIcon(style()->standardIcon(icon));
        b->setEnabled(false);
        connect(b, &QAbstractButton::clicked, this, f);
        return b;
    };

    play_btn = new PlayButton;
    play_btn->setEnabled(false);
    connect(play_btn, &PlayButton::play,  this, [=, this]() { player->start_or_resume(); });
    connect(play_btn, &PlayButton::pause, this, [=, this]() { player->pause(); });

    player->on_track_ended([=, this]() {
        play_btn->set_state(PlayButton::State::Pause);
    });

    prev_track = make_btn(QStyle::SP_MediaSkipBackward, [=, this]() { player->prev(); });
    next_track = make_btn(QStyle::SP_MediaSkipForward,  [=, this]() { player->next(); });
    stop_btn   = make_btn(QStyle::SP_MediaStop,         &MainWindow::stop);

    tempo = new QComboBox;
    tempo->addItem("2x",     2.0);
    tempo->addItem("Normal", 1.0);
    tempo->addItem("0.5x",   0.5);
    tempo->setCurrentIndex(options.tempo == 2.0 ? 2
                         : options.tempo == 1.0 ? 1
                         : options.tempo == 0.5 ? 0
                         : 1);
    tempo->setEnabled(false);
    connect(tempo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=, this] (int i) {
        player->set_tempo(tempo->currentData().toDouble());
    });

    volume = new QSlider(Qt::Horizontal);
    volume->setEnabled(false);
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

    // track and file playlist
    playlist = new QListWidget;
    connect(playlist, &QListWidget::itemActivated, this, [&](QListWidgetItem *) {
        player->load_track(playlist->currentRow());
    });

    file_playlist = new QListWidget;
    connect(file_playlist, &QListWidget::itemActivated, this, [&](QListWidgetItem *) {
        int fileno = file_playlist->currentRow();
        player->load_file(fileno);
        player->load_track(0);
        int trackno = player->load_track(0);
        playlist->clear();
        player->track_names([&](const std::string &name) {
            new QListWidgetItem(QString::fromStdString(name), playlist);
        });
        playlist->setCurrentRow(trackno);
    });
    player->on_file_changed([&](int fileno) {
        playlist->clear();
        player->track_names([&](const std::string &name) {
            new QListWidgetItem(QString::fromStdString(name), playlist);
        });
        file_playlist->setCurrentRow(fileno);
    });

    // playlist settings
#define PLAYER_SETTING(var, name, opt) \
    do {                                     \
        var = new QCheckBox(name);           \
        var->setChecked(options.opt);        \
        var->setEnabled(false);              \
    } while (0)

    PLAYER_SETTING(autoplay,       "Autoplay",          autoplay);
    PLAYER_SETTING(shuffle_tracks, "Shuffle tracks",    track_shuffle);
    PLAYER_SETTING(shuffle_files,  "Shuffle files",     file_shuffle);
    PLAYER_SETTING(repeat_track,   "Repeat track",      track_repeat);
    PLAYER_SETTING(repeat_file,    "Repeat file",       file_repeat);

#undef PLAYER_SETTING

    connect(autoplay,       &QCheckBox::stateChanged, this, [=, this] (int state) { player->set_autoplay(state); });
    connect(shuffle_tracks, &QCheckBox::stateChanged, this, [=, this] (int state) { player->set_track_shuffle(state); });
    connect(shuffle_files,  &QCheckBox::stateChanged, this, [=, this] (int state) { player->set_file_shuffle(state); });
    connect(repeat_track,   &QCheckBox::stateChanged, this, [=, this] (int state) {
        player->set_track_repeat(state);
        next_track->setEnabled(bool(player->get_next()));
        prev_track->setEnabled(player->get_prev_track() || bool(player->get_prev_file()));
    });
    connect(repeat_file,    &QCheckBox::stateChanged, this, [=, this] (int state) {
        player->set_file_repeat(state);
        next_track->setEnabled(bool(player->get_next()));
        prev_track->setEnabled(player->get_prev_track() || bool(player->get_prev_file()));
    });

    auto *box = make_groupbox<QGridLayout>("Playlist settings",
        std::make_tuple(autoplay,       0, 0),
        std::make_tuple(repeat_track,   1, 0),
        std::make_tuple(repeat_file,    1, 1),
        std::make_tuple(shuffle_tracks, 2, 0),
        std::make_tuple(shuffle_files,  2, 1)
    );

    // track information
    auto *title   = new QLabel;
    auto *game    = new QLabel;
    auto *system  = new QLabel;
    auto *author  = new QLabel;
    auto *comment = new QLabel;

    player->on_track_changed([=, this](int num, gme_info_t *info, int length) {
        title   ->setText(info->song);
        game    ->setText(info->game);
        author  ->setText(info->author);
        system  ->setText(info->system);
        comment ->setText(info->comment);
        duration_slider->setRange(0, player->effective_length());
        next_track->setEnabled(bool(player->get_next()));
        prev_track->setEnabled(player->get_prev_track() || bool(player->get_prev_file()));
        playlist->setCurrentRow(num);
        start_or_resume();
    });

    // load shortcuts only after everything has been constructed
    load_shortcuts();

    // and now create the gui
    center->setLayout(
        make_layout<QVBoxLayout>(
            make_layout<QHBoxLayout>(
                make_groupbox<QFormLayout>("Track info",
                    std::make_tuple(new QLabel(tr("Title:")),   title),
                    std::make_tuple(new QLabel(tr("Game:")),    game),
                    std::make_tuple(new QLabel(tr("System:")),  system),
                    std::make_tuple(new QLabel(tr("Author:")),  author),
                    std::make_tuple(new QLabel(tr("Comment:")), comment)
                ),
                make_layout<QVBoxLayout>(
                    make_layout<QHBoxLayout>(
                        make_layout<QVBoxLayout>(new QLabel("Track playlist"), playlist),
                        make_layout<QVBoxLayout>(new QLabel("File playlist"), file_playlist)
                    ),
                    box
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
    settings.beginGroup("shortcuts");

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

    add_shortcut("play", "Play/Pause", "Ctrl+Space", [=, this]() {
        player->is_playing() ? pause() : start_or_resume();
    });
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
    // load first file in file playlist
    int fileno = player->load_file(0);
    file_playlist->clear();
    player->file_names([&](const std::string &s) {
        new QListWidgetItem(QString::fromStdString(s), file_playlist);
    });
    file_playlist->setCurrentRow(fileno);
    // load first track in the file's track playlist
    int trackno = player->load_track(0);
    playlist->clear();
    player->track_names([&](const std::string &name) {
        new QListWidgetItem(QString::fromStdString(name), playlist);
    });
    playlist->setCurrentRow(trackno);
    // enable everything else
    play_btn->set_state(PlayButton::State::Play);
    duration_slider->setEnabled(true);
    stop_btn->setEnabled(true);
    next_track->setEnabled(bool(player->get_next()));
    prev_track->setEnabled(player->get_prev_track() || bool(player->get_prev_file()));
    volume_btn->setEnabled(true);
    play_btn->setEnabled(true);
    volume->setEnabled(true);
    tempo->setEnabled(true);
    autoplay->setEnabled(true);
    repeat_track->setEnabled(true);
    repeat_file->setEnabled(true);
    shuffle_tracks->setEnabled(true);
    shuffle_files->setEnabled(true);
}

void MainWindow::start_or_resume()
{
    if (player->can_play()) {
        player->start_or_resume();
        play_btn->set_state(PlayButton::State::Play);
    }
}

void MainWindow::pause()
{
    if (player->can_play()) {
        player->pause();
        play_btn->set_state(PlayButton::State::Pause);
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    save_player_settings(player->get_options());
    save_recent(recent_files->filenames(), recent_playlists->filenames());
    save_shortcuts(shortcuts);
    event->accept();
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
    connect(button_box, &QDialogButtonBox::accepted, this, [=, this]() {
        accept();
    });
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
