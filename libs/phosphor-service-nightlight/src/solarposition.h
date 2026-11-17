// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

// Internal (not installed): where the sun is, computed locally.

#include <QDateTime>

namespace PhosphorServiceNightLight {

/// The sun's elevation above the horizon, in degrees, seen from @p latitude /
/// @p longitude (degrees, north and east positive) at @p when. Geometric, no
/// refraction. Accurate to about 0.01° for dates within a few centuries of
/// 2000, which is far finer than a night light needs.
///
/// The low-precision solar coordinates of the Astronomical Almanac (as given
/// in its section C, "Low precision formulas for the Sun") and the Greenwich
/// mean sidereal time of the IAU 1982 expression, linearised.
[[nodiscard]] double solarElevation(double latitude, double longitude, const QDateTime& when);

} // namespace PhosphorServiceNightLight
