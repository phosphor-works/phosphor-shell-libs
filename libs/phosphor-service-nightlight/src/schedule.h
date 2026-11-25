// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

// Internal (not installed): when it is night. Pure functions of their inputs
// and an explicit moment, so they are tested with injected times and no timers.

#include <QDateTime>
#include <QTime>

namespace PhosphorServiceNightLight::Schedule {

/// How far into the night a moment is.
struct Phase
{
    /// 0 is day, 1 is night, anything between is part of a transition.
    double night = 0.0;
    /// Whether the moment is inside a transition.
    bool transitioning = false;
    /// When the current transition ends. Invalid outside a transition, and in
    /// location mode when the twilight does not end within the search horizon.
    QDateTime transitionEnd;
    /// When the next transition starts (strictly after the moment, not the
    /// current one). Invalid when none starts within the search horizon, as in
    /// a polar day or night.
    QDateTime nextTransition;

    bool operator==(const Phase&) const = default;
};

/// Fixed hours: the evening transition toward night starts at @c eveningStart,
/// the morning transition toward day at @c morningStart, each lasting
/// @c transitionSeconds (shortened by effectiveTransitionSeconds()).
struct Hours
{
    QTime eveningStart;
    QTime morningStart;
    int transitionSeconds = 0;
};

/// @p requested shortened so that neither transition runs into the next: at
/// most half of the shorter of the night (evening to morning) and the day.
/// 0 for invalid or equal times.
[[nodiscard]] int effectiveTransitionSeconds(QTime eveningStart, QTime morningStart, int requested);

/// The phase of @p now under @p hours. The times are read in @p now's own time
/// representation (local time in production, UTC in tests), so daylight-saving
/// changes move them with the wall clock. Invalid or equal times give a
/// permanent day.
[[nodiscard]] Phase evaluateHours(const Hours& hours, const QDateTime& now);

/// The sun's elevation at which the evening transition starts and the morning
/// one ends: the standard sunrise and sunset altitude, the upper limb on the
/// horizon after refraction.
inline constexpr double DayElevation = -0.833;
/// The elevation at which the evening transition ends and the morning one
/// starts: the end of civil twilight.
inline constexpr double NightElevation = -6.0;

/// 0 at or above DayElevation, 1 at or below NightElevation, linear between.
[[nodiscard]] double nightForElevation(double elevation);

/// The phase of @p now at @p latitude / @p longitude (degrees, north and east
/// positive), from the sun's computed elevation. Transitions are the
/// twilights: sunset to the end of civil dusk and the start of civil dawn to
/// sunrise. A polar day or night has no transitions. The search for the next
/// transition looks two days ahead.
[[nodiscard]] Phase evaluateLocation(double latitude, double longitude, const QDateTime& now);

/// The temperature @p progress (clamped to [0, 1]) of the way through a fade
/// from @p from to @p to, eased in and out (smoothstep), rounded to a kelvin.
[[nodiscard]] int fadeTemperature(int from, int to, double progress);

/// The temperature @p night (clamped to [0, 1]) of the way from @p day to
/// @p nightTemperature, rounded to a kelvin.
[[nodiscard]] int mixTemperature(int day, int nightTemperature, double night);

} // namespace PhosphorServiceNightLight::Schedule
