// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <memory>

struct xkb_context;
struct xkb_keymap;
struct xkb_state;

namespace PhosphorShellLock {

// What LockKeyboard reads from a wl_keyboard, without the wire: the keymap
// text from the keymap event and the four masks of the modifiers event,
// answered through xkbcommon.
class KeyboardState
{
public:
    KeyboardState();
    ~KeyboardState();

    // An xkb text keymap (XKB_KEYMAP_FORMAT_TEXT_V1). A keymap that does not
    // compile clears the state, and returns false.
    bool setKeymap(const QByteArray& text);
    void updateModifiers(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group);

    // The effective layout's name, empty without a keymap.
    [[nodiscard]] QString layoutName() const;
    // Caps Lock locked, as the Caps Lock key toggles it.
    [[nodiscard]] bool capsLock() const;

private:
    struct ContextDeleter
    {
        void operator()(xkb_context* context) const;
    };
    struct KeymapDeleter
    {
        void operator()(xkb_keymap* keymap) const;
    };
    struct StateDeleter
    {
        void operator()(xkb_state* state) const;
    };

    std::unique_ptr<xkb_context, ContextDeleter> m_context;
    std::unique_ptr<xkb_keymap, KeymapDeleter> m_keymap;
    std::unique_ptr<xkb_state, StateDeleter> m_state;
};
}
