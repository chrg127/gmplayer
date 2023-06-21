#pragma once

#include <tuple>
#include <QAction>
#include <QObject>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QGridLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QGroupBox>
#include <QMessageBox>
#include <QDirIterator>
#include <QSpinBox>
#include <QMenuBar>
#include <QDebug>
#include <QToolButton>

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
inline QFormLayout *label_pair(const char *text, T *widget)
{
    return make_layout<QFormLayout>(std::tuple {
        new QLabel(text),
        widget
    });
}

inline void msgbox(const QString &msg, const QString &details = "", const QString &info = "")
{
    QMessageBox box;
    box.setText(msg);
    box.setInformativeText(info);
    box.setDetailedText(details);
    box.exec();
}

template <typename T>
inline QMenu *create_menu(T *window, const QString &name, auto&&... actions)
{
    auto *menu = window->menuBar()->addMenu(name);
    auto f = [=](auto &a) {
        auto *act = new QAction(std::get<0>(a), window);
        QObject::connect(act, &QAction::triggered, window, std::get<1>(a));
        menu->addAction(act);
    };
    (f(actions), ...);
    return menu;
}

inline QCheckBox *make_checkbox(const QString &name, bool checked, QObject *o, auto &&fn)
{
    auto c = new QCheckBox(name);
    c->setChecked(checked);
    QObject::connect(c, &QCheckBox::stateChanged, o, fn);
    return c;
}

inline QCheckBox *make_checkbox(const QString &name, bool checked)
{
    auto c = new QCheckBox(name);
    c->setChecked(checked);
    return c;
}

inline QComboBox *make_combo(int cur, auto&&... args)
{
    auto *b = new QComboBox;
    (b->addItem(std::get<0>(args), std::get<1>(args)), ...);
    b->setCurrentIndex(cur);
    return b;
}

inline QSpinBox *make_spinbox(int maximum, int value, bool enabled = true)
{
    auto *s = new QSpinBox;
    s->setMaximum(maximum);
    s->setValue(value);
    s->setEnabled(enabled);
    return s;
}

inline QPushButton *make_button(const QString &name, QObject *o, auto &&fn)
{
    auto *b = new QPushButton(name);
    QObject::connect(b, &QPushButton::released, o, fn);
    return b;
}

inline QTabWidget *make_tabs(auto&&... args)
{
    auto *tabs = new QTabWidget;
    (tabs->addTab(std::get<0>(args), std::get<1>(args)), ...);
    return tabs;
}

inline void print_all_resources()
{
    QDirIterator it(":", QDirIterator::Subdirectories);
    while (it.hasNext())
        qDebug() << it.next();
}

QToolButton *make_tool_btn(auto *obj, auto icon, auto &&fn)
{
    auto *b = new QToolButton;
    b->setIcon(obj->style()->standardIcon(icon));
    QObject::connect(b, &QAbstractButton::clicked, obj, fn);
    return b;
}
