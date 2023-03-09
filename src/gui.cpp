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
#include <QString>
#include <QStringLiteral>
#include <QShortcut>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include "qtutils.hpp"
#include "player.hpp"
#include "io.hpp"
#include "appinfo.hpp"
#include "math.hpp"

namespace fs = std::filesystem;
using namespace gmplayer::literals;

namespace gui {

namespace {

constexpr auto MUSIC_FILE_FILTER =
    "All supported formats (*.spc *.nsf *.nsfe *.gbs *.gym *.ay *.kss *.hes *.vgm *.sap);;"
    "All files (*.*)"
    "SPC - SNES SPC700 Files (*.spc);;"
    "NSF - Nintendo Sound Format (*.nsf);;"
    "NSFE - Nintendo Sound Format Extended (*.nsfe);;"
    "GBS - Game Boy Sound System (*.gbs);;"
    "GYM - Genesis YM2612 Files (*.gym);;"
    "AY - AY-3-8910 (*.ay);;"
    "KSS - Konami Sound System (*.kss);;"
    "HES - NEC Home Entertainment System (*.hes);;"
    "VGM - Video Game Music (*.vgm);;"
    "SAP - Slight Atari Player (*.sap);;";

constexpr auto PLAYLIST_FILTER =
    "Playlist files (*.playlist);;"
    "Text files (*.txt);;"
    "All files (*.*)";

QString format_duration(int ms, int max)
{
    return QString("%1:%2/%3:%4")
        .arg(ms  / 1000 / 60, 2, 10, QChar('0'))
        .arg(ms  / 1000 % 60, 2, 10, QChar('0'))
        .arg(max / 1000 / 60, 2, 10, QChar('0'))
        .arg(max / 1000 % 60, 2, 10, QChar('0'));
};

std::vector<fs::path> load_recent(const QString &name)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "gmplayer", "gmplayer");
    settings.beginGroup("recent");
    auto files = settings.value(name).toStringList();
    std::vector<fs::path> paths;
    for (auto &file : files)
        paths.push_back(fs::path(file.toStdString()));
    settings.endGroup();
    return paths;
}

void save_recent(std::span<fs::path> paths, const QString &name)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "gmplayer", "gmplayer");
    settings.beginGroup("recent");
    QStringList list;
    for (auto &p : paths)
        list.append(QString::fromStdString(p.string()));
    settings.setValue(name, list);
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

int tempo_to_int(double value) { return math::map(std::log2(value), -2.0, 2.0, 0.0, 100.0); }
double int_to_tempo(int value) { return std::exp2(math::map(double(value), 0.0, 100.0, -2.0, 2.0)); }

} // namespace



RecentList::RecentList(QMenu *menu, std::vector<std::filesystem::path> &&ps)
    : menu{menu}, paths{std::move(ps)}
{
    regen();
}

void RecentList::regen()
{
    menu->clear();
    for (auto &p : paths) {
        auto filename = p.filename();
        auto *act     = new QAction(QString::fromStdString(filename), this);
        connect(act, &QAction::triggered, this, [=, this] { emit clicked(p); });
        menu->addAction(act);
    }
}

void RecentList::add(fs::path path)
{
    auto it = std::remove(paths.begin(), paths.end(), path);
    if (it != paths.end())
        paths.erase(it);
    paths.insert(paths.begin(), path);
    if (paths.size() > 10)
        paths.erase(paths.begin() + 10, paths.end());
    regen();
}



