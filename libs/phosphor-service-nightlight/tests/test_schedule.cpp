// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Schedule evaluation at injected moments: fixed hours, the sun's position and
// the location schedule built on it, and the fade and mix helpers. No timers,
// no sleeping.

#include "schedule.h"
#include "solarposition.h"

#include <QTest>
#include <QTimeZone>

#include <cmath>

using namespace PhosphorServiceNightLight;
using namespace PhosphorServiceNightLight::Schedule;

namespace {

QDateTime utc(int year, int month, int day, int hour, int minute, int second = 0)
{
    return QDateTime(QDate(year, month, day), QTime(hour, minute, second), QTimeZone::UTC);
}

// 20:00 to 06:00 with half-hour transitions, the shape most tests use.
const Hours kEveningToMorning{QTime(20, 0), QTime(6, 0), 30 * 60};

// Greenwich, London; and Longyearbyen, Svalbard, well inside the polar circle.
constexpr double kLondonLatitude = 51.5074;
constexpr double kLondonLongitude = -0.1278;
constexpr double kSvalbardLatitude = 78.22;
constexpr double kSvalbardLongitude = 15.65;

bool within(const QDateTime& actual, const QDateTime& expected, int seconds)
{
    return actual.isValid() && std::abs(actual.secsTo(expected)) <= seconds;
}

} // namespace

class ScheduleTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void hoursDay();
    void hoursEveningTransition();
    void hoursNightBeforeMidnight();
    void hoursNightAfterMidnight();
    void hoursMorningTransition();
    void hoursAtTransitionBoundaries();
    void hoursNightWithinOneDay();
    void hoursWithoutTransition();
    void hoursInvalidOrEqualIsDay();
    void hoursFollowTheClockZone();
    void transitionShortenedToFit();

    void solarElevationKnownValues();
    void elevationToNight();
    void locationDayAndNight();
    void locationEveningTwilight();
    void locationPolarNightAndDay();

    void fadeEasesBetweenEnds();
    void mixInterpolates();
};

void ScheduleTest::hoursDay()
{
    const Phase phase = evaluateHours(kEveningToMorning, utc(2026, 5, 4, 12, 0));
    QCOMPARE(phase.night, 0.0);
    QVERIFY(!phase.transitioning);
    QVERIFY(!phase.transitionEnd.isValid());
    QCOMPARE(phase.nextTransition, utc(2026, 5, 4, 20, 0));
}

void ScheduleTest::hoursEveningTransition()
{
    const Phase phase = evaluateHours(kEveningToMorning, utc(2026, 5, 4, 20, 15));
    QCOMPARE(phase.night, 0.5);
    QVERIFY(phase.transitioning);
    QCOMPARE(phase.transitionEnd, utc(2026, 5, 4, 20, 30));
    // The next transition is the morning one, not the end of this one.
    QCOMPARE(phase.nextTransition, utc(2026, 5, 5, 6, 0));
}

void ScheduleTest::hoursNightBeforeMidnight()
{
    const Phase phase = evaluateHours(kEveningToMorning, utc(2026, 5, 4, 23, 0));
    QCOMPARE(phase.night, 1.0);
    QVERIFY(!phase.transitioning);
    QCOMPARE(phase.nextTransition, utc(2026, 5, 5, 6, 0));
}

void ScheduleTest::hoursNightAfterMidnight()
{
    const Phase phase = evaluateHours(kEveningToMorning, utc(2026, 5, 4, 3, 0));
    QCOMPARE(phase.night, 1.0);
    QCOMPARE(phase.nextTransition, utc(2026, 5, 4, 6, 0));
}

void ScheduleTest::hoursMorningTransition()
{
    const Phase phase = evaluateHours(kEveningToMorning, utc(2026, 5, 4, 6, 20));
    QVERIFY(std::abs(phase.night - 1.0 / 3.0) < 1e-9);
    QVERIFY(phase.transitioning);
    QCOMPARE(phase.transitionEnd, utc(2026, 5, 4, 6, 30));
    QCOMPARE(phase.nextTransition, utc(2026, 5, 4, 20, 0));
}

void ScheduleTest::hoursAtTransitionBoundaries()
{
    // A transition starts at its own minute, at night 0, and the next one is
    // already the following transition.
    Phase phase = evaluateHours(kEveningToMorning, utc(2026, 5, 4, 20, 0));
    QCOMPARE(phase.night, 0.0);
    QVERIFY(phase.transitioning);
    QCOMPARE(phase.nextTransition, utc(2026, 5, 5, 6, 0));
    // And it is over at its end.
    phase = evaluateHours(kEveningToMorning, utc(2026, 5, 4, 20, 30));
    QCOMPARE(phase.night, 1.0);
    QVERIFY(!phase.transitioning);
    // One second before, almost there.
    phase = evaluateHours(kEveningToMorning, utc(2026, 5, 4, 20, 29, 59));
    QVERIFY(phase.night > 0.999 && phase.night < 1.0);
}

