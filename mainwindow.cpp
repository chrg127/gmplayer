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
#include "qtutils.hpp"

MediaControls::MediaControls(QWidget *parent)
    : QWidget(parent)
{
    auto make_btn = [&](auto icon, auto f) {
        auto *b = new QToolButton(this);
        b->setIcon(style()->standardIcon(icon));
        connect(b, &QAbstractButton::clicked, this, f);
        return b;
    };
    auto *volume = new QSlider(Qt::Horizontal);
    volume->setRange(0, 100);
    connect(volume, &QSlider::valueChanged, this, [&]() {
        emit change_volume(0);
    });
    setLayout(make_layout<QHBoxLayout>(
        make_btn(QStyle::SP_MediaSkipBackward, []() { player::prev(); }),
        make_btn(QStyle::SP_MediaPlay,         []() { player::start_or_resume(); }),
        make_btn(QStyle::SP_MediaStop,         []() { player::stop(); }),
        make_btn(QStyle::SP_MediaSkipForward,  []() { player::next(); }),
        make_btn(QStyle::SP_MediaVolume,       []() { /* mute */ }),
        volume
    ));
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

    title     = new QLabel;
    game      = new QLabel;
    system    = new QLabel;
    author    = new QLabel;
    comment   = new QLabel;

    auto *labels = make_grid_layout(
        std::tuple { new QLabel(tr("Title:")),   0, 0 },
        std::tuple { new QLabel(tr("Game:")),    1, 0 },
        std::tuple { new QLabel(tr("System:")),  2, 0 },
        std::tuple { new QLabel(tr("Author:")),  3, 0 },
        std::tuple { new QLabel(tr("Comment:")), 4, 0 },
        std::tuple { title,                      0, 1 },
        std::tuple { game,                       0, 2 },
        std::tuple { system,                     0, 3 },
        std::tuple { author,                     0, 4 },
        std::tuple { comment,                    0, 5 }
    );

    auto *duration_slider = new QSlider(Qt::Horizontal);
    // change its value with setValue
    connect(duration_slider, &QSlider::sliderMoved, this, [&](int secs) {
        fmt::print("{}\n", secs);
    });
    duration_label = new QLabel("00:00 / 00:00");

    auto *lt = make_layout<QVBoxLayout>(
        labels,
        make_layout<QHBoxLayout>(
            duration_slider,
            duration_label
        ),
        new MediaControls,
        new QWidget
    );
    center->setLayout(lt);
}

QMenu *MainWindow::create_menu(const char *name, auto... actions)
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
    last_dir = filename;
}

void MainWindow::edit_settings()
{

}
