#pragma once

#include <tuple>
#include <QObject>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>

template <typename T> void add_to_layout(T *lt, QWidget *w) { lt->addWidget(w); }
template <typename T> void add_to_layout(T *lt, QLayout *l) { lt->addLayout(l); }

template <typename T>
inline T *make_layout(auto... widgets)
{
    auto *lt = new T;
    if constexpr(std::is_same_v<T, QGridLayout>)
        (lt->addWidget(std::get<0>(widgets),
                       std::get<1>(widgets),
                       std::get<2>(widgets)), ...);
    else if constexpr(std::is_same_v<T, QFormLayout>)
        (lt->addRow(std::get<0>(widgets),
                    std::get<1>(widgets)), ...);
    else
        (add_to_layout(lt, widgets), ...);
    return lt;
}

template <typename T>
inline QGroupBox *make_groupbox(const QString &title, auto... widgets)
{
    auto *box = new QGroupBox(title);
    box->setLayout(make_layout<T>(widgets...));
    return box;
}

template <typename T>
QFormLayout *label_pair(const char *text, T *widget)
{
    return make_layout<QFormLayout>(std::tuple {
        new QLabel(text),
        widget
    });
}

inline void msgbox(const QString &msg)
{
    QMessageBox box;
    box.setText(msg);
    box.exec();
}

template <typename T>
QMenu *create_menu(T *window, const char *name, auto&&... actions)
{
    auto *menu = window->menuBar()->addMenu(QObject::tr(name));
    auto f = [=](auto &a) {
        auto *act = new QAction(QObject::tr(std::get<0>(a)), window);
        QObject::connect(act, &QAction::triggered, window, std::get<1>(a));
        menu->addAction(act);
    };
    (f(actions), ...);
    return menu;
}

QCheckBox *make_checkbox(const QString &name, bool checked, QObject *o, auto &&fn)
{
    auto c = new QCheckBox(name);
    c->setChecked(checked);
    QObject::connect(c, &QCheckBox::stateChanged, o, fn);
    return c;
}