SettingsWindow::SettingsWindow(gmplayer::Player *player, QWidget *parent)
{
    auto options = player->options();

    auto *fade              = make_checkbox(tr("&Enable fade-out"), options.fade_out != 0);
    auto *fade_secs         = make_spinbox(std::numeric_limits<int>::max(), options.fade_out / 1000, fade->isChecked());
    auto *default_duration  = make_spinbox(10_min / 1000, options.default_duration / 1000);

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
        accept();
    });

    connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    setLayout(make_layout<QVBoxLayout>(
        fade,
        label_pair("Fade seconds:", fade_secs),
        label_pair("Default duration:", default_duration),
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



Playlist::Playlist(gmplayer::Player::List type, gmplayer::Player *player, QWidget *parent)
    : QWidget(parent)
{
    list = new QListWidget;
    connect(list, &QListWidget::itemActivated, this, [=, this] {
        if (type == gmplayer::Player::List::Track)
            player->load_track(list->currentRow());
        else
            player->load_pair(list->currentRow(), 0);
    });
    auto *shuffle = make_button("Shuffle",    this, [=, this] { player->shuffle(type); });
    auto *up      = make_button("Up",         this, [=, this] { list->setCurrentRow(player->move(type, list->currentRow(), -1)); });
    auto *down    = make_button("Down",       this, [=, this] { list->setCurrentRow(player->move(type, list->currentRow(), +1)); });
    shuffle->setEnabled(false);
    up     ->setEnabled(false);
    down   ->setEnabled(false);
    player->on_playlist_changed([=, this] (gmplayer::Player::List list_type) {
        if (list_type == type) {
            list->clear();
            auto names = player->names(type);
            for (auto &name : names)
                new QListWidgetItem(QString::fromStdString(name), list);
            shuffle->setEnabled(names.size() != 0);
            up     ->setEnabled(names.size() != 0);
            down   ->setEnabled(names.size() != 0);
        }
    });
    setLayout(
        make_layout<QVBoxLayout>(
            new QLabel(QString("%1 playlist").arg(type == gmplayer::Player::List::Track ? "Track" : "File")),
            list,
            make_layout<QHBoxLayout>(shuffle, up, down)
        )
    );
}

void Playlist::set_current(int n) { list->setCurrentRow(n); }
int Playlist::current() const { return list->currentRow(); }

void Playlist::setup_context_menu(auto &&fn)
{
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list, &QWidget::customContextMenuRequested, this, [=, this] (const QPoint &p) { fn(list->mapToGlobal(p)); });
}



PlaylistTab::PlaylistTab(gmplayer::Player *player, const gmplayer::PlayerOptions &options, QWidget *parent)
    : QWidget(parent)
{
    auto *tracklist = new Playlist(gmplayer::Player::List::Track, player);
          filelist  = new Playlist(gmplayer::Player::List::File,  player);
    auto *autoplay     = make_checkbox("Autoplay",     options.autoplay,     this, [=, this] (int state) { player->set_autoplay(state); });
    auto *repeat_track = make_checkbox("Repeat track", options.track_repeat, this, [=, this] (int state) { player->set_track_repeat(state); });
    auto *repeat_file  = make_checkbox("Repeat file",  options.file_repeat,  this, [=, this] (int state) { player->set_file_repeat(state); });

    player->on_track_changed([=, this](int trackno, const gmplayer::Metadata &metadata) {
        tracklist->set_current(trackno);
    });

    player->on_file_changed([=, this] (int fileno) { filelist->set_current(fileno); });

    setLayout(
        make_layout<QVBoxLayout>(
            make_layout<QHBoxLayout>(
                tracklist,
                filelist
            ),
            make_groupbox<QVBoxLayout>("Playlist settings",
                autoplay, repeat_track, repeat_file
            )
        )
    );
}



CurrentlyPlayingTab::CurrentlyPlayingTab(gmplayer::Player *player, QWidget *parent)
    : QWidget(parent)
{
    auto *title   = new QLabel;
    auto *game    = new QLabel;
    auto *system  = new QLabel;
    auto *author  = new QLabel;
    auto *comment = new QLabel;
    auto *dumper  = new QLabel;

    player->on_track_changed([=, this](int trackno, const gmplayer::Metadata &metadata) {
        title   ->setText(metadata.song.data());
        game    ->setText(metadata.game.data());
        author  ->setText(metadata.author.data());
        system  ->setText(metadata.system.data());
        comment ->setText(metadata.comment.data());
        dumper  ->setText(metadata.dumper.data());
    });

    player->on_error([=, this] (gmplayer::Error error) {
        switch (static_cast<gmplayer::ErrType>(error.code.value())) {
        case gmplayer::ErrType::LoadFile:
        case gmplayer::ErrType::Header:
        case gmplayer::ErrType::FileType:
        case gmplayer::ErrType::LoadTrack:
            title   ->setText("");
            game    ->setText("");
            author  ->setText("");
            system  ->setText("");
            comment ->setText("");
            dumper  ->setText("");
        }
    });

    setLayout(
        make_layout<QVBoxLayout>(
            make_groupbox<QFormLayout>("Track info",
                std::make_tuple(new QLabel(tr("Title:")),   title),
                std::make_tuple(new QLabel(tr("Game:")),    game),
                std::make_tuple(new QLabel(tr("System:")),  system),
                std::make_tuple(new QLabel(tr("Author:")),  author),
                std::make_tuple(new QLabel(tr("Comment:")), comment),
                std::make_tuple(new QLabel(tr("Dumper:")),  dumper)
            )
        )
    );
}



Controls::Controls(gmplayer::Player *player, const gmplayer::PlayerOptions &options, QWidget *parent)
    : QWidget(parent)
{
    // duration slider
    auto *duration_slider = new QSlider(Qt::Horizontal);
    auto *duration_label = new QLabel("00:00 / 00:00");
    duration_slider->setEnabled(false);

    connect(duration_slider, &QSlider::valueChanged, this, [=, this] (int ms) {
        duration_label->setText(format_duration(ms, duration_slider->maximum()));
    });

    connect(duration_slider, &QSlider::sliderPressed,  this, [=, this]() {
        history = player->is_playing() ? SliderHistory::WasPlaying : SliderHistory::WasPaused;
        player->pause();
    });

    connect(duration_slider, &QSlider::sliderReleased, this, [=, this]() {
        player->seek(duration_slider->value());
    });

    // playback buttons
    auto make_btn = [&](auto icon, auto &&fn) {
        auto *b = new QToolButton(this);
        b->setIcon(style()->standardIcon(icon));
        QObject::connect(b, &QAbstractButton::clicked, this, fn);
        return b;
    };

    auto *play_btn   = make_btn(QStyle::SP_MediaPlay,         [=, this] { player->play_pause(); });
    auto *stop_btn   = make_btn(QStyle::SP_MediaStop,         [=, this] { player->stop();       });
    auto *next_track = make_btn(QStyle::SP_MediaSkipForward,  [=, this] { player->next();       });
    auto *prev_track = make_btn(QStyle::SP_MediaSkipBackward, [=, this] { player->prev();       });
    play_btn->setEnabled(false);
    stop_btn->setEnabled(false);
    next_track->setEnabled(false);
    prev_track->setEnabled(false);

    // tempo slider
    auto *tempo = new QSlider(Qt::Horizontal);
    auto *tempo_label = new QLabel(QString("%1x").arg(options.tempo, 4, 'f', 2));
    tempo->setMinimum(0);
    tempo->setMaximum(100);
    tempo->setTickInterval(25);
    tempo->setTickPosition(QSlider::TicksBelow);
    tempo->setValue(int_to_tempo(options.tempo));

    auto get_tempo_value = [=, this] (int value) {
        double r = int_to_tempo(value);
        tempo_label->setText(QString("%1x").arg(r, 4, 'f', 2));
        return r;
    };

    connect(tempo, &QSlider::valueChanged, this, [=, this] (int value) {
        auto r = get_tempo_value(value);
        if (tempo->hasTracking())
            player->set_tempo(r);
    });

    connect(tempo, &QSlider::sliderMoved, this, [=, this] { get_tempo_value(tempo->value()); });

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

    // player signals
    player->on_played([=, this] { play_btn->setEnabled(true); play_btn->setIcon(style()->standardIcon(QStyle::SP_MediaPause)); });
    player->on_paused([=, this] {                             play_btn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));  });

    player->on_seeked([=, this] {
        if (history == SliderHistory::WasPlaying)
            player->start_or_resume();
        history = SliderHistory::DontKnow;
    });

    player->on_position_changed([=, this] (int ms) {
        duration_slider->setValue(ms);
    });

    player->on_fade_changed([=, this] (int len) { duration_slider->setRange(0, len); });

    player->on_volume_changed([=, this] (int value) {
        volume_slider->setValue(value);
        volume_btn->setIcon(style()->standardIcon(value == 0 ? QStyle::SP_MediaVolumeMuted : QStyle::SP_MediaVolume));
    });

    player->on_tempo_changed([=, this] (double value) { tempo->setValue(tempo_to_int(value)); });

    player->on_repeat_changed([=, this] (bool, bool) {
        next_track->setEnabled(player->has_next());
        prev_track->setEnabled(player->has_prev());
    });

    player->on_cleared([=, this] {
        duration_slider->setEnabled(false);
        duration_slider->setRange(0, 0);
        duration_slider->setValue(0);
        play_btn->setEnabled(false);
    });

    player->on_track_changed([=, this] (int trackno, const gmplayer::Metadata &metadata) {
        play_btn->setEnabled(true);
        duration_slider->setEnabled(true);
        duration_slider->setRange(0, player->length());
        next_track->setEnabled(player->has_next());
        prev_track->setEnabled(player->has_prev());
    });
    player->on_track_ended([=, this] { play_btn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));  });
    player->on_file_changed([=, this] (int fileno) { stop_btn->setEnabled(true); });
    player->on_file_removed([=, this] (int fileno) {
        next_track->setEnabled(player->has_next());
        prev_track->setEnabled(player->has_prev());
    });

    player->on_error([=, this] (gmplayer::Error error) {
        switch (static_cast<gmplayer::ErrType>(error.code.value())) {
        case gmplayer::ErrType::LoadFile:
        case gmplayer::ErrType::Header:
        case gmplayer::ErrType::FileType:
        case gmplayer::ErrType::LoadTrack:
            duration_slider->setRange(0, 0);
            duration_slider->setValue(0);
        case gmplayer::ErrType::Seek:
        case gmplayer::ErrType::Play:
            play_btn->setEnabled(false);
            play_btn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
            duration_slider->setEnabled(false);
        }
    });

    setLayout(
        make_layout<QVBoxLayout>(
            make_layout<QHBoxLayout>(
                duration_slider,
                duration_label
            ),
            make_layout<QHBoxLayout>(
                prev_track,
                play_btn,
                next_track,
                stop_btn,
                tempo,
                tempo_label,
                volume_btn,
                volume_slider
            )
        )
    );
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
        std::make_tuple("Open files",    [this] {
            auto files = multiple_file_dialog(tr("Open files"), tr(MUSIC_FILE_FILTER));
            open_files(files, OpenFilesFlags::AddToRecent | OpenFilesFlags::ClearAndPlay);
        }),
        std::make_tuple("Open playlist", [this] {
            if (auto f = file_dialog(tr("Open playlist"), tr(PLAYLIST_FILTER)); !f)
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
    recent_files     = new RecentList(file_menu->addMenu(tr("&Recent files")),     load_recent("recent_files"));
    recent_playlists = new RecentList(file_menu->addMenu(tr("R&ecent playlists")), load_recent("recent_playlists"));
    connect(recent_files,     &RecentList::clicked, this, &MainWindow::open_file);
    connect(recent_playlists, &RecentList::clicked, this, &MainWindow::open_playlist);
    load_shortcuts();
    last_file = load_last_dir();

    player->on_track_changed([=, this](int trackno, const gmplayer::Metadata &metadata) {
        player->start_or_resume();
    });

    player->on_shuffled([=, this] (gmplayer::Player::List l) {
        if (l == gmplayer::Player::List::Track)
            player->load_track(0);
        else
            player->load_pair(0, 0);
    });

    player->on_error([=, this] (gmplayer::Error error) {
        if (error)
            msgbox(
                format_error(static_cast<gmplayer::ErrType>(error.code.value())),
                QString::fromStdString(error.details.data())
            );
    });

    auto *playlist_tab = new PlaylistTab(player, options);
    auto *currently_playing_tab = new CurrentlyPlayingTab(player);
    auto *controls = new Controls(player, options);
    auto *tabs = make_tabs(
        std::tuple { playlist_tab, "Playlists" },
        std::tuple { currently_playing_tab, "Current Track" }
    );

    playlist_tab->setup_context_menu([=, this] (const QPoint &p) {
        QMenu menu;
        menu.addAction(tr("Add files"), [=, this] {
            auto files = multiple_file_dialog(tr("Add files"), tr(MUSIC_FILE_FILTER));
            open_files(files);
        });
        menu.addAction(tr("Remove file"), [=, this] {
            auto cur = playlist_tab->current_file();
            if (cur == -1)
                msgbox(tr("A file must be selected first."));
            else
                player->remove_file(cur);
        });
        menu.addAction(tr("Save playlist"), [=, this]() {
            auto filename = save_dialog(tr("Save playlist"), tr("Playlist files (*.playlist)"));
            if (filename.isEmpty())
                return;
            auto file_path = fs::path(filename.toStdString());
            auto f = io::File::open(file_path, io::Access::Write);
            if (!f) {
                msgbox(QString("Couldn't open file %1. (%2)")
                    .arg(QString::fromStdString(file_path.filename().string()))
                    .arg(QString::fromStdString(f.error().message())));
                return;
            }
            player->save_playlist(gmplayer::Player::List::File, f.value());
        });
        menu.exec(p);
    });

    auto *center = new QWidget(this);
    setCentralWidget(center);
    center->setLayout(
        make_layout<QVBoxLayout>(
            tabs,
            controls,
            new QWidget
        )
    );
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
    add_shortcut("play",  "Play/Pause",     "Ctrl+Space",   [=, this] { player->play_pause();            });
    add_shortcut("next",  "Next",           "Ctrl+Right",   [=, this] { player->next();                  });
    add_shortcut("prev",  "Previous",       "Ctrl+Left",    [=, this] { player->prev();                  });
    add_shortcut("stop",  "Stop",           "Ctrl+S",       [=, this] { player->stop();                  });
    add_shortcut("seekf", "Seek forward",   "Right",        [=, this] { player->seek_relative(1_sec);    });
    add_shortcut("seekb", "Seek backwards", "Left",         [=, this] { player->seek_relative(-1_sec);   });
    add_shortcut("volup", "Volume up",      "0",            [=, this] { player->set_volume_relative( 2); });
    add_shortcut("voldw", "Volume down",    "9",            [=, this] { player->set_volume_relative(-2); });
    settings.endGroup();
}

std::optional<fs::path> MainWindow::file_dialog(const QString &window_name, const QString &filter)
{
    auto filename = QFileDialog::getOpenFileName(this, window_name, last_file, filter);
    if (filename.isEmpty())
        return std::nullopt;
    last_file = filename;
    return fs::path{filename.toUtf8().constData()};
}

std::vector<fs::path> MainWindow::multiple_file_dialog(const QString &window_name, const QString &filter)
{
    auto files = QFileDialog::getOpenFileNames(this, window_name, last_file, filter);
    if (!files.isEmpty())
        last_file = files[0];
    std::vector<fs::path> paths;
    for (auto file : files)
        paths.push_back(fs::path(file.toStdString()));
    return paths;
}

QString MainWindow::save_dialog(const QString &window_name, const QString &filter)
{
    return QFileDialog::getSaveFileName(this, window_name, last_file, filter);
}

void MainWindow::open_playlist(fs::path file_path)
{
    recent_playlists->add(file_path);
    auto file = io::File::open(file_path, io::Access::Read);
    if (!file) {
        msgbox(QString("Couldn't open playlist %1 (%2).").arg(QString::fromStdString(file_path.string())),
               QString::fromStdString(file.error().message()));
        return;
    }

    std::vector<fs::path> paths;
    for (std::string line; file.value().get_line(line); ) {
        auto p = fs::path(line);
        if (p.is_relative())
            p = file_path.parent_path() / p;
        paths.push_back(p);
    }

    open_files(paths, OpenFilesFlags::ClearAndPlay);
}

void MainWindow::open_file(fs::path filename)
{
    auto paths = std::array{filename};
    open_files(paths, OpenFilesFlags::AddToRecent | OpenFilesFlags::ClearAndPlay);
}

void MainWindow::open_files(std::span<fs::path> paths, OpenFilesFlags flags)
{
    if ((flags & OpenFilesFlags::AddToRecent) != OpenFilesFlags::None)
        for (auto &p : paths)
            recent_files->add(p);
    if ((flags & OpenFilesFlags::ClearAndPlay) != OpenFilesFlags::None) {
        player->clear();
    }
    auto [errors, num_files] = player->add_files(paths);
    if (errors.size() > 0) {
        QString text;
        for (auto &e : errors)
            text += QString("%1: %2\n")
                        .arg(QString::fromStdString(e.code.message()))
                        .arg(QString::fromStdString(e.details));
        msgbox("Errors were found while opening files.", text);
    }
    if ((flags & OpenFilesFlags::ClearAndPlay) != OpenFilesFlags::None && num_files > 0)
        player->load_pair(0, 0);
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

QString MainWindow::format_error(gmplayer::ErrType type)
{
    switch (type) {
    case gmplayer::ErrType::Seek:      return tr("Got an error while seeking.");
    case gmplayer::ErrType::LoadFile:  return tr("Got an error while loading file '%1'").arg(QString::fromStdString(player->current_file().filename()));
    case gmplayer::ErrType::LoadTrack: return tr("Got an error while loading track '%1' of file '%2'").arg(QString::fromStdString(player->current_track().song));
    case gmplayer::ErrType::Play:      return tr("Got an error while playing.");
    case gmplayer::ErrType::Header:    return tr("Header of file '%1' is invalid.").arg(QString::fromStdString(player->current_file().filename()));
    case gmplayer::ErrType::FileType:  return tr("File %1 has an invalid file type.").arg(QString::fromStdString(player->current_file().filename()));
    default:                           return "";
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    save_recent(recent_files->filenames(), "recent_files");
    save_recent(recent_playlists->filenames(), "recent_playlists");
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
        msgbox("No file paths/urls dropped (this should never happen)");
        return;
    }
    std::vector<fs::path> files;
    QString errors = "";
    for (auto url : mime->urls())
        if (url.isLocalFile())
            files.push_back(fs::path(url.toLocalFile().toStdString()));
        else
            errors += QString("%1: not a local file").arg(url.toString());
    if (!errors.isEmpty())
        msgbox("Errors were found while inspecting dropped files.", errors);
    open_files(files);
    event->acceptProposedAction();
}

} // namespace gui
