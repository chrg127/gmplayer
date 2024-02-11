/*
 * This file was heavily edited from
 * https://api.kde.org/frameworks/kguiaddons/html/keysequencerecorder_8cpp_source.html
 * which is part of the KGuiAddons library.
 *
 * Therefore, it will still be subject to the LGPL 2.0 license.
 */

#include "keyrecorder.hpp"
#include <cassert>
#include <QDebug>
#include <QEvent>
#include <QKeyEvent>
#include <array>
#include <qnamespace.h>

constexpr int MAX_KEY_COUNT = 4;

// these are the modifiers we care about.
// remove, for example, the shift modifiers and the shift key
// will stop being listened.
constexpr Qt::KeyboardModifiers MASK = Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier
                                     | Qt::MetaModifier  | Qt::KeypadModifier;

namespace {

QKeySequence append_to_sequence(const QKeySequence &sequence, QKeyCombination comb, int key_count)
{
    if (sequence.count() >= key_count) {
        qDebug() << "Cannot append to a key to a sequence which is already of length"
                 << sequence.count();
        return sequence;
    }
    std::array<QKeyCombination, MAX_KEY_COUNT> keys{
        sequence[0], sequence[1], sequence[2], sequence[3]
    };
    keys[sequence.count()] = comb;
    return QKeySequence(keys[0], keys[1], keys[2], keys[3]);
}

} // namespace



KeyRecorder::KeyRecorder(QObject *obj, int kc, QObject *parent)
    : QObject(parent)
    , is_recording(false)
    , cur_sequence(QKeySequence())
    , event_object(obj)
    , key_count(kc)
{
    assert(event_object && "object is nullptr");
    event_object->installEventFilter(this);
    // qDebug() << "listening for events in" << event_object;
}

KeyRecorder::~KeyRecorder() noexcept
{
    event_object->removeEventFilter(this);
}

void KeyRecorder::start()
{
    if (!event_object) {
        qDebug() << "Cannot record without a window";
        return;
    }
    is_recording = true;
    cur_sequence = QKeySequence();
}



/* private functions */

bool KeyRecorder::eventFilter(QObject *watched, QEvent *event)
{
    if (!is_recording)
        return QObject::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::ShortcutOverride:
    case QEvent::ContextMenu:
        event->accept();
        return true;
    case QEvent::KeyRelease:
        handle_key_release(static_cast<QKeyEvent *>(event));
        event->accept();
        return true;
    case QEvent::KeyPress:
        handle_key_press(  static_cast<QKeyEvent *>(event));
        event->accept();
        return true;
    default:
        return QObject::eventFilter(watched, event);
    }
}

void KeyRecorder::handle_key_press(QKeyEvent *event)
{
    cur_modifiers = event->modifiers() & MASK;
    int key = event->key();
    switch (key) {
    case -1:
        qDebug() << "Got unknown key";
        return;
    case 0:
        break;
    case Qt::Key_AltGr:
        break;
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
        cur_modifiers |= Qt::MetaModifier;
        /* FALLTHROUGH */
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Alt:
    case Qt::Key_Meta:
        update_timer();
        break;
    default: {
        // auto the_key = static_cast<Qt::Key>(key);
        QKeyCombination comb = (key == Qt::Key_Backtab) && (cur_modifiers & Qt::ShiftModifier)
            ? QKeyCombination(cur_modifiers, Qt::Key_Tab)
            : QKeyCombination(cur_modifiers, static_cast<Qt::Key>(key));
        cur_sequence = append_to_sequence(cur_sequence, comb, key_count);
        if (cur_sequence.count() == key_count) {
            finish();
            break;
        }
        update_timer();
    }
    }
}

void KeyRecorder::handle_key_release(QKeyEvent *event)
{
    Qt::KeyboardModifiers modifiers = event->modifiers() & MASK;
    switch (event->key()) {
    case -1:
        return;
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
        // Qt doesn't properly recognize Super_L/Super_R as MetaModifier
        modifiers &= ~Qt::MetaModifier;
    }

    if ((modifiers & cur_modifiers) < cur_modifiers) {
        cur_modifiers = modifiers;
        update_timer();
    }
}

void KeyRecorder::update_timer()
{
    if (cur_sequence != 0 && !cur_modifiers) {
        // no modifier key pressed currently. Start the timeout
        timer.start(600);
    } else {
        // a modifier is pressed. Stop the timeout
        timer.stop();
    }
}

void KeyRecorder::finish()
{
    is_recording = false;
    cur_modifiers = Qt::NoModifier;
    timer.stop();
    emit got_key_sequence(cur_sequence);
}
