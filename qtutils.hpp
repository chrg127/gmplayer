#pragma once

#include <tuple>
#include <QGridLayout>

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