void ScheduleTest::hoursNightWithinOneDay()
{
    const Hours hours{QTime(1, 0), QTime(5, 0), 10 * 60};
    QCOMPARE(evaluateHours(hours, utc(2026, 5, 4, 0, 30)).night, 0.0);
    QCOMPARE(evaluateHours(hours, utc(2026, 5, 4, 0, 30)).nextTransition, utc(2026, 5, 4, 1, 0));
    QCOMPARE(evaluateHours(hours, utc(2026, 5, 4, 3, 0)).night, 1.0);
    QCOMPARE(evaluateHours(hours, utc(2026, 5, 4, 23, 0)).night, 0.0);
    QCOMPARE(evaluateHours(hours, utc(2026, 5, 4, 23, 0)).nextTransition, utc(2026, 5, 5, 1, 0));
}

void ScheduleTest::hoursWithoutTransition()
{
    const Hours hours{QTime(20, 0), QTime(6, 0), 0};
    Phase phase = evaluateHours(hours, utc(2026, 5, 4, 19, 59, 59));
    QCOMPARE(phase.night, 0.0);
    phase = evaluateHours(hours, utc(2026, 5, 4, 20, 0));
    QCOMPARE(phase.night, 1.0);
    QVERIFY(!phase.transitioning);
}

void ScheduleTest::hoursInvalidOrEqualIsDay()
{
    const Phase none{};
    QCOMPARE(evaluateHours({QTime(), QTime(6, 0), 60}, utc(2026, 5, 4, 23, 0)), none);
    QCOMPARE(evaluateHours({QTime(6, 0), QTime(6, 0), 60}, utc(2026, 5, 4, 23, 0)), none);
}

void ScheduleTest::hoursFollowTheClockZone()
{
    // The hours are wall-clock times in the moment's own zone.
    const QTimeZone plusTwo = QTimeZone::fromSecondsAheadOfUtc(2 * 60 * 60);
    const QDateTime noon(QDate(2026, 7, 1), QTime(12, 0), plusTwo);
    const Phase phase = evaluateHours(kEveningToMorning, noon);
    QCOMPARE(phase.nextTransition, QDateTime(QDate(2026, 7, 1), QTime(20, 0), plusTwo));
    QCOMPARE(phase.nextTransition.toUTC(), utc(2026, 7, 1, 18, 0));
    // 21:00 there is 19:00 UTC: night by the zone's clock.
    QCOMPARE(evaluateHours(kEveningToMorning, utc(2026, 7, 1, 19, 0).toTimeZone(plusTwo)).night, 1.0);
}

void ScheduleTest::transitionShortenedToFit()
{
    QCOMPARE(effectiveTransitionSeconds(QTime(20, 0), QTime(6, 0), 1800), 1800);
    // A 20-minute night: each transition at most half of it.
    QCOMPARE(effectiveTransitionSeconds(QTime(20, 0), QTime(20, 20), 1800), 600);
    // A 20-minute day.
    QCOMPARE(effectiveTransitionSeconds(QTime(20, 20), QTime(20, 0), 1800), 600);
    QCOMPARE(effectiveTransitionSeconds(QTime(20, 0), QTime(6, 0), -5), 0);
    QCOMPARE(effectiveTransitionSeconds(QTime(6, 0), QTime(6, 0), 1800), 0);

    // The shortened transitions do not overlap.
    const Hours tight{QTime(20, 0), QTime(20, 20), 1800};
    const Phase phase = evaluateHours(tight, utc(2026, 5, 4, 20, 10));
    QCOMPARE(phase.night, 1.0);
    QCOMPARE(phase.nextTransition, utc(2026, 5, 4, 20, 20));
}

void ScheduleTest::solarElevationKnownValues()
{
    // Solar noon at Greenwich on the June solstice: 90 - 51.51 + 23.44.
    QVERIFY(std::abs(solarElevation(kLondonLatitude, kLondonLongitude, utc(2024, 6, 21, 12, 2)) - 61.93) < 0.1);
    // Midnight: 90 - 51.51 - 23.44 below the horizon.
    QVERIFY(std::abs(solarElevation(kLondonLatitude, kLondonLongitude, utc(2024, 6, 21, 0, 0)) + 15.05) < 0.2);
    // The published sunset (21:21 BST) puts the sun at the sunset altitude.
    QVERIFY(std::abs(solarElevation(kLondonLatitude, kLondonLongitude, utc(2024, 6, 21, 20, 21, 30)) - DayElevation)
            < 0.2);
}

