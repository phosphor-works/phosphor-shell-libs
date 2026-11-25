// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "solarposition.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace PhosphorServiceNightLight {

namespace {

constexpr double kRadiansPerDegree = std::numbers::pi / 180.0;

double radians(double degrees)
{
    return degrees * kRadiansPerDegree;
}

double degrees(double radians)
{
    return radians / kRadiansPerDegree;
}

// @p value reduced to [0, period).
double wrap(double value, double period)
{
    const double reduced = std::fmod(value, period);
    return reduced < 0.0 ? reduced + period : reduced;
}

} // namespace

double solarElevation(double latitude, double longitude, const QDateTime& when)
{
    // Days since J2000.0 (2000-01-01 12:00 TT, taken as UT: the difference is
    // about a minute and moves the sun by a quarter of a degree of hour angle
    // at most, well inside what a night light needs).
    const double n = double(when.toMSecsSinceEpoch()) / 86400000.0 - 10957.5;

    const double meanLongitude = wrap(280.460 + 0.9856474 * n, 360.0);
    const double meanAnomaly = radians(wrap(357.528 + 0.9856003 * n, 360.0));
    const double eclipticLongitude =
        radians(meanLongitude + 1.915 * std::sin(meanAnomaly) + 0.020 * std::sin(2.0 * meanAnomaly));
    const double obliquity = radians(23.439 - 0.0000004 * n);

    const double rightAscension =
        std::atan2(std::cos(obliquity) * std::sin(eclipticLongitude), std::cos(eclipticLongitude));
    const double declination = std::asin(std::sin(obliquity) * std::sin(eclipticLongitude));

    const double siderealDegrees = wrap(280.46061837 + 360.98564736629 * n, 360.0);
    const double hourAngle = radians(siderealDegrees + longitude) - rightAscension;

    const double phi = radians(latitude);
    const double sine =
        std::sin(phi) * std::sin(declination) + std::cos(phi) * std::cos(declination) * std::cos(hourAngle);
    return degrees(std::asin(std::clamp(sine, -1.0, 1.0)));
}

} // namespace PhosphorServiceNightLight
