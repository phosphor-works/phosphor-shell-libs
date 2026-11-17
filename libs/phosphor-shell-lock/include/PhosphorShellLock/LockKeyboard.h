// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <memory>

struct wl_keyboard;
struct wl_seat;

namespace PhosphorShellLock {

class KeyboardState;

// The active keyboard layout and Caps Lock, read from this client's own
// wl_keyboard on the seat: the keymap names the layouts, and the modifiers
// event carries the active layout (the xkb group) and the locked modifiers.
// The compositor sends those to the client with keyboard focus, which the
// lock or prompt surface has while it is up, so Caps Lock already on at lock
// shows from the first frame that surface has focus. Wayland has no standard
// request to switch layouts, so this only reports them.
class LockKeyboard : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString layoutName READ layoutName NOTIFY layoutChanged)
    Q_PROPERTY(bool capsLock READ capsLock NOTIFY capsLockChanged)

public:
    // On the seat of Qt's own Wayland connection. Inert off Wayland or on a
    // seat without a keyboard.
    explicit LockKeyboard(QObject* parent = nullptr);
    // On `seat`, whose connection's default queue someone dispatches.
    // Null: inert.
    explicit LockKeyboard(wl_seat* seat, QObject* parent = nullptr);
    ~LockKeyboard() override;

    // The active layout's name from the keymap, such as "English (US)".
    // Empty until a keymap arrives.
    [[nodiscard]] QString layoutName() const;
    [[nodiscard]] bool capsLock() const;

Q_SIGNALS:
    void layoutChanged();
    void capsLockChanged();

private:
    struct Listener;
    friend struct Listener;

    void publish();

    wl_keyboard* m_keyboard = nullptr;
    std::unique_ptr<KeyboardState> m_state;
    QString m_layoutName;
    bool m_capsLock = false;
};
}
