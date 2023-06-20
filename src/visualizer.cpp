#include "visualizer.hpp"

#include <QVBoxLayout>
#include "qtutils.hpp"
#include "player.hpp"
#include "const.hpp"
#include "math.hpp"

namespace gui {

void plot(std::span<i16> data, i64 width, i64 height, int voice, int num_channels, int num_voices, auto &&draw)
{
    auto m = [&](auto s) { return math::map<i64>(s, std::numeric_limits<i16>::min(), std::numeric_limits<i16>::max(), 0, height); };
    const auto frame_size = num_voices * num_channels;
    const auto num_frames = data.size() / frame_size;
    for (i64 f = 0; f < num_frames; f += 2) {
        auto y0 = m(math::avg(data.subspan((f+0)*frame_size + voice*num_channels*2 + 0, num_channels)));
        auto y1 = m(math::avg(data.subspan((f+0)*frame_size + voice*num_channels*2 + 2, num_channels)));
        auto y2 = m(math::avg(data.subspan((f+2)*frame_size + voice*num_channels*2 + 0, num_channels)));
        draw(std::array{f+0, y0}, std::array{f+1, y1});
        draw(std::array{f+1, y1}, std::array{f+2, y2});
    }
}

Visualizer::Visualizer(std::span<i16> data, int channel, int channel_size, int num_channels, QWidget *parent)
    : QGraphicsView(parent)
    , scene{new QGraphicsScene(this)}
    , data{data}
    , channel{channel}
    , channel_size{channel_size}
    , num_channels{num_channels}
{
    setScene(scene);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    render();
}

void Visualizer::render()
{
    auto size   = viewport()->size();
    auto width  = size.width();
    auto height = size.height();
    QPixmap image{width, height};
    image.fill({0, 0, 0});
    QPainter painter{&image};
    painter.setPen({0xff, 0xff, 0xff});
    plot(data, width, height, channel, channel_size, num_channels, [&] (auto p, auto q) {
        painter.drawLine(QPoint(p[0], p[1]), QPoint(q[0], q[1]));
    });
    painter.drawText(8, height - 8, name);
    scene->clear();
    scene->addPixmap(image);
}

void Visualizer::showEvent(QShowEvent *ev)
{
    render();
    QGraphicsView::showEvent(ev);
}

void Visualizer::resizeEvent(QResizeEvent *ev)
{
    render();
    QGraphicsView::resizeEvent(ev);
}

VisualizerTab::VisualizerTab(gmplayer::Player *player, QWidget *parent)
    : QWidget(parent)
{
    full = new Visualizer(full_data, 0, 2, 1);
    connect(this, &VisualizerTab::updated, full, &Visualizer::render);
    full->set_name(tr("Full"));
    for (int i = 0; i < NUM_VOICES; i++) {
        single[i] = new Visualizer(single_data, i, 2, 8);
        connect(this, &VisualizerTab::updated, single[i], &Visualizer::render);
        single[i]->setVisible(false);
    }

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

    player->on_samples_played([=, this] (std::span<i16> single, std::span<i16> full) {
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
