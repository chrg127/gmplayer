#pragma once

#include <array>
#include <span>
#include <QWidget>
#include <QGraphicsView>
#include "common.hpp"
#include "const.hpp"

namespace gmplayer { class Player; }

namespace gui {

template <typename T, i64 NUM_CHANNELS, i64 NUM_VOICES>
class Visualizer : public QGraphicsView {
    QGraphicsScene *scene;
    std::span<T> data;
    i64 voice;
    QString name;

    void showEvent(QShowEvent *ev)     override { render(); QGraphicsView::showEvent(ev); }
    void resizeEvent(QResizeEvent *ev) override { render(); QGraphicsView::resizeEvent(ev); }
    void wheelEvent(QWheelEvent *)     override { }

public:
    Visualizer(std::span<T> data, i64 voice, const QString &name = "", QWidget *parent = nullptr);
    void set_name(const QString &name) { this->name = name; }
    void render();
};

class VisualizerTab : public QWidget {
    Q_OBJECT
    std::array<i16, NUM_FRAMES * NUM_CHANNELS * NUM_VOICES> single_data = {};
    std::array<f32, NUM_FRAMES * NUM_CHANNELS>              full_data   = {};
    std::array<Visualizer<i16, 2, 8> *, 8> single;
    Visualizer<f32, 2, 1> *full;
public:
    VisualizerTab(gmplayer::Player *player, QWidget *parent = nullptr);
signals:
    void updated();
};

} // namespace gui
