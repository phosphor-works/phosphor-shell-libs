// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

/**
 * @file PhosphorServiceUPower.h
 * @brief Umbrella header for the PhosphorServiceUPower library.
 *
 * PhosphorServiceUPower exposes `org.freedesktop.UPower` (system bus)
 * as Qt/QML-friendly types (`UPowerHost`, `UPowerDevice`,
 * `UPowerDeviceModel`) for Phosphor-based desktop shells. The library
 * has no UI of its own; shells consume the host's `onBattery` /
 * `displayDevice` properties and the model's per-device rows and
 * render however they like.
 */

#include <PhosphorServiceUPower/UPowerDevice.h>
#include <PhosphorServiceUPower/UPowerHost.h>
#include <PhosphorServiceUPower/UPowerDeviceModel.h>
#include <PhosphorServiceUPower/QmlRegistration.h>
