#include "visualizer.hpp"

#include <QVBoxLayout>
#include "qtutils.hpp"
#include "player.hpp"
#include "const.hpp"
#include "math.hpp"

namespace gui {

template <typename T>
auto get_minmax()
{
    if constexpr(std::is_same_v<T, i16>) return std::pair { std::numeric_limits<i16>::min(), std::numeric_limits<i16>::max() };
    if constexpr(std::is_same_v<T, f32>) return std::pair { -1.f, 1.f };
}

template <typename T, i64 NUM_CHANNELS, i64 NUM_VOICES>
void plot(std::span<T> data, i64 width, i64 height, i64 voice, auto &&draw)
{
    auto [min, max] = get_minmax<T>();
    auto m = [&](auto s) { return i64(math::map<T>(s, min, max, 0, height)); };
    const auto FRAME_SIZE = NUM_VOICES * NUM_CHANNELS;
    const auto num_frames = data.size() / FRAME_SIZE;
    for (i64 f = 0; f < num_frames; f += 2) {
        auto y0 = m(math::avg(data.subspan((f+0)*FRAME_SIZE + voice*NUM_CHANNELS*2 + 0, NUM_CHANNELS)));
        auto y1 = m(math::avg(data.subspan((f+0)*FRAME_SIZE + voice*NUM_CHANNELS*2 + 2, NUM_CHANNELS)));
        auto y2 = m(math::avg(data.subspan((f+2)*FRAME_SIZE + voice*NUM_CHANNELS*2 + 0, NUM_CHANNELS)));
        draw(std::array{f+0, y0}, std::array{f+1, y1});
        draw(std::array{f+1, y1}, std::array{f+2, y2});
    }
}

template <typename T, i64 NUM_CHANNELS, i64 NUM_VOICES>
Visualizer<T, NUM_CHANNELS, NUM_VOICES>::Visualizer(std::span<T> data, i64 voice, const QString &name, QWidget *parent)
    : QGraphicsView(parent)
    , scene{new QGraphicsScene(this)}
    , data{data}
    , voice{voice}
    , name{name}
{
    setScene(scene);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    render();
}

template <typename T, i64 NUM_CHANNELS, i64 NUM_VOICES>
void Visualizer<T, NUM_CHANNELS, NUM_VOICES>::render()
{
    auto size   = viewport()->size();
    auto width  = size.width();
    auto height = size.height();
    QPixmap image{width, height};
    image.fill({0, 0, 0});
    QPainter painter{&image};
    painter.setPen({0xff, 0xff, 0xff});
    plot<T, NUM_CHANNELS, NUM_VOICES>(data, width, height, voice, [&] (auto p, auto q) {
        painter.drawLine(QPoint(p[0], p[1]), QPoint(q[0], q[1]));
    });
    painter.drawText(8, height - 8, name);
    scene->clear();
    scene->addPixmap(image);
}

VisualizerTab::VisualizerTab(gmplayer::Player *player, QWidget *parent)
    : QWidget(parent)
{
    full = new Visualizer<f32, 2, 1>(full_data, 0, tr("Full"));
    for (int i = 0; i < NUM_VOICES; i++) {
        single[i] = new Visualizer<i16, 2, 8>(single_data, i);
        single[i]->setVisible(false);
    }

    connect(this, &VisualizerTab::updated, this, [=, this] {
        full->render();
        for (auto &s : single)
            s->render();
    });

    player->on_file_changed([=, this] (int) {
        for (auto &s : single)
            s->setVisible(false);
        if (player->is_multi_channel()) {
            auto channels = player->channel_names();
            for (int i = 0; i < channels.size(); i++) {
                single[i]->set_name(QString::fromStdString(channels[i]));
                single[i]->setVisible(true);
            }
        }
    });

    player->on_samples_played([=, this] (std::span<i16> single, std::span<f32> full) {
        std::copy(single.begin(), single.begin() + 32768, single_data.begin());
        std::copy(full.begin(),   full.end(),   full_data.begin());
        emit updated();
    });

    setLayout(
        make_layout<QVBoxLayout>(
            full,
            make_layout<QGridLayout>(
                std::make_tuple(single[0], 0, 0), std::make_tuple(single[1], 0, 1),
                std::make_tuple(single[2], 1, 0), std::make_tuple(single[3], 1, 1),
                std::make_tuple(single[4], 2, 0), std::make_tuple(single[5], 2, 1),
                std::make_tuple(single[6], 3, 0), std::make_tuple(single[7], 3, 1)
            )
        )
    );
}

} // namespace gui
