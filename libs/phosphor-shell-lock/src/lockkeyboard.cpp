// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later
#include <PhosphorShellLock/LockKeyboard.h>

#include "keyboardstate.h"

#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>

#include <wayland-client.h>

#include <sys/mman.h>
#include <unistd.h>

#include <cstring>

namespace PhosphorShellLock {
namespace {
wl_seat* qtSeat()
{
    auto* app = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr;
    // Qt asks the seat for a keyboard only when it has the capability, and
    // asking a seat that never had one is a protocol error.
    return app && app->keyboard() ? app->seat() : nullptr;
}
}

struct LockKeyboard::Listener
{
    static void keymap(void* data, wl_keyboard*, uint32_t format, int32_t fd, uint32_t size)
    {
        auto* self = static_cast<LockKeyboard*>(data);
        QByteArray text;
        if (format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 && size > 0) {
            void* mapped = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
            if (mapped != MAP_FAILED) {
                // The keymap is NUL-terminated within `size`.
                const auto* chars = static_cast<const char*>(mapped);
                text = QByteArray(chars, qsizetype(strnlen(chars, size)));
                munmap(mapped, size);
            }
        }
        close(fd);
        self->m_state->setKeymap(text);
        self->publish();
    }

    static void enter(void*, wl_keyboard*, uint32_t, wl_surface*, wl_array*)
    {
    }

    static void leave(void*, wl_keyboard*, uint32_t, wl_surface*)
    {
    }

    static void key(void*, wl_keyboard*, uint32_t, uint32_t, uint32_t, uint32_t)
    {
    }

    static void modifiers(void* data, wl_keyboard*, uint32_t, uint32_t depressed, uint32_t latched, uint32_t locked,
                          uint32_t group)
    {
        auto* self = static_cast<LockKeyboard*>(data);
        self->m_state->updateModifiers(depressed, latched, locked, group);
        self->publish();
    }

    static void repeatInfo(void*, wl_keyboard*, int32_t, int32_t)
    {
    }

    static constexpr wl_keyboard_listener listener = {
        .keymap = keymap,
        .enter = enter,
        .leave = leave,
        .key = key,
        .modifiers = modifiers,
        .repeat_info = repeatInfo,
    };
};

LockKeyboard::LockKeyboard(QObject* parent)
    : LockKeyboard(qtSeat(), parent)
{
}

LockKeyboard::LockKeyboard(wl_seat* seat, QObject* parent)
    : QObject(parent)
    , m_state(std::make_unique<KeyboardState>())
{
    if (!seat) {
        return;
    }
    // A wl_keyboard of our own: Qt's has its listener already, and keeps
    // its xkb state private.
    m_keyboard = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(m_keyboard, &Listener::listener, this);
}

LockKeyboard::~LockKeyboard()
{
    if (!m_keyboard) {
        return;
    }
    if (wl_keyboard_get_version(m_keyboard) >= WL_KEYBOARD_RELEASE_SINCE_VERSION) {
        wl_keyboard_release(m_keyboard);
    } else {
        wl_keyboard_destroy(m_keyboard);
    }
}

QString LockKeyboard::layoutName() const
{
    return m_layoutName;
}

bool LockKeyboard::capsLock() const
{
    return m_capsLock;
}

void LockKeyboard::publish()
{
    const QString layoutName = m_state->layoutName();
    const bool capsLock = m_state->capsLock();
    if (layoutName != m_layoutName) {
        m_layoutName = layoutName;
        Q_EMIT layoutChanged();
    }
    if (capsLock != m_capsLock) {
        m_capsLock = capsLock;
        Q_EMIT capsLockChanged();
    }
}
}
