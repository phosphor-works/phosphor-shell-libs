// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later
#include "keyboardstate.h"

#include <xkbcommon/xkbcommon.h>

namespace PhosphorShellLock {

void KeyboardState::ContextDeleter::operator()(xkb_context* context) const
{
    xkb_context_unref(context);
}

void KeyboardState::KeymapDeleter::operator()(xkb_keymap* keymap) const
{
    xkb_keymap_unref(keymap);
}

void KeyboardState::StateDeleter::operator()(xkb_state* state) const
{
    xkb_state_unref(state);
}

KeyboardState::KeyboardState()
    // No include paths or environment: a keymap from the compositor is
    // complete, nothing is looked up.
    : m_context(xkb_context_new(xkb_context_flags(XKB_CONTEXT_NO_DEFAULT_INCLUDES | XKB_CONTEXT_NO_ENVIRONMENT_NAMES)))
{
}

KeyboardState::~KeyboardState() = default;

bool KeyboardState::setKeymap(const QByteArray& text)
{
    m_state.reset();
    m_keymap.reset();
    if (!m_context) {
        return false;
    }
    m_keymap.reset(xkb_keymap_new_from_string(m_context.get(), text.constData(), XKB_KEYMAP_FORMAT_TEXT_V1,
                                              XKB_KEYMAP_COMPILE_NO_FLAGS));
    if (!m_keymap) {
        return false;
    }
    m_state.reset(xkb_state_new(m_keymap.get()));
    return bool(m_state);
}

void KeyboardState::updateModifiers(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group)
{
    if (m_state) {
        xkb_state_update_mask(m_state.get(), depressed, latched, locked, 0, 0, group);
    }
}

QString KeyboardState::layoutName() const
{
    if (!m_state) {
        return {};
    }
    const xkb_layout_index_t layout = xkb_state_serialize_layout(m_state.get(), XKB_STATE_LAYOUT_EFFECTIVE);
    return QString::fromUtf8(xkb_keymap_layout_get_name(m_keymap.get(), layout));
}

bool KeyboardState::capsLock() const
{
    return m_state && xkb_state_mod_name_is_active(m_state.get(), XKB_MOD_NAME_CAPS, XKB_STATE_MODS_LOCKED) > 0;
}
}
