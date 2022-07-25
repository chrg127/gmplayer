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
#include <fmt/core.h>
#include <gme/gme.h>    // gme_info_t
#include "qtutils.hpp"
#include "player.hpp"

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

PlayerOptions load_player_settings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "gmplayer", "gmplayer");
    settings.beginGroup("player");
    PlayerOptions options = {
        .fade_out_ms        = settings.value("fade_out_ms",                            0).toInt(),
        .autoplay_next      = settings.value("autoplay_next",                      false).toBool(),
        .repeat             = settings.value("repeat",                             false).toBool(),
        .shuffle            = settings.value("shuffle",                            false).toBool(),
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
    settings.setValue("autoplay_next",     options.autoplay_next);
    settings.setValue("repeat",            options.repeat);
    settings.setValue("shuffle",           options.shuffle);
    settings.setValue("default_duration",  options.default_duration);
    settings.setValue("silence_detection", options.silence_detection);
    settings.setValue("tempo",             options.tempo);
    settings.setValue("volume",            options.volume);
    settings.endGroup();
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

    create_menu("&File",
        std::tuple { "Open file", [=, this] () {
            auto filename = QFileDialog::getOpenFileName(
                this,
                tr("Open file"),
                last_dir,
                "Game music files (*.spc *.nsf)"
            );
            if (!filename.isEmpty())
                open_file(filename);
        } },
        std::tuple { "Open playlist", [](){} },
        std::tuple { "Open recent",   [](){} }
    );

    create_menu("&Edit",
        std::tuple { "Settings",  &MainWindow::edit_settings  },
        std::tuple { "Shortcuts", &MainWindow::edit_shortcuts }
    );

    create_menu("&About"
    );

    // duration slider
    duration_label = new QLabel("00:00 / 00:00");
    duration_slider = new QSlider(Qt::Horizontal);

    connect(duration_slider, &QSlider::sliderPressed,  this, [=, this]() {
        was_paused = player->is_paused();
        pause();
    });
    connect(duration_slider, &QSlider::sliderReleased, this, [=, this]() {
        if (!was_paused) {
            start_or_resume();
        }
    });
    connect(duration_slider, &QSlider::sliderMoved, this, [=, this](int ms) {
        player->seek(ms);
        set_duration_label(ms, duration_slider->maximum());
    });

    // track information
    auto *title   = new QLabel;
    auto *game    = new QLabel;
    auto *system  = new QLabel;
    auto *author  = new QLabel;
    auto *comment = new QLabel;

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

    // track playlist
    playlist = new QListWidget;
    connect(playlist, &QListWidget::itemActivated, this, [&](QListWidgetItem *item) {
        int index = playlist->currentRow();
        player->load_track(index);
        start_or_resume();
    });

    // track playlist settings
    autoplay = new QCheckBox(tr("Autoplay next track"));
    repeat   = new QCheckBox(tr("Repeat"));
    shuffle  = new QCheckBox(tr("Shuffle"));
    autoplay->setChecked(options.autoplay_next);
    autoplay->setEnabled(false);
    repeat->setChecked(options.repeat);
    repeat->setEnabled(false);
    shuffle->setChecked(options.shuffle);
    shuffle->setEnabled(false);

    connect(autoplay, &QCheckBox::stateChanged, this, [=, this](int state) { player->set_autoplay(state); });
    connect(repeat,   &QCheckBox::stateChanged, this, [=, this](int state) {
        player->set_repeat(state);
        next_track->setEnabled(bool(player->get_next()));
        prev_track->setEnabled(bool(player->get_prev()));
    });
    connect(shuffle,  &QCheckBox::stateChanged, this, [=, this](int state) { player->set_shuffle(state);  });

    // player stuff
    player->on_position_changed([=, this](int ms) {
        duration_slider->setValue(ms);
        set_duration_label(ms, duration_slider->maximum());
    });

    player->on_track_changed([=, this](int num, gme_info_t *info, int length) {
        title   ->setText(info->song);
        game    ->setText(info->game);
        author  ->setText(info->author);
        system  ->setText(info->system);
        comment ->setText(info->comment);
        duration_slider->setRange(0, player->effective_length());
        next_track->setEnabled(bool(player->get_next()));
        prev_track->setEnabled(bool(player->get_prev()));
        playlist->setCurrentRow(num);
        start_or_resume();
    });

    player->on_track_ended([=, this]() {
        play_btn->set_state(PlayButton::State::Pause);
    });

    load_shortcuts();


    // and now create the gui
    center->setLayout(
        make_layout<QVBoxLayout>(
            make_layout<QHBoxLayout>(
                make_groupbox<QFormLayout>("Track info",
                    std::tuple { new QLabel(tr("Title:")),   title      },
                    std::tuple { new QLabel(tr("Game:")),    game       },
                    std::tuple { new QLabel(tr("System:")),  system     },
                    std::tuple { new QLabel(tr("Author:")),  author     },
                    std::tuple { new QLabel(tr("Comment:")), comment    }
                ),
                make_layout<QVBoxLayout>(
                    new QLabel(tr("Track playlist:")),
                    playlist,
                    make_groupbox<QVBoxLayout>("Playlist settings",
                        autoplay,
                        repeat,
                        shuffle
                    )
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
        if (player->is_paused()) {
            start_or_resume();
        } else
            pause();
    });
    add_shortcut("next",  "Next",           "Ctrl+Right",   [=, this] { player->next();    });
    add_shortcut("prev",  "Previous",       "Ctrl+Left",    [=, this] { player->prev();    });
    add_shortcut("stop",  "Stop",           "Ctrl+S",       &MainWindow::stop);
    add_shortcut("seekf", "Seek forward",   "Right",        [=, this] { player->seek(player->tell() + 1_sec); });
    add_shortcut("seekb", "Seek backwards", "Left",         [=, this] { player->seek(player->tell() - 1_sec); });
    add_shortcut("volup", "Volume up",      "0",            [=, this] { volume->setValue(volume->value() + 2); });
    add_shortcut("voldw", "Volume down",    "9",            [=, this] { volume->setValue(volume->value() - 2); });
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

QMenu *MainWindow::create_menu(const char *name, auto&&... actions)
{
    auto *menu = menuBar()->addMenu(tr(name));
    auto f = [=, this](auto &a) {
        auto *act = new QAction(tr(std::get<0>(a)), this);
        connect(act, &QAction::triggered, this, std::get<1>(a));
        menu->addAction(act);
    };
    (f(actions), ...);
    return menu;
}

void MainWindow::open_file(QString filename)
{
    // filename = QString("test_files/rudra.spc");
    auto err = player->use_file(filename.toUtf8().constData());
    if (err) {
        msgbox(QString("The file %1 couldn't be opened. Error: %2")
                       .arg(filename)
                       .arg(err));
        return;
    }
    player->load_track(0);
    play_btn->set_state(PlayButton::State::Play);
    duration_slider->setEnabled(true);
    stop_btn->setEnabled(true);
    prev_track->setEnabled(bool(player->get_next()));
    next_track->setEnabled(bool(player->get_next()));
    volume_btn->setEnabled(true);
    play_btn->setEnabled(true);
    autoplay->setEnabled(true);
    repeat->setEnabled(true);
    shuffle->setEnabled(true);
    volume->setEnabled(true);
    tempo->setEnabled(true);
    playlist->clear();
    for (auto &track : player->track_names())
        new QListWidgetItem(QString::fromStdString(track), playlist);
    playlist->setCurrentRow(player->get_index(0));
    last_dir = filename;
}

void MainWindow::start_or_resume()
{
    if (player->loaded()) {
        player->start_or_resume();
        play_btn->set_state(PlayButton::State::Play);
    }
}

void MainWindow::pause()
{
    if (player->loaded()) {
        player->pause();
        play_btn->set_state(PlayButton::State::Pause);
    }
}

void MainWindow::stop()
{
    if (player->loaded()) {
        // load track also calls on_track_changed() callback, which causes a
        // call to start_or_resume, so put pause after load.
        player->load_track(0);
        pause();
        duration_slider->setValue(0);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    save_player_settings(player->get_options());
    save_shortcuts(shortcuts);
    event->accept();
}

void MainWindow::set_duration_label(int ms, int max)
{
    int mins = ms / 1000 / 60;
    int secs = ms / 1000 % 60;
    int max_mins = max / 1000 / 60;
    int max_secs = max / 1000 % 60;
    auto str = fmt::format("{:02}:{:02}/{:02}:{:02}", mins, secs, max_mins, max_secs);
    duration_label->setText(QString::fromStdString(str));
};

void MainWindow::edit_settings()
{
    auto *wnd = new SettingsWindow(player, this);
    wnd->open();
    connect(wnd, &QDialog::finished, this, [=, this](int result) {
        if (result == QDialog::Accepted && player->loaded()) {
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
    open_file(url.toLocalFile());
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
