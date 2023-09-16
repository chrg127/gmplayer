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
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QTreeWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QString>
#include <QStringLiteral>
#include <QShortcut>
#include <QShowEvent>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include <QList>
#include <QScrollArea>
#include <QLineEdit>
#include "qtutils.hpp"
#include "io.hpp"
#include "appinfo.hpp"
#include "math.hpp"
#include "visualizer.hpp"
#include "config.hpp"

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

QString format_position(int ms, int max)
{
    return QString("%1:%2/%3:%4")
        .arg(ms  / 1000 / 60, 2, 10, QChar('0'))
        .arg(ms  / 1000 % 60, 2, 10, QChar('0'))
        .arg(max / 1000 / 60, 2, 10, QChar('0'))
        .arg(max / 1000 % 60, 2, 10, QChar('0'));
};

std::vector<fs::path> load_recent(const std::string &key)
{
    return conf::convert_list_no_errors<fs::path, std::string>(config.get<conf::ValueList>(key));
}

QString format_error(const gmplayer::Error &error)
{
    using enum gmplayer::Error::Type;
    switch (error.type()) {
    case Seek:      return QObject::tr("Got an error while seeking.");
    case LoadFile:  return QObject::tr("Got an error while loading file '%1'")
                                   .arg(QString::fromStdString(error.file_path.filename().string()));
    case LoadTrack: return QObject::tr("Got an error while loading track '%1' of file '%2'")
                                   .arg(QString::fromStdString(error.track_name));
    default:        return "";
    }
}

void handle_error(const gmplayer::Error &error)
{
    if (error)
        msgbox(
            format_error(error),
            QString::fromStdString(error.details.data())
        );
}

