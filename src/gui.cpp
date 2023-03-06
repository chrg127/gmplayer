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
    auto *silence_detection = make_checkbox(tr("Do silence detection"), options.silence_detection);

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
    player->on_playlist_changed(type, [=, this] {
        list->clear();
        auto names = player->names(type);
        for (auto &name : names)
            new QListWidgetItem(QString::fromStdString(name), list);
        shuffle->setEnabled(names.size() != 0);
        up     ->setEnabled(names.size() != 0);
        down   ->setEnabled(names.size() != 0);
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



MainWindow::MainWindow(gmplayer::Player *player, QWidget *parent)
    : QMainWindow(parent), player{player}
{
    setWindowTitle("gmplayer");
    setWindowIcon(QIcon(":/icons/gmplayer64.png"));
    setAcceptDrops(true);

    auto options = player->options();

    // menus
    auto *file_menu = create_menu(this, "&File",
        std::make_tuple("Open file",     [this] {
            if (auto files = multiple_file_dialog(tr("Open file"), tr(MUSIC_FILE_FILTER)); !files.empty())
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

    // duration slider
    auto *duration_slider = new QSlider(Qt::Horizontal);
    auto *duration_label = new QLabel("00:00 / 00:00");
    connect(duration_slider, &QSlider::sliderPressed,  this, [=, this]() {
        history = player->is_playing() ? SliderHistory::WasPlaying : SliderHistory::WasPaused;
        player->pause();
    });
    connect(duration_slider, &QSlider::sliderReleased, this, [=, this]() {
        player->seek(duration_slider->value());
    });
    connect(duration_slider, &QSlider::sliderMoved, this, [=, this] (int ms) {
        duration_label->setText(format_duration(ms, duration_slider->maximum()));
    });

    player->on_seeked([=, this] {
        if (history == SliderHistory::WasPlaying)
            player->start_or_resume();
        history = SliderHistory::DontKnow;
    });

    player->on_position_changed([=, this] (int ms) {
        duration_slider->setValue(ms);
        duration_label->setText(format_duration(ms, duration_slider->maximum()));
    });

    player->on_fade_changed([=, this] (int len) { duration_slider->setRange(0, len); });

    // buttons under duration slider
    auto make_btn = [&](auto icon, auto &&fn) {
        auto *b = new QToolButton(this);
        b->setIcon(style()->standardIcon(icon));
        QObject::connect(b, &QAbstractButton::clicked, this, fn);
        return b;
    };

    auto *play_btn        = make_btn(QStyle::SP_MediaPlay,         [=, this] { player->play_pause(); });
    auto *stop_btn  = make_btn(QStyle::SP_MediaStop,         [=, this] { player->stop();       });
    next_track      = make_btn(QStyle::SP_MediaSkipForward,  [=, this] { player->next();       });
    prev_track      = make_btn(QStyle::SP_MediaSkipBackward, [=, this] { player->prev();       });
    play_btn->setEnabled(false);
    stop_btn->setEnabled(false);
    next_track->setEnabled(false);
    prev_track->setEnabled(false);

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

    player->on_volume_changed([=, this] (int value) {
        volume_slider->setValue(value);
        volume_btn->setIcon(style()->standardIcon(value == 0 ? QStyle::SP_MediaVolumeMuted
                                                             : QStyle::SP_MediaVolume));
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

    // track information
    auto *title   = new QLabel;
    auto *game    = new QLabel;
    auto *system  = new QLabel;
    auto *author  = new QLabel;
    auto *comment = new QLabel;
    auto *dumper  = new QLabel;

    // track and file playlist
    auto *tracklist = new Playlist(gmplayer::Player::List::Track, player);
    auto *filelist  = new Playlist(gmplayer::Player::List::File,  player);

    player->on_track_changed([=, this](int trackno, const gmplayer::Metadata &metadata) {
        title   ->setText(metadata.song.data());
        game    ->setText(metadata.game.data());
        author  ->setText(metadata.author.data());
        system  ->setText(metadata.system.data());
        comment ->setText(metadata.comment.data());
        dumper  ->setText(metadata.dumper.data());
        play_btn->setEnabled(true);
        duration_slider->setEnabled(true);
        duration_slider->setRange(0, player->length());
        update_next_prev_track();
        tracklist->set_current(trackno);
        player->start_or_resume();
    });

    player->on_file_changed([=, this] (int fileno) {
        stop_btn->setEnabled(true);
        filelist->set_current(fileno);
    });

    // context menu for file playlist
    filelist->setup_context_menu([=, this] (const QPoint &p) {
        QMenu menu;
        menu.addAction("Add to playlist...", [=, this] {
            if (auto files = multiple_file_dialog(tr("Open file"), tr(MUSIC_FILE_FILTER)); !files.empty())
                open_files(files);
        });
        menu.addAction("Remove from playlist", [=, this] {
            if (filelist->current() == -1)
                msgbox("An item must be selected first.");
            else {
                player->remove_file(filelist->current());
                update_next_prev_track();
            }
        });
        menu.addAction("Save playlist", [=, this]() {
            auto filename = save_dialog(tr("Save playlist"), "Playlist files (*.playlist)");
            if (filename.isEmpty())
                return;
            auto path = fs::path(filename.toStdString());
            if (auto f = io::File::open(path, io::Access::Write); f)
                player->save_playlist(gmplayer::Player::List::File, f.value());
            else
                msgbox(QString("Couldn't open file %1. (%2)")
                    .arg(QString::fromStdString(path.filename().string()))
                    .arg(QString::fromStdString(f.error().message())));
        });
        menu.exec(p);
    });

    // playlist settings
    auto *autoplay     = make_checkbox("Autoplay",     options.autoplay,     this, [=, this] (int state) { player->set_autoplay(state); });
    auto *repeat_track = make_checkbox("Repeat track", options.track_repeat, this, [=, this] (int state) { player->set_track_repeat(state); });
    auto *repeat_file  = make_checkbox("Repeat file",  options.file_repeat,  this, [=, this] (int state) { player->set_file_repeat(state); });

    player->on_repeat_changed([=, this] (bool, bool) { update_next_prev_track(); });

    player->on_played(     [=, this] { play_btn->setEnabled(true); play_btn->setIcon(style()->standardIcon(QStyle::SP_MediaPause)); });
    player->on_paused(     [=, this] {                             play_btn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));  });
    player->on_track_ended([=, this] {                             play_btn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));  });
    player->on_shuffled(   [=, this] (gmplayer::Player::List l) {
        if (l == gmplayer::Player::List::Track)
            player->load_track(0);
        else
            player->load_pair(0, 0);
    });

    player->on_error([=, this] (gmplayer::Error error) {
        if (!error)
            return;
        msgbox(
            format_error(static_cast<gmplayer::ErrType>(error.code.value())),
            QString::fromStdString(error.details.data())
        );
        switch (static_cast<gmplayer::ErrType>(error.code.value())) {
        case gmplayer::ErrType::LoadFile:
        case gmplayer::ErrType::Header:
        case gmplayer::ErrType::FileType:
            duration_slider->setRange(0, 0);
            duration_label->setText("00:00 / 00:00");
            title   ->setText("");
            game    ->setText("");
            author  ->setText("");
            system  ->setText("");
            comment ->setText("");
            dumper  ->setText("");
            update_next_prev_track();
        case gmplayer::ErrType::LoadTrack:
        case gmplayer::ErrType::Seek:
        case gmplayer::ErrType::Play:
            play_btn->setEnabled(false);
            play_btn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
            player->pause();
            duration_slider->setEnabled(false);
        }
    });

    // create the gui
    auto *center = new QWidget(this);
    setCentralWidget(center);
    center->setLayout(
        make_layout<QVBoxLayout>(
            make_layout<QHBoxLayout>(
                tracklist,
                filelist
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
    next_track->setEnabled(player->has_next());
    prev_track->setEnabled(player->has_prev());
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

void MainWindow::open_playlist(fs::path file_path)
{
    auto file = io::File::open(file_path, io::Access::Read);
    if (!file) {
        msgbox(QString("Couldn't open playlist %1 (%2).").arg(QString::fromStdString(file_path.string())),
               QString::fromStdString(file.error().message()));
        return;
    }
    recent_playlists->add(file_path);

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
    if ((flags & OpenFilesFlags::ClearAndPlay) != OpenFilesFlags::None)
        player->clear();
    auto errors = player->add_files(paths);
    if (errors.size() > 0) {
        QString text;
        for (auto &e : errors)
            text += QString("%1: %2\n")
                        .arg(QString::fromStdString(e.code.message()))
                        .arg(QString::fromStdString(e.details));
        msgbox("Errors were found while opening files.", text);
    } else
        if ((flags & OpenFilesFlags::ClearAndPlay) != OpenFilesFlags::None)
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