void ScheduleTest::elevationToNight()
{
    QCOMPARE(nightForElevation(10.0), 0.0);
    QCOMPARE(nightForElevation(DayElevation), 0.0);
    QCOMPARE(nightForElevation(NightElevation), 1.0);
    QCOMPARE(nightForElevation(-20.0), 1.0);
    QVERIFY(std::abs(nightForElevation((DayElevation + NightElevation) / 2.0) - 0.5) < 1e-9);
}

void ScheduleTest::locationDayAndNight()
{
    const Phase noon = evaluateLocation(kLondonLatitude, kLondonLongitude, utc(2024, 6, 21, 12, 0));
    QCOMPARE(noon.night, 0.0);
    QVERIFY(!noon.transitioning);
    // Sunset in London that evening: 20:21 UTC.
    QVERIFY2(within(noon.nextTransition, utc(2024, 6, 21, 20, 21, 38), 90),
             qPrintable(noon.nextTransition.toString(Qt::ISODate)));

    const Phase midnight = evaluateLocation(kLondonLatitude, kLondonLongitude, utc(2024, 6, 21, 0, 0));
    QCOMPARE(midnight.night, 1.0);
    // The morning transition starts at civil dawn, about 02:55 UTC.
    QVERIFY2(within(midnight.nextTransition, utc(2024, 6, 21, 2, 55, 26), 90),
             qPrintable(midnight.nextTransition.toString(Qt::ISODate)));
}

void ScheduleTest::locationEveningTwilight()
{
    // Between sunset (20:21) and the end of civil dusk (21:09).
    const Phase phase = evaluateLocation(kLondonLatitude, kLondonLongitude, utc(2024, 6, 21, 20, 45));
    QVERIFY(phase.transitioning);
    QVERIFY(phase.night > 0.2 && phase.night < 0.8);
    QVERIFY2(within(phase.transitionEnd, utc(2024, 6, 21, 21, 9, 24), 90),
             qPrintable(phase.transitionEnd.toString(Qt::ISODate)));
    // The next transition is the following dawn.
    QVERIFY2(within(phase.nextTransition, utc(2024, 6, 22, 2, 55, 30), 300),
             qPrintable(phase.nextTransition.toString(Qt::ISODate)));
}

void ScheduleTest::locationPolarNightAndDay()
{
    const Phase polarNight = evaluateLocation(kSvalbardLatitude, kSvalbardLongitude, utc(2024, 12, 21, 11, 0));
    QCOMPARE(polarNight.night, 1.0);
    QVERIFY(!polarNight.transitioning);
    QVERIFY(!polarNight.nextTransition.isValid());

    const Phase polarDay = evaluateLocation(kSvalbardLatitude, kSvalbardLongitude, utc(2024, 6, 21, 23, 0));
    QCOMPARE(polarDay.night, 0.0);
    QVERIFY(!polarDay.nextTransition.isValid());
}

void ScheduleTest::fadeEasesBetweenEnds()
{
    QCOMPARE(fadeTemperature(6500, 4500, 0.0), 6500);
    QCOMPARE(fadeTemperature(6500, 4500, 1.0), 4500);
    QCOMPARE(fadeTemperature(6500, 4500, 0.5), 5500);
    QCOMPARE(fadeTemperature(6500, 4500, -1.0), 6500);
    QCOMPARE(fadeTemperature(6500, 4500, 2.0), 4500);
    // Eased: slower than linear at the start.
    QVERIFY(fadeTemperature(6500, 4500, 0.1) > 6500 - 200);
    int previous = 6500;
    for (int step = 1; step <= 100; ++step) {
        const int value = fadeTemperature(6500, 4500, step / 100.0);
        QVERIFY(value <= previous);
        previous = value;
    }
}

void ScheduleTest::mixInterpolates()
{
    QCOMPARE(mixTemperature(6500, 4500, 0.0), 6500);
    QCOMPARE(mixTemperature(6500, 4500, 1.0), 4500);
    QCOMPARE(mixTemperature(6500, 4500, 0.25), 6000);
    QCOMPARE(mixTemperature(6500, 4500, 3.0), 4500);
}

QTEST_GUILESS_MAIN(ScheduleTest)
#include "test_schedule.moc"
