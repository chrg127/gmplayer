#pragma once

#include <array>
#include <span>
#include <QWidget>
#include <QGraphicsView>
#include "common.hpp"
#include "const.hpp"

namespace gmplayer { class Player; }

namespace gui {

class Visualizer : public QGraphicsView {
    Q_OBJECT
    QGraphicsScene *scene;
    std::span<i16> data;
    int channel, channel_size, num_channels;
    QString name;
    void showEvent(QShowEvent *) override;
    void resizeEvent(QResizeEvent *ev) override;
public:
    Visualizer(std::span<i16> data, int channel, int channel_size, int num_channels, QWidget *parent = nullptr);
    void set_name(const QString &name) { this->name = name; }
public slots:
    void render();
};

class VisualizerTab : public QWidget {
    Q_OBJECT
    std::array<i16, NUM_FRAMES * NUM_CHANNELS * NUM_VOICES> single_data = {};
    std::array<i16, NUM_FRAMES * NUM_CHANNELS>              full_data   = {};
    Visualizer *full;
    std::array<Visualizer *, 8> single;
public:
    VisualizerTab(gmplayer::Player *player, QWidget *parent = nullptr);
signals:
    void updated();
};

} // namespace gui
