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
    connect(play_btn, &PlayButton::play,  this, [=, this]() { player->start_or_resume(); });
    connect(play_btn, &PlayButton::pause, this, [=, this]() { player->pause(); });

    auto *duration_label = new QLabel("00:00 / 00:00");
    duration_slider = new QSlider(Qt::Horizontal);

    auto set_duration_label = [=, this](int ms, int max) {
        int mins = ms / 1000 / 60;
        int secs = ms / 1000 % 60;
        int max_mins = max / 1000 / 60;
        int max_secs = max / 1000 % 60;
        auto str = fmt::format("{:02}:{:02}/{:02}:{:02}", mins, secs, max_mins, max_secs);
        duration_label->setText(QString::fromStdString(str));
    };

    connect(duration_slider, &QSlider::sliderPressed,  this, [=, this]() {
        player->pause();
        play_btn->set_state(PlayButton::State::Pause);
    });
    connect(duration_slider, &QSlider::sliderReleased, this, [=, this]() {
        player->start_or_resume();
        play_btn->set_state(PlayButton::State::Play);
    });
    connect(duration_slider, &QSlider::sliderMoved, this, [=, this](int ms) {
        player->seek(ms);
        set_duration_label(ms, duration_slider->maximum());
    });

    auto *title   = new QLabel;
    auto *game    = new QLabel;
    auto *system  = new QLabel;
    auto *author  = new QLabel;
    auto *comment = new QLabel;

    auto make_btn = [=, this](auto icon, auto f) {
        auto *b = new QToolButton(this);
        b->setIcon(style()->standardIcon(icon));
        connect(b, &QAbstractButton::clicked, this, f);
        return b;
    };

    prev_track = make_btn(QStyle::SP_MediaSkipBackward, [=, this]() { player->prev(); });
    next_track = make_btn(QStyle::SP_MediaSkipForward,  [=, this]() { player->next(); });
    stop = make_btn(QStyle::SP_MediaStop,               [=, this]() {
        play_btn->set_state(PlayButton::State::Pause);
        player->pause();
        player->seek(0);
        duration_slider->setValue(0);
    });

    volume = new QSlider(Qt::Horizontal);
    volume->setRange(0, get_max_volume_value());
    volume->setValue(volume->maximum());
    volume_btn = make_btn(QStyle::SP_MediaVolume,
        [=, this, last_volume = get_max_volume_value()] () mutable {
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

    playlist = new QListWidget(this);
    connect(playlist, &QListWidget::itemActivated, this, [&](QListWidgetItem *item) {
        int index = playlist->currentRow(); //playlist->selectionModel()->selectedIndexes()[0].row();
        player->load_track(index);
        player->start_or_resume();
        play_btn->set_state(PlayButton::State::Play);
    });

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
        duration_slider->setRange(0, length);
        next_track->setEnabled(player->has_next());
        prev_track->setEnabled(player->has_prev());
        player->start_or_resume();
        play_btn->set_state(PlayButton::State::Play);
        playlist->setCurrentRow(num);
    });
    player->on_track_ended([=, this]() {
        play_btn->set_state(PlayButton::State::Pause);
    });

    set_enabled(false);
    center->setLayout(
        make_layout<QHBoxLayout>(
            make_layout<QVBoxLayout>(
                make_groupbox("Track info", [&]() {
                    return make_form_layout(
                        std::tuple { new QLabel(tr("Title:")),   title      },
                        std::tuple { new QLabel(tr("Game:")),    game       },
                        std::tuple { new QLabel(tr("System:")),  system     },
                        std::tuple { new QLabel(tr("Author:")),  author     },
                        std::tuple { new QLabel(tr("Comment:")), comment    }
                    );
                }),
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
            ),
            make_layout<QVBoxLayout>(
                new QLabel(tr("Track playlist:")),
                playlist
            )
        )
    );
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

void MainWindow::open_file()
{
    // auto filename = QString("smb3.nsf");
    auto filename = QFileDialog::getOpenFileName(
        this,
        tr("Open file"),
        last_dir,
        "Game music files (*.spc *.nsf)"
    );
    if (filename.isEmpty())
        return;
    set_enabled(true);
    player->pause();
    player->use_file(filename.toUtf8().constData());
    player->load_track(0);
    player->start_or_resume();
    play_btn->set_state(PlayButton::State::Play);
    playlist->clear();
    for (auto &track : player->track_names())
        new QListWidgetItem(QString::fromStdString(track), playlist);
    playlist->setCurrentRow(0);
    last_dir = filename;
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

void MainWindow::edit_settings()
{
    auto *wnd = new SettingsWindow(player->get_options(), this);
    wnd->open();
    connect(wnd, &QDialog::finished, this, [=, this](int result) {
        if (result == QDialog::Accepted) {
            fmt::print("modifying options\n");
            auto opts = wnd->get();
            fmt::print("fade: {}\n", opts.fade_out);
            fmt::print("fade secs: {}\n", opts.fade_out_ms);
            fmt::print("autoplay next: {}\n", opts.autoplay_next);
            player->set_options(wnd->get());
        }
    });
}



SettingsWindow::SettingsWindow(const PlayerOptions &options, QWidget *parent)
    : QDialog(parent), selected_options{options}
{
    fade = new QCheckBox(tr("&Enable fade-out"));
    fade->setChecked(options.fade_out);

    fade_secs = new QSpinBox;
    fade_secs->setValue(options.fade_out_ms);

    autoplay = new QCheckBox(tr("Autoplay next track"));
    autoplay->setChecked(options.autoplay_next);

    auto *button_box = new QDialogButtonBox(QDialogButtonBox::Ok
                                          | QDialogButtonBox::Cancel);
    connect(button_box, &QDialogButtonBox::accepted, this, [&]() {
        // make an option object that the main window will get with get()
        selected_options = (PlayerOptions) {
            .fade_out           = fade->isChecked(),
            .fade_out_ms        = fade_secs->value() * 1000,
            .autoplay_next      = autoplay->isChecked(),
            .default_duration   = 3_min,
            .silence_detection  = 0,
            .loops_limit        = 0,
            .gain               = 0,
        };
        accept();
    });
    connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *lt = make_layout<QVBoxLayout>(
        fade,
        make_form_layout(std::tuple { new QLabel(tr("Fade seconds:")), fade_secs }),
        autoplay,
        button_box
    );
    setLayout(lt);
}
