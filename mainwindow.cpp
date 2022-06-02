#include "mainwindow.hpp"

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
    });
}

void PlayButton::set_state(State state)
{
    switch (this->state = state) {
    case State::Play:
        setIcon(style()->standardIcon(QStyle::SP_MediaPause));
        emit play();
        break;
    case State::Pause:
        setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        emit pause();
        break;
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("gmplayer");
    auto *center = new QWidget(this);
    setCentralWidget(center);

    create_menu("&File",
        std::tuple { "Open", &MainWindow::open_file }
    );

    create_menu("&Edit",
        std::tuple { "Settings", &MainWindow::edit_settings }
    );

    title     = new QLabel("title");
    game      = new QLabel("game");
    system    = new QLabel("system");
    author    = new QLabel("author");
    comment   = new QLabel("comment");

    duration_label = new QLabel("00:00 / 00:00");
    auto *duration_slider = new QSlider(Qt::Horizontal);
    // change its value with setValue
    connect(duration_slider, &QSlider::sliderMoved, this, [&](int secs) {
        fmt::print("{}\n", secs);
    });

    auto *volume = new QSlider(Qt::Horizontal);
    volume->setRange(0, 100);
    connect(volume, &QSlider::valueChanged, this, [&]() {
        fmt::print("changing volume\n");
    });

    play_btn = new PlayButton;
    connect(play_btn, &PlayButton::play,  this, []() { fmt::print("play\n"); player::start_or_resume(); });
    connect(play_btn, &PlayButton::pause, this, []() { fmt::print("pause\n"); player::pause(); });

    auto make_btn = [&](auto icon, auto f) {
        auto *b = new QToolButton(this);
        b->setIcon(style()->standardIcon(icon));
        connect(b, &QAbstractButton::clicked, this, f);
        return b;
    };

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
            make_btn(QStyle::SP_MediaSkipBackward, []() { player::prev(); }),
            play_btn,
            make_btn(QStyle::SP_MediaStop,         []() { player::stop(); }),
            make_btn(QStyle::SP_MediaSkipForward,  []() { player::next(); }),
            make_btn(QStyle::SP_MediaVolume,       []() { /* mute */ }),
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
    auto filename = QFileDialog::getOpenFileName(this, tr("Open file"), last_dir,
        "Game music files (*.spc *.nsf)");
    if (filename.isEmpty())
        return;
    player::use_file(filename);
    auto metadata = player::get_track_metadata();
    title->setText(metadata.info->song);
    game->setText(metadata.info->game);
    author->setText(metadata.info->author);
    system->setText(metadata.info->system);
    comment->setText(metadata.info->comment);
    play_btn->set_state(PlayButton::State::Play); // also calls start_or_resume
    last_dir = filename;
}

void MainWindow::edit_settings()
{
}
