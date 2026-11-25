// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// The lock screen's layout and Caps Lock readout. KeyboardState is checked
// against real keymaps from xkbcommon; LockKeyboard against a seat served
// by a libwayland server display in this process, connected over a socket
// pair, so the keymap and modifiers events take the real wire path.

#include <PhosphorShellLock/LockKeyboard.h>

#include "keyboardstate.h"

#include <QSignalSpy>
#include <QTest>

#include <wayland-client.h>
#include <wayland-server.h>
#include <xkbcommon/xkbcommon.h>

#include <poll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

using PhosphorShellLock::KeyboardState;
using PhosphorShellLock::LockKeyboard;

// A two-layout keymap compiled from the system's xkb data, as text, the
// form a compositor sends. Empty when that data is missing.
struct TestKeymap
{
    QByteArray text;
    uint32_t capsMask = 0;
};

namespace {

TestKeymap compileKeymap()
{
    TestKeymap result;
    xkb_context* context = xkb_context_new(XKB_CONTEXT_NO_ENVIRONMENT_NAMES);
    const xkb_rule_names names{"evdev", "pc105", "us,de", "", ""};
    xkb_keymap* keymap = context ? xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS) : nullptr;
    if (keymap) {
        char* text = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
        result.text = QByteArray(text);
        free(text);
        result.capsMask = 1u << xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_CAPS);
        xkb_keymap_unref(keymap);
    }
    xkb_context_unref(context);
    return result;
}

class FakeSeat
{
public:
    FakeSeat()
    {
        display = wl_display_create();
        int fds[2];
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds) != 0) {
            qFatal("socketpair failed");
        }
        client = wl_client_create(display, fds[0]);
        clientDisplay = wl_display_connect_to_fd(fds[1]);
        wl_global_create(display, &wl_seat_interface, 5, this, bind);
        registry = wl_display_get_registry(clientDisplay);
        wl_registry_add_listener(registry, &registryListener, this);
        pump();
    }

    ~FakeSeat()
    {
        if (seat) {
            wl_seat_release(seat);
        }
        wl_registry_destroy(registry);
        wl_display_disconnect(clientDisplay);
        wl_display_destroy(display);
    }

    void pump()
    {
        for (int round = 0; round < 8; ++round) {
            wl_display_flush(clientDisplay);
            wl_event_loop_dispatch(wl_display_get_event_loop(display), 0);
            wl_display_flush_clients(display);
            while (wl_display_prepare_read(clientDisplay) != 0) {
                wl_display_dispatch_pending(clientDisplay);
            }
            pollfd fd{wl_display_get_fd(clientDisplay), POLLIN, 0};
            if (poll(&fd, 1, 0) > 0) {
                wl_display_read_events(clientDisplay);
            } else {
                wl_display_cancel_read(clientDisplay);
            }
            wl_display_dispatch_pending(clientDisplay);
        }
    }

    void sendKeymap(const QByteArray& text)
    {
        // The keymap travels as a file descriptor, NUL included.
        const int fd = memfd_create("keymap", MFD_CLOEXEC);
        const auto size = size_t(text.size()) + 1;
        if (fd < 0 || write(fd, text.constData(), size) != ssize_t(size)) {
            qFatal("cannot write the keymap");
        }
        wl_keyboard_send_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, uint32_t(size));
        close(fd);
    }

    void sendModifiers(uint32_t locked, uint32_t group)
    {
        wl_keyboard_send_modifiers(keyboard, ++serial, 0, 0, locked, group);
    }

    wl_display* display = nullptr;
    wl_client* client = nullptr;
    wl_display* clientDisplay = nullptr;
    wl_registry* registry = nullptr;
    wl_seat* seat = nullptr;
    wl_resource* keyboard = nullptr;
    int keyboardsReleased = 0;
    uint32_t serial = 0;

