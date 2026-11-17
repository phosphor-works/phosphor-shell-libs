// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

/**
 * @file PhosphorServiceNightLight.h
 * @brief Umbrella header for the PhosphorServiceNightLight library.
 *
 * A compositor-neutral night light: warms the screen through
 * wlr-gamma-control-unstable-v1 on a constant, fixed-hours or sunrise/sunset
 * schedule, with fades and temporary inhibition. No Wayland type appears in
 * the public headers.
 *
 * Shells consume:
 *   - `NightLight`: the state model and controller.
 *   - `whitePoint()` / `gammaTable()`: the colour-temperature math, for previews.
 *   - `GammaBackend`: the seam to drive something other than the default
 *     wlr-gamma-control client.
 */

#include <PhosphorServiceNightLight/ColorTemperature.h>
#include <PhosphorServiceNightLight/GammaBackend.h>
#include <PhosphorServiceNightLight/NightLight.h>
#include <PhosphorServiceNightLight/QmlRegistration.h>