std::vector<QString> get_names(gmplayer::Player *player, gmplayer::Playlist::Type type)
{
    std::vector<QString> names;
    if (type == gmplayer::Playlist::Track) {
        player->loop_tracks([&](int, const gmplayer::Metadata &m) {
            names.push_back(QString::fromStdString(m.info[gmplayer::Metadata::Song]));
        });
    } else {
        player->loop_files([&](int, const io::MappedFile &f) {
            names.push_back(QString::fromStdString(f.path().stem().string()));
        });
    }
    return names;
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
        auto *act = new QAction(QString::fromStdString(p.filename().string()), this);
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
    : QDialog(parent)
{
    auto fade_val = config.get<int>("fade");
    fmt::print("fade = {}\n", fade_val);
    auto *fade_box          = make_checkbox(tr("Enable &fade-out"), fade_val != 0);
    auto *fade_secs         = make_spinbox(std::numeric_limits<int>::max(), fade_val / 1000, fade_box->isChecked());
    auto *default_duration  = make_spinbox(10_min / 1000, config.get<int>("default_duration") / 1000);
    auto *fmtstring         = new QLineEdit(QString::fromStdString(config.get<std::string>("status_format_string")));

    connect(fade_box, &QCheckBox::stateChanged, this, [=, this](int state) {
        fade_secs->setEnabled(state);
        if (!state)
            fade_secs->setValue(0);
    });

    auto *button_box = new QDialogButtonBox(QDialogButtonBox::Ok
                                          | QDialogButtonBox::Cancel);
    connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(this, &QDialog::finished, this, [=, this] (int r) {
        if (r == QDialog::Accepted) {
            config.set<int>("fade", fade_secs->value() * 1000);
            config.set<int>("default_duration", default_duration->value());
            config.set<std::string>("status_format_string", fmtstring->text().toStdString());
        }
    });

    setLayout(make_layout<QVBoxLayout>(
        fade_box,
        label_pair("Fade seconds:", fade_secs),
        label_pair("Default duration:", default_duration),
        label_pair("Status format string:", fmtstring),
        button_box
    ));
}



ShortcutsWindow::ShortcutsWindow(std::span<Shortcut> shortcuts, QWidget *parent)
    : QDialog(parent)
{
    auto *layout = new QFormLayout;
    for (auto obj : shortcuts) {
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



static const QString ABOUT_TEXT = R"(
<p style="white-space: pre-wrap; margin: 25px;">

A music player for retro game music.
Supports the following file formats: SPC, GYM, NSF, NSFE, GBS, AY, KSS, HES, VGM, SAP.
</p>
<p style="white-space: pre-wrap; margin: 25px;">
gmplayer is distributed under the GNU GPLv3 license.

<a href="https://github.com/chrg127/gmplayer">Home page</a>.
</p>
)";

static const QString LIBS_TEXT = R"(
<p style="white-space: pre-wrap; margin: 25px;">

gmplayer uses the following libraries:
</p>
<ul>
    <li><a href="https://www.qt.io/">Qt5 (Base, GUI, Widgets, DBus)</a></li>
    <li><a href="https://bitbucket.org/mpyne/game-music-emu/wiki/Home">Game_Music_Emu</a></li>
    <li><a href="https://www.libsdl.org">SDL2</a></li>
</ul>

<p style="white-space: pre-wrap; margin: 25px;">

</p>
)";

AboutDialog::AboutDialog(QWidget *parent)
{
    auto *icon = new QLabel;
    icon->setPixmap(QPixmap((":/icons/gmplayer32.png")));
    auto *label       = new QLabel(QString("<h2><b>%1 %2</b></h2>")
                                    .arg(QString::fromStdString(APP_NAME))
                                    .arg(QString::fromStdString(VERSION)));
    auto *about_label = new QLabel(ABOUT_TEXT);   about_label->setOpenExternalLinks(true);
    auto *lib_label   = new QLabel(LIBS_TEXT);    lib_label->setOpenExternalLinks(true);
    auto *tabs = make_tabs(
        std::make_tuple(about_label, "About"),
        std::make_tuple(lib_label, "Libraries")
    );
    setWindowTitle("About gmplayer");
    auto *title_lt = make_layout<QHBoxLayout>(icon, label);
    title_lt->addStretch();
    setLayout(make_layout<QVBoxLayout>(title_lt, tabs));
}



Playlist::Playlist(gmplayer::Playlist::Type type, gmplayer::Player *player, QWidget *parent)
    : QWidget(parent)
{
    list = new QListWidget;
    connect(list, &QListWidget::itemActivated, this, [=, this] {
        if (type == gmplayer::Playlist::Track)
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
    player->on_playlist_changed([=, this] (gmplayer::Playlist::Type list_type) {
        if (list_type == type) {
            list->clear();
            auto names = get_names(player, type);
            for (auto &name : names)
                new QListWidgetItem(name, list);
            shuffle->setEnabled(names.size() != 0);
            up     ->setEnabled(names.size() != 0);
            down   ->setEnabled(names.size() != 0);
        }
    });
    setLayout(
        make_layout<QVBoxLayout>(
            new QLabel(QString("%1 playlist").arg(type == gmplayer::Playlist::Type::Track ? "Track" : "File")),
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



PlaylistTab::PlaylistTab(gmplayer::Player *player, QWidget *parent)
    : QWidget(parent)
{
    tracklist = new Playlist(gmplayer::Playlist::Track, player);
    filelist  = new Playlist(gmplayer::Playlist::File,  player);
    auto *autoplay     = make_checkbox("Autoplay",     config.get<bool>("autoplay"),     this, [=, this] (int state) { config.set<bool>("autoplay", state); });
    auto *repeat_track = make_checkbox("Repeat track", config.get<bool>("repeat_track"), this, [=, this] (int state) { config.set<bool>("repeat_track", state); });
    auto *repeat_file  = make_checkbox("Repeat file",  config.get<bool>("repeat_file"),  this, [=, this] (int state) { config.set<bool>("repeat_file", state); });

    player->on_track_changed([=, this](int trackno, const gmplayer::Metadata &metadata) {
        tracklist->set_current(trackno);
    });

    player->on_file_changed([=, this] (int fileno) { filelist->set_current(fileno); });

    setLayout(
        make_layout<QVBoxLayout>(
            make_layout<QHBoxLayout>(
                filelist,
                tracklist
            ),
            make_groupbox<QHBoxLayout>("Playlist settings",
                autoplay, repeat_track, repeat_file
            )
        )
    );
}



ChannelWidget::ChannelWidget(int index, gmplayer::Player *player, QWidget *parent)
    : QWidget(parent), index{index}
{
    label = new QLabel;
    auto *checkbox = make_checkbox("Mute", false, this, [=, this] (int state) {
        player->mute_channel(index, bool(state));
    });
    player->on_track_changed([=, this] (int, const gmplayer::Metadata &) {
        checkbox->setChecked(false);
    });

    // volume = new QSlider(Qt::Horizontal);
    // volume->setRange(0, MAX_VOLUME_VALUE);
    // volume->setValue(MAX_VOLUME_VALUE/2);
    // connect(volume, &QSlider::sliderMoved, this, [=, this] (int value) {
    //     player->set_channel_volume(index, value);
    // });
    // player->on_channel_volume_changed([=, this] (int i, int v) {
    //     if (i == index)
    //         volume->setValue(v);
    // });
    //

    setLayout(
        make_layout<QVBoxLayout>(
            label, checkbox
            // make_layout<QHBoxLayout>(new QLabel("Volume:"), volume)
        )
    );
}

void ChannelWidget::set_name(const QString &name) { setEnabled(true);  label->setText(name);                             }
void ChannelWidget::reset()                       { setEnabled(false); label->setText(QString("Channel %1").arg(index)); }
// void ChannelWidget::enable_volume(bool enable)    { volume->setEnabled(enable); }




VoicesTab::VoicesTab(gmplayer::Player *player, QWidget *parent)
    : QWidget(parent)
{
    std::array<ChannelWidget *, 8> channels;
    for (int i = 0; i < 8; i++) {
        channels[i] = new ChannelWidget(i, player);
        channels[i]->reset();
    }

    player->on_file_changed([=, this] (int) {
        auto names = player->channel_names();
        auto multi = player->is_multi_channel();
        for (auto &c : channels)
            c->reset();
        for (int i = 0; i < names.size(); i++) {
            channels[i]->set_name(QString::fromStdString(names[i]));
            // channels[i]->enable_volume(multi);
        }
    });

    setLayout(
        make_layout<QGridLayout>(
            std::make_tuple(channels[0], 0, 0), std::make_tuple(channels[1], 0, 1),
            std::make_tuple(channels[2], 1, 0), std::make_tuple(channels[3], 1, 1),
            std::make_tuple(channels[4], 2, 0), std::make_tuple(channels[5], 2, 1),
            std::make_tuple(channels[6], 3, 0), std::make_tuple(channels[7], 3, 1)
        )
    );
}



Controls::Controls(gmplayer::Player *player, QWidget *parent)
    : QWidget(parent)
{
    // duration slider
    auto *duration_slider = new QSlider(Qt::Horizontal);
    auto *duration_label = new QLabel("00:00 / 00:00");
    duration_slider->setEnabled(false);

    connect(duration_slider, &QSlider::valueChanged, this, [=, this] (int ms) {
        duration_label->setText(format_position(ms, duration_slider->maximum()));
    });

    connect(duration_slider, &QSlider::sliderPressed,  this, [=, this]() {
        history = player->is_playing() ? SliderHistory::WasPlaying : SliderHistory::WasPaused;
        player->pause();
    });

    connect(duration_slider, &QSlider::sliderReleased, this, [=, this]() {
        player->seek(duration_slider->value());
    });

    // playback buttons
    auto *play_btn   = make_tool_btn(this, QStyle::SP_MediaPlay,         [=, this] { player->play_pause(); });
    auto *stop_btn   = make_tool_btn(this, QStyle::SP_MediaStop,         [=, this] { player->stop();       });
    auto *next_track = make_tool_btn(this, QStyle::SP_MediaSkipForward,  [=, this] { player->next(); });
    auto *prev_track = make_tool_btn(this, QStyle::SP_MediaSkipBackward, [=, this] { player->prev(); });
    play_btn->setEnabled(false);
    stop_btn->setEnabled(false);
    next_track->setEnabled(false);
    prev_track->setEnabled(false);

    // tempo slider
    auto tempo = config.get<int>("tempo");
    auto *tempo_slider = new QSlider(Qt::Horizontal);
    auto *tempo_label = new QLabel(QString("%1x").arg(gmplayer::int_to_tempo(tempo), 4, 'f', 2));
    tempo_slider->setMinimum(0);
    tempo_slider->setMaximum(100);
    tempo_slider->setTickInterval(25);
    tempo_slider->setTickPosition(QSlider::TicksBelow);
    tempo_slider->setValue(tempo);

    auto get_tempo_value = [=, this] (int value) {
        tempo_label->setText(QString("%1x").arg(gmplayer::int_to_tempo(value), 4, 'f', 2));
        return value;
    };

    connect(tempo_slider, &QSlider::valueChanged, this, [=, this] (int value) {
        if (tempo_slider->hasTracking())
            config.set<int>("tempo", get_tempo_value(value));
    });

    connect(tempo_slider, &QSlider::sliderMoved, this, [=, this] { get_tempo_value(tempo_slider->value()); });

    // volume slider and button
    auto *volume = new VolumeWidget(config.get<int>("volume"), 0, MAX_VOLUME_VALUE);
    connect(volume, &VolumeWidget::volume_changed, this, [=, this] (auto value) { config.set<int>("volume", value); });

    // status message
    status = new QLabel;
    config.when_set("status_format_string", [=, this](const conf::Value &v) {
        status->setText(QString::fromStdString(gmplayer::format_metadata(v.as<std::string>(), this->metadata)));
    });

    // player signals
    player->on_played([=, this] { play_btn->setEnabled(true); play_btn->setIcon(style()->standardIcon(QStyle::SP_MediaPause)); });
    player->on_paused([=, this] {                             play_btn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));  });

    player->on_seeked([=, this] (int) {
        if (history == SliderHistory::WasPlaying)
            player->start_or_resume();
        history = SliderHistory::DontKnow;
    });

    player->on_position_changed([=, this] (int ms) {
        duration_slider->setValue(ms);
    });

    config.when_set("fade",   [=, this](const conf::Value &value) {
        duration_slider->setRange(0, player->length());

    });
    config.when_set("tempo",  [=, this](const conf::Value &value) { tempo_slider->setValue(value.as<int>()); });
    config.when_set("volume", [=, this](const conf::Value &value) { volume->set_value(value.as<int>()); });

    auto enable_next_buttons = [=, this] {
        next_track->setEnabled(player->has_next());
        prev_track->setEnabled(player->has_prev());
    };

    config.when_set("repeat_file",  [=, this] (const conf::Value &_) { enable_next_buttons(); });
    config.when_set("repeat_track", [=, this] (const conf::Value &_) { enable_next_buttons(); });

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
        enable_next_buttons();
        this->metadata = metadata;
        status->setText(QString::fromStdString(gmplayer::format_metadata(config.get<std::string>("status_format_string"), metadata)));
    });
    player->on_track_ended([=, this] {
        play_btn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        if (config.get<bool>("autoplay")) {
            player->next();
        }
    });
    player->on_file_changed([=, this] (int) { stop_btn->setEnabled(true); });
    player->on_files_removed([=, this] (std::span<int>) { enable_next_buttons(); });

    player->on_error([=, this] (gmplayer::Error error) {
        switch (error.type()) {
        case gmplayer::Error::Type::LoadTrack:
        case gmplayer::Error::Type::Seek:
            play_btn->setEnabled(false);
            play_btn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
            duration_slider->setRange(0, 0);
            duration_slider->setValue(0);
            duration_slider->setEnabled(false);
        }
    });

    auto *lt = make_layout<QHBoxLayout>(
        prev_track,
        play_btn,
        next_track,
        stop_btn,
        tempo_slider,
        tempo_label,
        volume
    );
    lt->insertStretch(4);

    setLayout(
        make_layout<QVBoxLayout>(
            status,
            make_layout<QHBoxLayout>(
                duration_slider,
                duration_label
            ),
            lt
        )
    );
}



VolumeWidget::VolumeWidget(int start_value, int min, int max, int tick_interval, QWidget *parent)
{
    slider = new QSlider(Qt::Horizontal);
    slider->setRange(min, max);
    slider->setValue(start_value);
    connect(slider, &QSlider::sliderMoved, this, [=, this] (int value) { emit volume_changed(value); });

    mute = make_tool_btn(this,
        start_value == 0 ? QStyle::SP_MediaVolumeMuted : QStyle::SP_MediaVolume,
        [=, this, last = start_value == 0 ? max : start_value] () mutable {
            int value = slider->value();
            emit volume_changed(value != 0 ? last = value, 0 : last);
        }
    );

    setLayout(make_layout<QHBoxLayout>(mute, slider));
}

void VolumeWidget::set_value(int value)
{
    slider->setValue(value);
    mute->setIcon(style()->standardIcon(value == 0 ? QStyle::SP_MediaVolumeMuted : QStyle::SP_MediaVolume));
}



DetailsWindow::DetailsWindow(const gmplayer::Metadata &metadata, QWidget *parent)
{
    std::array<QLabel *, 7> labels;
    for (int i = 0; i < labels.size(); i++) {
        labels[i] = new QLabel;
        labels[i]->setText(QString::fromStdString(metadata.info[i]));
    }
    setLayout(make_layout<QFormLayout>(
        std::make_tuple(new QLabel(QObject::tr("Title:")),   labels[gmplayer::Metadata::Song]),
        std::make_tuple(new QLabel(QObject::tr("Game:")),    labels[gmplayer::Metadata::Game]),
        std::make_tuple(new QLabel(QObject::tr("System:")),  labels[gmplayer::Metadata::System]),
        std::make_tuple(new QLabel(QObject::tr("Author:")),  labels[gmplayer::Metadata::Author]),
        std::make_tuple(new QLabel(QObject::tr("Comment:")), labels[gmplayer::Metadata::Comment]),
        std::make_tuple(new QLabel(QObject::tr("Dumper:")),  labels[gmplayer::Metadata::Dumper])
    ));
}

DetailsWindow::DetailsWindow(std::span<const gmplayer::Metadata> metadata, QWidget *parent)
{
    ms.assign(metadata.begin(), metadata.end());
    std::array<QLabel *, 7> labels;
    for (int i = 0; i < labels.size(); i++) {
        labels[i] = new QLabel;
        labels[i]->setText(QString::fromStdString(ms[0].info[i]));
    }
    auto *list = new QListWidget;
    for (const auto &m : ms)
        new QListWidgetItem(QString::fromStdString(m.info[gmplayer::Metadata::Song]), list);
    connect(list, &QListWidget::itemActivated, this, [=, this] {
        for (int i = 0; i < labels.size(); i++)
            labels[i]->setText(QString::fromStdString(ms[list->currentRow()].info[i]));
    });
    setLayout(
        make_layout<QHBoxLayout>(
            list,
            make_layout<QFormLayout>(
                std::make_tuple(new QLabel(QObject::tr("Title:")),   labels[gmplayer::Metadata::Song]),
                std::make_tuple(new QLabel(QObject::tr("Game:")),    labels[gmplayer::Metadata::Game]),
                std::make_tuple(new QLabel(QObject::tr("System:")),  labels[gmplayer::Metadata::System]),
                std::make_tuple(new QLabel(QObject::tr("Author:")),  labels[gmplayer::Metadata::Author]),
                std::make_tuple(new QLabel(QObject::tr("Comment:")), labels[gmplayer::Metadata::Comment]),
                std::make_tuple(new QLabel(QObject::tr("Dumper:")),  labels[gmplayer::Metadata::Dumper])
            )
        )
    );
}



MainWindow::MainWindow(gmplayer::Player *player, QWidget *parent)
    : QMainWindow(parent), player{player}
{
    setWindowTitle(QString::fromStdString(APP_NAME));
    setWindowIcon(QIcon(":/icons/gmplayer64.png"));
    setAcceptDrops(true);

    // menus
    auto *file_menu = create_menu(this, "&File",
        std::make_tuple(tr("Open &files"),    [this] {
            if (auto files = multiple_file_dialog(tr("Open files"), tr(MUSIC_FILE_FILTER)); !files.empty())
                open_files(files, { OpenFilesFlags::AddToRecent, OpenFilesFlags::ClearAndPlay });
        }),
        std::make_tuple(tr("Open &playlist"), [this] {
            if (auto f = file_dialog(tr("Open playlist"), tr(PLAYLIST_FILTER)); f)
                open_playlist(f.value());
        })
    );

    create_menu(this, tr("&Edit"),
        std::make_tuple(tr("&Settings"),  [&] {
            auto *wnd = new SettingsWindow(player, this);
            wnd->open();
        }),
        std::make_tuple(tr("S&hortcuts"), [&] {
            auto *wnd = new ShortcutsWindow(shortcuts, this);
            wnd->open();
        })
    );

    create_menu(this, tr("&About"),
        std::make_tuple(tr("About &gmplayer"), [=, this] {
            auto *wnd = new AboutDialog();
            wnd->open();
        }),
        std::make_tuple(tr("About &Qt"), &QApplication::aboutQt)
    );

    // recent files, shortcuts, open dialog position
    recent_files     = new RecentList(file_menu->addMenu(tr("&Recent files")),     load_recent("recent_files"));
    recent_playlists = new RecentList(file_menu->addMenu(tr("R&ecent playlists")), load_recent("recent_playlists"));
    connect(recent_files,     &RecentList::clicked, this, &MainWindow::open_file);
    connect(recent_playlists, &RecentList::clicked, this, &MainWindow::open_playlist);
    load_shortcuts();
    last_file = QString::fromStdString(config.get<std::string>("last_visited"));

    player->on_track_changed([=, this](int trackno, const gmplayer::Metadata &metadata) {
        player->start_or_resume();
    });

    player->on_shuffled([=, this] (gmplayer::Playlist::Type l) {
        if (l == gmplayer::Playlist::Track)
            player->load_track(0);
        else
            player->load_pair(0, 0);
    });

    player->on_error([=, this] (gmplayer::Error error) { handle_error(error); });

    // tabs
    auto *playlist_tab      = new PlaylistTab(player);
    auto *voices_tab        = new VoicesTab(player);
    auto *visualizer_tab    = new VisualizerTab(player);
    controls = new Controls(player);
    auto *tabs = make_tabs(
        std::tuple { playlist_tab,   tr("&Playlists") },
        std::tuple { voices_tab,     tr("&Channels") },
        std::tuple { visualizer_tab, tr("&Visualizer") }
    );

    playlist_tab->setup_context_menu(gmplayer::Playlist::File, [=, this] (const QPoint &p) {
        QMenu menu;
        menu.addAction(tr("&Add files"), [=, this] {
            if (auto files = multiple_file_dialog(tr("Add files"), tr(MUSIC_FILE_FILTER)); !files.empty())
                open_files(files);
        });
        menu.addAction(tr("&Remove file"), [=, this] {
            if (auto cur = playlist_tab->current_file(); cur != -1)
                player->remove_file(cur);
            else
                msgbox(tr("A file must be selected first."));
        });
        menu.addAction(tr("&Save playlist"), [=, this] {
            if (auto filename = save_dialog(tr("Save playlist"), tr("Playlist files (*.playlist)")); !filename.isEmpty()) {
                auto file_path = fs::path(filename.toStdString());
                auto savefile = io::File::open(file_path, io::Access::Write);
                if (!savefile) {
                    msgbox(tr("Couldn't open file %1. (%2)")
                        .arg(QString::fromStdString(file_path.filename().string()))
                        .arg(QString::fromStdString(savefile.error().message())));
                    return;
                }
                player->loop_files([&](int, const io::MappedFile &f) {
                    fmt::print(savefile.value().data(), "{}\n", f.path().string());
                });
            }
        });
        auto *action = menu.addAction(tr("&Details"), [=, this] {
            if (auto id = playlist_tab->current_file(); id != -1) {
                auto mds = player->file_tracks(id);
                if (mds.empty()) {
                    msgbox("Can't get contents of this file.");
                    return;
                }
                auto *wnd = new DetailsWindow(mds);
                wnd->open();
            }
        });
        action->setEnabled(playlist_tab->current_file() != -1);
        menu.exec(p);
    });

    playlist_tab->setup_context_menu(gmplayer::Playlist::Track, [=, this] (const QPoint &p) {
        QMenu menu;
        auto *action = menu.addAction(tr("&Details"), [=, this] {
            if (auto trackno = playlist_tab->current_track(); trackno != -1) {
                auto *wnd = new DetailsWindow(player->track_info(player->current_track()));
                wnd->open();
            }
        });
        action->setEnabled(playlist_tab->current_track() != -1);
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
    auto add_shortcut = [&](const std::string &key, const QString &display_name, auto &&fn) {
        auto value = QString::fromStdString(config.get<std::string>(key));
        auto *shortcut = new QShortcut(QKeySequence(value), this);
        connect(shortcut, &QShortcut::activated, this, fn);
        shortcuts.push_back({
            .shortcut = shortcut,
            .display_name = display_name,
            .key = key,
        });
    };
    add_shortcut("play_pause",      "Play/Pause",     [=, this] { player->play_pause();            });
    add_shortcut("next",            "Next",           [=, this] { player->next();                  });
    add_shortcut("prev",            "Previous",       [=, this] { player->prev();                  });
    add_shortcut("stop",            "Stop",           [=, this] { player->stop();                  });
    add_shortcut("seek_forward",    "Seek forward",   [=, this] { player->seek_relative(1_sec);    });
    add_shortcut("seek_backward",   "Seek backwards", [=, this] { player->seek_relative(-1_sec);   });
    add_shortcut("volume_up",       "Volume up",      [=, this] { config.set<int>("volume", config.get<int>("volume") + 2); });
    add_shortcut("volume_down",     "Volume down",    [=, this] { config.set<int>("volume", config.get<int>("volume") - 2);});
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
        msgbox(tr("Couldn't open playlist %1 (%2).").arg(QString::fromStdString(file_path.string())),
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

    open_files(paths, { OpenFilesFlags::ClearAndPlay });
}

void MainWindow::open_file(fs::path filename)
{
    auto paths = std::array{filename};
    open_files(paths, { OpenFilesFlags::AddToRecent, OpenFilesFlags::ClearAndPlay });
}

void MainWindow::open_files(std::span<fs::path> paths, Flags<OpenFilesFlags> flags)
{
    if (flags.contains(OpenFilesFlags::AddToRecent))
        for (auto &p : paths)
            recent_files->add(p);
    if (flags.contains(OpenFilesFlags::ClearAndPlay))
        player->clear();
    auto errors = player->add_files(paths);
    if (errors.size() > 0) {
        QString text;
        for (auto &e : errors)
            text += QString("%1: %2\n")
                        .arg(QString::fromStdString(e.first.string()))
                        .arg(QString::fromStdString(e.second.message()));
        msgbox(tr("Errors were found while opening files."), text);
    }
    if (flags.contains(OpenFilesFlags::ClearAndPlay))
        player->load_pair(0, 0);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    auto f = [&](const auto &paths) {
        conf::ValueList v;
        for (auto &p : paths)
            v.push_back(conf::Value(p.string()));
        return v;
    };
    config.set("recent_files",     f(recent_files->get_paths()));
    config.set("recent_playlists", f(recent_playlists->get_paths()));
    config.set("last_visited", last_file.toStdString());
    for (auto &s : shortcuts)
        config.set(s.key, s.shortcut->key().toString().toStdString());
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
        msgbox(tr("No file paths/urls dropped (this should never happen)"));
        return;
    }
    std::vector<fs::path> files;
    QString errors = "";
    for (auto url : mime->urls())
        if (url.isLocalFile())
            files.push_back(fs::path(url.toLocalFile().toStdString()));
        else
            errors += tr("%1: not a local file").arg(url.toString());
    if (!errors.isEmpty())
        msgbox(tr("Errors were found while inspecting dropped files."), errors);
    open_files(files);
    event->acceptProposedAction();
}

} // namespace gui
