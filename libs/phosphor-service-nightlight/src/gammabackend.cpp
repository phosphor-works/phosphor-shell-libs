// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <PhosphorServiceNightLight/GammaBackend.h>

namespace PhosphorServiceNightLight {

GammaBackend::GammaBackend(QObject* parent)
    : QObject(parent)
{
}

// Out of line: anchors the vtable and gives AUTOMOC a translation unit.
GammaBackend::~GammaBackend() = default;

} // namespace PhosphorServiceNightLight
