#include "mainwindow.hpp"

#include <functional>
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
#include <fmt/core.h>

template <typename T> void add_to_layout(T *lt, QWidget *w) { lt->addWidget(w); }
template <typename T> void add_to_layout(T *lt, QLayout *l) { lt->addLayout(l); }

template <typename T>
T *make_layout(auto... widgets)
{
    auto *lt = new T;
    (add_to_layout(lt, widgets), ...);
    return lt;
}

inline QGridLayout *make_grid_layout(auto... widget_tuples)
{
    auto *lt = new QGridLayout;
    (lt->addWidget(std::get<0>(widget_tuples),
                   std::get<1>(widget_tuples),
                   std::get<2>(widget_tuples)), ...);
    return lt;
}

MediaControls::MediaControls(QWidget *parent)
    : QWidget(parent)
{
    auto make_btn = [&](auto icon, auto f) {
        auto *b = new QToolButton(this);
        b->setIcon(style()->standardIcon(icon));
        connect(b, &QAbstractButton::clicked, this, f);
        return b;
    };
    auto *slider = new QSlider(Qt::Horizontal, this);
    slider->setRange(0, 100);
    connect(slider, &QSlider::valueChanged, this, [&]() {
        emit change_volume(0);
    });
    setLayout(make_layout<QHBoxLayout>(
        make_btn(QStyle::SP_MediaSkipBackward, &MediaControls::prev),
        make_btn(QStyle::SP_MediaPlay,         &MediaControls::play),
        make_btn(QStyle::SP_MediaSkipForward,  &MediaControls::next),
        make_btn(QStyle::SP_MediaStop,         &MediaControls::stop),
        slider
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
        std::tuple { new QLabel("Title:"),   0, 0 },
        std::tuple { new QLabel("Game:"),    1, 0 },
        std::tuple { new QLabel("System:"),  2, 0 },
        std::tuple { new QLabel("Author:"),  3, 0 },
        std::tuple { new QLabel("Comment:"), 4, 0 },
        std::tuple { title,                  0, 1 },
        std::tuple { game ,                  0, 2 },
        std::tuple { system,                 0, 3 },
        std::tuple { author,                 0, 4 },
        std::tuple { comment,                0, 5 }
    );

    auto *lt = make_layout<QVBoxLayout>(labels, new MediaControls, new QWidget);
    center->setLayout(lt);
}

QMenu *MainWindow::create_menu(const char *name, auto... actions)
{
    auto *menu = menuBar()->addMenu(tr(name));
    auto f = [&](auto &a) {
        auto *act = new QAction(std::get<0>(a), this);
        connect(act, &QAction::triggered, this, std::get<1>(a));
        menu->addAction(act);
    };
    (f(actions), ...);
    return menu;
}

void MainWindow::open_file()
{

}

void MainWindow::edit_settings()
{

}
