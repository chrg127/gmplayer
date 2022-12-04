#pragma once

#include <QKeySequence>
#include <QObject>
#include <QWindow>
#include <QTimer>

class KeyRecorder : public QObject {
    Q_OBJECT

    bool is_recording;
    QKeySequence cur_sequence;
    Qt::KeyboardModifiers cur_modifiers;
    // the object to which the events go.
    // it can be a simple button too.
    QObject *event_object;
    QTimer timer;
    int key_count;

    bool eventFilter(QObject *watched, QEvent *event) override;
    void handle_key_release(QKeyEvent *event);
    void handle_key_press(QKeyEvent *event);
    void update_timer();
    void finish();

public:
    explicit KeyRecorder(QObject *event_object, int key_count = 1, QObject *parent = nullptr);
    ~KeyRecorder() noexcept override;
    void start();
    bool recording() const { return is_recording; }

signals:
    void got_key_sequence(const QKeySequence &keySequence);
};
