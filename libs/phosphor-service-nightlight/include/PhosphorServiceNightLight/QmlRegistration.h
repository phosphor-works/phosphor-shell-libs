// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <PhosphorServiceNightLight/phosphorservicenightlight_export.h>

namespace PhosphorServiceNightLight {

/// Register every PhosphorServiceNightLight QML type under the
/// `Phosphor.Service.NightLight` module at version 1.0. Idempotent on repeat
/// calls: internally guarded by `std::call_once` so a hot-reloading shell that
/// builds a fresh `QQmlEngine` per reload can call this from every engine setup
/// without triggering Qt's duplicate-registration warning.
///
/// Called from the consuming binary before any `QQmlEngine` loads a `.qml`
/// file.
PHOSPHORSERVICENIGHTLIGHT_EXPORT void registerQmlTypes();

} // namespace PhosphorServiceNightLight