private:
    static void global(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t)
    {
        auto* self = static_cast<FakeSeat*>(data);
        if (std::strcmp(interface, wl_seat_interface.name) == 0) {
            self->seat = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, 5));
        }
    }

    static void globalRemove(void*, wl_registry*, uint32_t)
    {
    }

    static constexpr wl_registry_listener registryListener = {
        .global = global,
        .global_remove = globalRemove,
    };

    static void bind(wl_client* client, void* data, uint32_t version, uint32_t id)
    {
        auto* resource = wl_resource_create(client, &wl_seat_interface, int(version), id);
        wl_resource_set_implementation(resource, &seatImpl, data, nullptr);
    }

    static void getKeyboard(wl_client* client, wl_resource* seatResource, uint32_t id)
    {
        auto* self = static_cast<FakeSeat*>(wl_resource_get_user_data(seatResource));
        self->keyboard = wl_resource_create(client, &wl_keyboard_interface, wl_resource_get_version(seatResource), id);
        wl_resource_set_implementation(self->keyboard, &keyboardImpl, self, keyboardDestroyed);
    }

    static void keyboardDestroyed(wl_resource* resource)
    {
        auto* self = static_cast<FakeSeat*>(wl_resource_get_user_data(resource));
        ++self->keyboardsReleased;
        self->keyboard = nullptr;
    }

    static void getOther(wl_client*, wl_resource*, uint32_t)
    {
    }

    static void release(wl_client*, wl_resource* resource)
    {
        wl_resource_destroy(resource);
    }

    static constexpr struct wl_seat_interface seatImpl = {
        .get_pointer = getOther,
        .get_keyboard = getKeyboard,
        .get_touch = getOther,
        .release = release,
    };
    static constexpr struct wl_keyboard_interface keyboardImpl = {
        .release = release,
    };
};

} // namespace

class TestLockKeyboard : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        m_keymap = compileKeymap();
        if (m_keymap.text.isEmpty()) {
            QSKIP("no xkb data to compile a keymap from");
        }
    }

    void stateNamesTheEffectiveLayout()
    {
        KeyboardState state;
        QVERIFY(state.layoutName().isEmpty());
        QVERIFY(!state.capsLock());
        QVERIFY(state.setKeymap(m_keymap.text));
        QCOMPARE(state.layoutName(), QStringLiteral("English (US)"));
        state.updateModifiers(0, 0, 0, 1);
        QCOMPARE(state.layoutName(), QStringLiteral("German"));
    }

    void stateReadsCapsLockFromTheLockedMask()
    {
        KeyboardState state;
        QVERIFY(state.setKeymap(m_keymap.text));
        state.updateModifiers(0, 0, m_keymap.capsMask, 0);
        QVERIFY(state.capsLock());
        // Held, not locked: Shift-like, not the Caps Lock warning.
        state.updateModifiers(m_keymap.capsMask, 0, 0, 0);
        QVERIFY(!state.capsLock());
    }

    void aBrokenKeymapClearsTheState()
    {
        KeyboardState state;
        QVERIFY(state.setKeymap(m_keymap.text));
        QVERIFY(!state.setKeymap(QByteArrayLiteral("xkb_keymap { not a keymap")));
        QVERIFY(state.layoutName().isEmpty());
        QVERIFY(!state.capsLock());
    }

    void withoutASeatItIsInert()
    {
        LockKeyboard keyboard(static_cast<wl_seat*>(nullptr));
        QVERIFY(keyboard.layoutName().isEmpty());
        QVERIFY(!keyboard.capsLock());
    }

    /// The real path: the seat's keymap and modifiers events, in the order
    /// a compositor sends them when the lock surface takes focus with Caps
    /// Lock already on and the second layout active.
    void followsTheSeatsKeymapAndModifiers()
    {
        FakeSeat seat;
        QVERIFY(seat.seat);
        {
            LockKeyboard keyboard(seat.seat);
            QSignalSpy layout(&keyboard, &LockKeyboard::layoutChanged);
            QSignalSpy caps(&keyboard, &LockKeyboard::capsLockChanged);
            seat.pump();
            QVERIFY(seat.keyboard);

            seat.sendKeymap(m_keymap.text);
            seat.pump();
            QCOMPARE(keyboard.layoutName(), QStringLiteral("English (US)"));
            QCOMPARE(layout.count(), 1);

            seat.sendModifiers(m_keymap.capsMask, 1);
            seat.pump();
            QCOMPARE(keyboard.layoutName(), QStringLiteral("German"));
            QVERIFY(keyboard.capsLock());
            QCOMPARE(layout.count(), 2);
            QCOMPARE(caps.count(), 1);

            // The same state again announces nothing.
            seat.sendModifiers(m_keymap.capsMask, 1);
            seat.pump();
            QCOMPARE(layout.count(), 2);
            QCOMPARE(caps.count(), 1);

            seat.sendModifiers(0, 1);
            seat.pump();
            QVERIFY(!keyboard.capsLock());
            QCOMPARE(caps.count(), 2);
        }
        // The keyboard is released with the object.
        seat.pump();
        QCOMPARE(seat.keyboardsReleased, 1);
    }

private:
    TestKeymap m_keymap;
};
QTEST_GUILESS_MAIN(TestLockKeyboard)
#include "test_lockkeyboard.moc"
