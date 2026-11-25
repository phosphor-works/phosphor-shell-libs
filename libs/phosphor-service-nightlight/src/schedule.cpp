// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "schedule.h"

#include "solarposition.h"

#include <QList>
#include <QTimeZone>

#include <algorithm>
#include <cmath>

namespace PhosphorServiceNightLight::Schedule {

namespace {

constexpr int kSecondsPerDay = 24 * 60 * 60;

// The location search: coarse steps, then bisection down to a second. A
// twilight lasts at least about twenty minutes (at the equator), so a
// two-minute step cannot step over one.
constexpr qint64 kSearchStepSeconds = 2 * 60;
constexpr qint64 kSearchHorizonSeconds = 2 * kSecondsPerDay;

enum class Light {
    Day,
    Twilight,
    Night,
};

Light classify(double night)
{
    if (night <= 0.0) {
        return Light::Day;
    }
    if (night >= 1.0) {
        return Light::Night;
    }
    return Light::Twilight;
}

Light lightAt(double latitude, double longitude, const QDateTime& when)
{
    return classify(nightForElevation(solarElevation(latitude, longitude, when)));
}

// The first moment after @p from whose light is no longer @p light, to the
// second. Invalid when the light holds for the whole search horizon.
QDateTime nextChange(double latitude, double longitude, const QDateTime& from, Light light)
{
    for (qint64 offset = kSearchStepSeconds; offset <= kSearchHorizonSeconds; offset += kSearchStepSeconds) {
        if (lightAt(latitude, longitude, from.addSecs(offset)) == light) {
            continue;
        }
        qint64 low = offset - kSearchStepSeconds; // still `light`
        qint64 high = offset; // changed
        while (high - low > 1) {
            const qint64 middle = low + (high - low) / 2;
            if (lightAt(latitude, longitude, from.addSecs(middle)) == light) {
                low = middle;
            } else {
                high = middle;
            }
        }
        return from.addSecs(high);
    }
    return {};
}

} // namespace

int effectiveTransitionSeconds(QTime eveningStart, QTime morningStart, int requested)
{
    if (!eveningStart.isValid() || !morningStart.isValid() || eveningStart == morningStart) {
        return 0;
    }
    const int night = (eveningStart.secsTo(morningStart) + kSecondsPerDay) % kSecondsPerDay;
    const int day = kSecondsPerDay - night;
    return std::clamp(requested, 0, std::min(night, day) / 2);
}

Phase evaluateHours(const Hours& hours, const QDateTime& now)
{
    if (!hours.eveningStart.isValid() || !hours.morningStart.isValid() || hours.eveningStart == hours.morningStart) {
        return {};
    }
    const int duration = effectiveTransitionSeconds(hours.eveningStart, hours.morningStart, hours.transitionSeconds);

    struct Event
    {
        QDateTime start;
        bool toNight = false;
    };
    // Yesterday through the day after tomorrow: always one event at or before
    // now and one after it, however the times and the transitions fall.
    QList<Event> events;
    const auto zone = now.timeRepresentation();
    for (int day = -1; day <= 2; ++day) {
        const QDate date = now.date().addDays(day);
        events.append({QDateTime(date, hours.eveningStart, zone), true});
        events.append({QDateTime(date, hours.morningStart, zone), false});
    }
    std::sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
        return a.start < b.start;
    });
    const auto after =
        std::upper_bound(events.cbegin(), events.cend(), now, [](const QDateTime& moment, const Event& e) {
            return moment < e.start;
        });
    const Event& current = *std::prev(after);

    Phase phase;
    phase.nextTransition = after->start;
    const QDateTime end = current.start.addSecs(duration);
    if (now < end) {
        const double progress = double(current.start.msecsTo(now)) / (double(duration) * 1000.0);
        phase.night = current.toNight ? progress : 1.0 - progress;
        phase.transitioning = true;
        phase.transitionEnd = end;
    } else {
        phase.night = current.toNight ? 1.0 : 0.0;
    }
    return phase;
}

double nightForElevation(double elevation)
{
    return std::clamp((DayElevation - elevation) / (DayElevation - NightElevation), 0.0, 1.0);
}

Phase evaluateLocation(double latitude, double longitude, const QDateTime& now)
{
    Phase phase;
    phase.night = nightForElevation(solarElevation(latitude, longitude, now));
    Light light = classify(phase.night);
    QDateTime from = now;
    if (light == Light::Twilight) {
        phase.transitioning = true;
        phase.transitionEnd = nextChange(latitude, longitude, now, Light::Twilight);
        if (!phase.transitionEnd.isValid()) {
            return phase;
        }
        from = phase.transitionEnd;
        light = lightAt(latitude, longitude, from);
    }
    phase.nextTransition = nextChange(latitude, longitude, from, light);
    return phase;
}

int fadeTemperature(int from, int to, double progress)
{
    const double t = std::clamp(progress, 0.0, 1.0);
    const double eased = t * t * (3.0 - 2.0 * t);
    return int(std::lround(from + (to - from) * eased));
}

int mixTemperature(int day, int nightTemperature, double night)
{
    const double t = std::clamp(night, 0.0, 1.0);
    return int(std::lround(day + (nightTemperature - day) * t));
}

} // namespace PhosphorServiceNightLight::Schedule
