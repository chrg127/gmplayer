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

VolumeButton::VolumeButton(QSlider *volume_slider, QWidget *parent)
    : QToolButton(parent)
    , last_volume{get_max_volume_value()}
    , slider{volume_slider}
{
    setIcon(style()->standardIcon(QStyle::SP_MediaVolume));
    connect(this, &QAbstractButton::clicked, this, [&]() {
        int volume = slider->value();
        if (volume != 0) {
            // mute
            last_volume = volume;
            slider->setValue(0);
            setIcon(style()->standardIcon(QStyle::SP_MediaVolumeMuted));
        } else {
            // unmute
            slider->setValue(last_volume);
            setIcon(style()->standardIcon(QStyle::SP_MediaVolume));
        }
    });
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("gmplayer");
    auto *center = new QWidget(this);
    setCentralWidget(center);

    player = new Player;

    create_menu("&File",
        std::tuple { "Open", &MainWindow::open_file }
    );

    create_menu("&Edit",
        std::tuple { "Settings", &MainWindow::edit_settings }
    );

    play_btn = new PlayButton;
    connect(play_btn, &PlayButton::play,  this, [&]() { player->start_or_resume(); });
    connect(play_btn, &PlayButton::pause, this, [&]() { player->pause(); });

    duration_label = new QLabel("00:00 / 00:00");
    duration_slider = new QSlider(Qt::Horizontal);
    connect(duration_slider, &QSlider::sliderPressed,  this, [&]() {
        player->pause();
        play_btn->set_state(PlayButton::State::Pause);
    });
    connect(duration_slider, &QSlider::sliderReleased, this, [&]() {
        player->start_or_resume();
        play_btn->set_state(PlayButton::State::Play);
    });
    connect(duration_slider, &QSlider::sliderMoved, this, [&](int ms) {
        player->seek(ms);
        set_duration_label(ms, duration_slider->maximum());
    });

    title   = new QLabel;
    game    = new QLabel;
    system  = new QLabel;
    author  = new QLabel;
    comment = new QLabel;

    auto make_btn = [&](auto icon, auto f) {
        auto *b = new QToolButton(this);
        b->setIcon(style()->standardIcon(icon));
        connect(b, &QAbstractButton::clicked, this, f);
        return b;
    };

    prev_track = make_btn(QStyle::SP_MediaSkipBackward, [&]() { player->prev(); });
    next_track = make_btn(QStyle::SP_MediaSkipForward,  [&]() { player->next(); });
    stop = make_btn(QStyle::SP_MediaStop,               [&]() {
        play_btn->set_state(PlayButton::State::Pause);
        player->pause();
        player->seek(0);
        duration_slider->setValue(0);
    });

    volume = new QSlider(Qt::Horizontal);
    volume->setRange(0, get_max_volume_value());
    volume->setValue(volume->maximum());
    volume_btn = new VolumeButton(volume, this);
    connect(volume, &QSlider::valueChanged, this, [&]() {
        player->set_volume(volume->value());
        volume_btn->setIcon(style()->standardIcon(
            volume->value() == 0 ? QStyle::SP_MediaVolumeMuted
                                 : QStyle::SP_MediaVolume
        ));
    });

    player->on_position_changed([&](int ms) {
        duration_slider->setValue(ms);
        set_duration_label(ms, duration_slider->maximum());
    });
    player->on_track_changed([&](gme_info_t *info, int length) {
        title   ->setText(info->song);
        game    ->setText(info->game);
        author  ->setText(info->author);
        system  ->setText(info->system);
        comment ->setText(info->comment);
        duration_slider->setRange(0, length);
    });
    player->on_track_ended([&]() {
        play_btn->set_state(PlayButton::State::Pause);
    });

    set_enabled(false);
    center->setLayout(make_layout<QVBoxLayout>(
        make_grid_layout(
            std::tuple { new QLabel(tr("Title:")),   0, 0 },
            std::tuple { title,                      0, 1 },
            std::tuple { new QLabel(tr("Game:")),    1, 0 },
            std::tuple { game,                       1, 1 },
            std::tuple { new QLabel(tr("System:")),  0, 2 },
            std::tuple { system,                     0, 3 },
            std::tuple { new QLabel(tr("Author:")),  1, 2 },
            std::tuple { author,                     1, 3 },
            std::tuple { new QLabel(tr("Comment:")), 2, 0 },
            std::tuple { comment,                    2, 1 }
        ),
        make_layout<QHBoxLayout>(
            duration_slider,
            duration_label
        ),
        make_layout<QHBoxLayout>(
            prev_track,
            play_btn,
            next_track,
            stop,
            volume_btn,
            volume
        ),
        new QWidget
    ));
}

QMenu *MainWindow::create_menu(const char *name, auto&&... actions)
{
    auto *menu = menuBar()->addMenu(tr(name));
    auto f = [&](auto &a) {
        auto *act = new QAction(tr(std::get<0>(a)), this);
        connect(act, &QAction::triggered, this, std::get<1>(a));
        menu->addAction(act);
    };
    (f(actions), ...);
    return menu;
}

void MainWindow::open_file()
{
    // auto filename = QString("skyfortress.spc");
    auto filename = QFileDialog::getOpenFileName(this, tr("Open file"), last_dir,
        "Game music files (*.spc *.nsf)");
    if (filename.isEmpty())
        return;
    player->pause();
    player->use_file(filename.toUtf8().constData());
    player->start_or_resume();
    play_btn->set_state(PlayButton::State::Play);
    last_dir = filename;
    set_enabled(true);
}

void MainWindow::edit_settings()
{
}

void MainWindow::set_enabled(bool val)
{
    duration_slider->setEnabled(val);
    volume->setEnabled(val);
    stop->setEnabled(val);
    prev_track->setEnabled(val);
    next_track->setEnabled(val);
    volume_btn->setEnabled(val);
    play_btn->setEnabled(val);
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
