// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// NightLight's state transitions, driven through a fake GammaBackend and an
// injected clock: what reaches the backend, which properties change and which
// signals fire. Time moves by assigning the clock and calling refresh(); no
// test waits on a timer.

#include <PhosphorServiceNightLight/ColorTemperature.h>
#include <PhosphorServiceNightLight/GammaBackend.h>
#include <PhosphorServiceNightLight/NightLight.h>
#include <PhosphorServiceNightLight/QmlRegistration.h>

#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>

#include <cmath>

using namespace PhosphorServiceNightLight;

// At file scope, not in an anonymous namespace: the test class below holds a
// pointer to it.
class FakeBackend : public GammaBackend
{
    Q_OBJECT

public:
    bool isAvailable() const override
    {
        return available;
    }
    void setActive(bool value) override
    {
        if (value != active) {
            active = value;
            ++activations;
        }
    }
    void setWhitePoint(const WhitePoint& value) override
    {
        white = value;
    }
    void retry() override
    {
        ++retries;
    }

    void setAvailable(bool value)
    {
        available = value;
        Q_EMIT availableChanged();
    }

    bool available = true;
    bool active = false;
    int activations = 0;
    int retries = 0;
    WhitePoint white;
};

namespace {

QDateTime utc(int day, int hour, int minute)
{
    return QDateTime(QDate(2026, 5, day), QTime(hour, minute), QTimeZone::UTC);
}

} // namespace

class NightLightTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();

    void startsDisabledAndNeutral();
    void enableConstantAppliesNightTemperature();
    void disableReleasesTheOutputs();
    void signalsOnlyOnChange();
    void temperaturesClamp();
    void hoursFollowTheClock();
    void hoursValidation();
    void transitionDurationShortensTransitions();
    void locationScheduleNeedsALocation();
    void inhibitHoldsNeutral();
    void temporaryInhibitEndsAtTheNextTransition();
    void temporaryInhibitFollowsScheduleChanges();
    void temporaryInhibitInConstantModeLasts();
    void availabilityGatesActive();
    void refreshRetriesTheBackend();
    void fadeRunsTowardTheTarget();
    void fadeOutKeepsTheOutputsUntilNeutral();
    void reversingAFadeStartsFromTheScreen();

    void productionConstructsWithoutWayland();
    void registerQmlTypesIsIdempotent();

private:
    QDateTime m_now;
    FakeBackend* m_backend = nullptr;
    std::unique_ptr<NightLight> m_light;
};

void NightLightTest::init()
{
    m_now = utc(4, 12, 0);
    auto backend = std::make_unique<FakeBackend>();
    m_backend = backend.get();
    m_light = std::make_unique<NightLight>(std::move(backend), [this] {
        return m_now;
    });
    m_light->setFadeDuration(0);
}

void NightLightTest::cleanup()
{
    m_light.reset();
    m_backend = nullptr;
}

void NightLightTest::startsDisabledAndNeutral()
{
    QVERIFY(m_light->isAvailable());
    QVERIFY(!m_light->isEnabled());
    QCOMPARE(m_light->mode(), NightLight::Mode::Hours);
    QCOMPARE(m_light->dayTemperature(), ColorTemperature::Neutral);
    QCOMPARE(m_light->nightTemperature(), NightLight::DefaultNightTemperature);
    QCOMPARE(m_light->eveningStart(), QTime(18, 0));
    QCOMPARE(m_light->morningStart(), QTime(6, 0));
    QCOMPARE(m_light->targetTemperature(), ColorTemperature::Neutral);
    QCOMPARE(m_light->currentTemperature(), ColorTemperature::Neutral);
    QVERIFY(!m_light->isActive());
    QVERIFY(!m_light->isInhibited());
    QVERIFY(!m_backend->active);
    // The schedule is evaluated even while disabled.
    QCOMPARE(m_light->nextTransition(), utc(4, 18, 0));
}

void NightLightTest::enableConstantAppliesNightTemperature()
{
    m_light->setMode(NightLight::Mode::Constant);
    QSignalSpy active(m_light.get(), &NightLight::activeChanged);
    m_light->setEnabled(true);
    QVERIFY(m_backend->active);
    QCOMPARE(m_light->targetTemperature(), 4500);
    QCOMPARE(m_light->currentTemperature(), 4500);
    QCOMPARE(m_backend->white, whitePoint(4500));
    QVERIFY(m_light->isActive());
    QCOMPARE(active.count(), 1);
    QVERIFY(!m_light->nextTransition().isValid());

    m_light->setNightTemperature(3000);
    QCOMPARE(m_light->currentTemperature(), 3000);
    QCOMPARE(m_backend->white, whitePoint(3000));
}

void NightLightTest::disableReleasesTheOutputs()
{
    m_light->setMode(NightLight::Mode::Constant);
    m_light->setEnabled(true);
    m_light->setEnabled(false);
    QCOMPARE(m_light->targetTemperature(), ColorTemperature::Neutral);
    QCOMPARE(m_light->currentTemperature(), ColorTemperature::Neutral);
    QCOMPARE(m_backend->white, WhitePoint{});
    QVERIFY(!m_backend->active);
    QVERIFY(!m_light->isActive());
}

void NightLightTest::signalsOnlyOnChange()
{
    QSignalSpy enabled(m_light.get(), &NightLight::enabledChanged);
    QSignalSpy mode(m_light.get(), &NightLight::modeChanged);
    QSignalSpy night(m_light.get(), &NightLight::nightTemperatureChanged);
    QSignalSpy hours(m_light.get(), &NightLight::hoursChanged);
    QSignalSpy location(m_light.get(), &NightLight::locationChanged);
    m_light->setEnabled(false);
    m_light->setMode(NightLight::Mode::Hours);
    m_light->setNightTemperature(NightLight::DefaultNightTemperature);
    QVERIFY(m_light->setHours(QTime(18, 0), QTime(6, 0)));
    m_light->clearLocation();
    QCOMPARE(enabled.count(), 0);
    QCOMPARE(mode.count(), 0);
    QCOMPARE(night.count(), 0);
    QCOMPARE(hours.count(), 0);
    QCOMPARE(location.count(), 0);
    m_light->setEnabled(true);
    m_light->setEnabled(true);
    QVERIFY(m_light->setLocation(51.5, -0.13));
    QVERIFY(m_light->setLocation(51.5, -0.13));
    QCOMPARE(enabled.count(), 1);
    QCOMPARE(location.count(), 1);
}

void NightLightTest::temperaturesClamp()
{
    m_light->setNightTemperature(200);
    QCOMPARE(m_light->nightTemperature(), ColorTemperature::Minimum);
    m_light->setDayTemperature(50000);
    QCOMPARE(m_light->dayTemperature(), ColorTemperature::Maximum);
    m_light->setTransitionDuration(-3);
    QCOMPARE(m_light->transitionDuration(), 0);
    m_light->setFadeDuration(1000000);
    QCOMPARE(m_light->fadeDuration(), NightLight::MaximumFadeDuration);
}

void NightLightTest::hoursFollowTheClock()
{
    QVERIFY(m_light->setHours(QTime(20, 0), QTime(6, 0)));
    m_light->setEnabled(true);
    // Midday: enabled but day, the outputs held at the day temperature.
    QVERIFY(m_backend->active);
    QVERIFY(!m_light->isActive());
    QCOMPARE(m_light->targetTemperature(), ColorTemperature::Neutral);
    QCOMPARE(m_light->nextTransition(), utc(4, 20, 0));

    QSignalSpy next(m_light.get(), &NightLight::nextTransitionChanged);
    m_now = utc(4, 20, 15);
    m_light->refresh();
    QVERIFY(m_light->isActive());
    QCOMPARE(m_light->targetTemperature(), 5500);
    QCOMPARE(m_light->currentTemperature(), 5500);
    QCOMPARE(m_light->nextTransition(), utc(5, 6, 0));
    QCOMPARE(next.count(), 1);

    m_now = utc(5, 1, 0);
    m_light->refresh();
    QCOMPARE(m_light->currentTemperature(), 4500);

    m_now = utc(5, 7, 0);
    m_light->refresh();
    QVERIFY(!m_light->isActive());
    QCOMPARE(m_light->currentTemperature(), ColorTemperature::Neutral);

    // A day temperature other than neutral applies during the day.
    m_light->setDayTemperature(6000);
    QCOMPARE(m_light->currentTemperature(), 6000);
}

void NightLightTest::hoursValidation()
{
    QSignalSpy hours(m_light.get(), &NightLight::hoursChanged);
    QVERIFY(!m_light->setHours(QTime(), QTime(6, 0)));
    QVERIFY(!m_light->setHours(QTime(6, 0), QTime(6, 0, 30)));
    QCOMPARE(hours.count(), 0);
    QVERIFY(m_light->setHours(QTime(21, 30, 45), QTime(7, 15, 10)));
    QCOMPARE(m_light->eveningStart(), QTime(21, 30));
    QCOMPARE(m_light->morningStart(), QTime(7, 15));
    QCOMPARE(hours.count(), 1);
}

void NightLightTest::transitionDurationShortensTransitions()
{
    QVERIFY(m_light->setHours(QTime(20, 0), QTime(6, 0)));
    m_light->setEnabled(true);
    m_light->setTransitionDuration(0);
    m_now = utc(4, 20, 0);
    m_light->refresh();
    QCOMPARE(m_light->currentTemperature(), 4500);
    m_light->setTransitionDuration(3600);
    QCOMPARE(m_light->currentTemperature(), ColorTemperature::Neutral);
    m_now = utc(4, 20, 30);
    m_light->refresh();
    QCOMPARE(m_light->currentTemperature(), 5500);
}

void NightLightTest::locationScheduleNeedsALocation()
{
    m_now = QDateTime(QDate(2024, 6, 21), QTime(23, 0), QTimeZone::UTC);
    m_light->setEnabled(true);
    m_light->setMode(NightLight::Mode::Location);
    QVERIFY(!m_light->hasLocation());
    QVERIFY(!m_light->isActive());
    QVERIFY(!m_light->nextTransition().isValid());

    QVERIFY(!m_light->setLocation(91.0, 0.0));
    QVERIFY(!m_light->setLocation(0.0, -180.5));
    QVERIFY(!m_light->setLocation(std::nan(""), 0.0));
    QVERIFY(!m_light->hasLocation());

    QVERIFY(m_light->setLocation(51.5074, -0.1278));
    QVERIFY(m_light->hasLocation());
    QVERIFY(m_light->isActive());
    QCOMPARE(m_light->currentTemperature(), 4500);
    QVERIFY(m_light->nextTransition().isValid());

    m_light->clearLocation();
    QVERIFY(!m_light->isActive());
    QCOMPARE(m_light->latitude(), 0.0);
}

void NightLightTest::inhibitHoldsNeutral()
{
    m_light->setMode(NightLight::Mode::Constant);
    m_light->setEnabled(true);
    QSignalSpy inhibited(m_light.get(), &NightLight::inhibitedChanged);

    const uint first = m_light->inhibit();
    const uint second = m_light->inhibit();
    QVERIFY(first != 0 && second != 0 && first != second);
    QVERIFY(m_light->isInhibited());
    QVERIFY(m_light->isEnabled());
    QVERIFY(!m_light->isActive());
    QCOMPARE(m_light->currentTemperature(), ColorTemperature::Neutral);
    // Still holding the outputs: a pause, not an off switch.
    QVERIFY(m_backend->active);

    m_light->uninhibit(first);
    QVERIFY(m_light->isInhibited());
    m_light->uninhibit(12345);
    m_light->uninhibit(first);
    QVERIFY(m_light->isInhibited());
    m_light->uninhibit(second);
    QVERIFY(!m_light->isInhibited());
    QCOMPARE(m_light->currentTemperature(), 4500);
    QCOMPARE(inhibited.count(), 2);
}

void NightLightTest::temporaryInhibitEndsAtTheNextTransition()
{
    QVERIFY(m_light->setHours(QTime(20, 0), QTime(6, 0)));
    m_light->setEnabled(true);
    m_now = utc(4, 23, 0);
    m_light->refresh();
    QVERIFY(m_light->isActive());

    QSignalSpy inhibited(m_light.get(), &NightLight::inhibitedChanged);
    m_light->inhibit(true);
    QVERIFY(m_light->isInhibited());
    m_now = utc(5, 5, 59);
    m_light->refresh();
    QVERIFY(m_light->isInhibited());
    m_now = utc(5, 6, 0);
    m_light->refresh();
    QVERIFY(!m_light->isInhibited());
    QCOMPARE(inhibited.count(), 2);
    // Resumed at the start of the morning transition.
    QCOMPARE(m_light->currentTemperature(), 4500);
}

void NightLightTest::temporaryInhibitFollowsScheduleChanges()
{
    QVERIFY(m_light->setHours(QTime(20, 0), QTime(6, 0)));
    m_light->setEnabled(true);
    m_now = utc(4, 23, 0);
    m_light->refresh();
    m_light->inhibit(true);
    // A new morning moves the resume time with it.
    QVERIFY(m_light->setHours(QTime(20, 0), QTime(8, 0)));
    m_now = utc(5, 7, 0);
    m_light->refresh();
    QVERIFY(m_light->isInhibited());
    m_now = utc(5, 8, 0);
    m_light->refresh();
    QVERIFY(!m_light->isInhibited());
}

void NightLightTest::temporaryInhibitInConstantModeLasts()
{
    m_light->setMode(NightLight::Mode::Constant);
    m_light->setEnabled(true);
    const uint cookie = m_light->inhibit(true);
    m_now = utc(9, 12, 0);
    m_light->refresh();
    QVERIFY(m_light->isInhibited());
    m_light->uninhibit(cookie);
    QVERIFY(!m_light->isInhibited());
}

void NightLightTest::availabilityGatesActive()
{
    m_light->setMode(NightLight::Mode::Constant);
    m_light->setEnabled(true);
    QVERIFY(m_light->isActive());

    QSignalSpy available(m_light.get(), &NightLight::availableChanged);
    QSignalSpy active(m_light.get(), &NightLight::activeChanged);
    m_backend->setAvailable(false);
    QVERIFY(!m_light->isAvailable());
    QVERIFY(!m_light->isActive());
    QVERIFY(m_light->isEnabled());
    QCOMPARE(available.count(), 1);
    QCOMPARE(active.count(), 1);

    m_backend->setAvailable(false);
    QCOMPARE(available.count(), 1);

    m_backend->setAvailable(true);
    QVERIFY(m_light->isActive());
    QCOMPARE(available.count(), 2);
}

void NightLightTest::refreshRetriesTheBackend()
{
    m_light->refresh();
    m_light->refresh();
    QCOMPARE(m_backend->retries, 2);
}

void NightLightTest::fadeRunsTowardTheTarget()
{
    m_light->setMode(NightLight::Mode::Constant);
    m_light->setFadeDuration(5000);
    QSignalSpy current(m_light.get(), &NightLight::currentTemperatureChanged);
    m_light->setEnabled(true);
    // The target moves at once; the screen fades toward it.
    QCOMPARE(m_light->targetTemperature(), 4500);
    QCOMPARE(m_light->currentTemperature(), ColorTemperature::Neutral);
    QVERIFY(m_backend->active);
    QCOMPARE(current.count(), 0);
    // Cutting the fade short lands on the target.
    m_light->setFadeDuration(0);
    QCOMPARE(m_light->currentTemperature(), 4500);
    QCOMPARE(m_backend->white, whitePoint(4500));
    QCOMPARE(current.count(), 1);
}

void NightLightTest::fadeOutKeepsTheOutputsUntilNeutral()
{
    m_light->setMode(NightLight::Mode::Constant);
    m_light->setEnabled(true);
    m_light->setFadeDuration(5000);
    m_light->setEnabled(false);
    // Fading back: still holding the outputs, or the compositor would snap
    // them to their original ramps.
    QCOMPARE(m_light->targetTemperature(), ColorTemperature::Neutral);
    QCOMPARE(m_light->currentTemperature(), 4500);
    QVERIFY(m_backend->active);
    m_light->setFadeDuration(0);
    QCOMPARE(m_light->currentTemperature(), ColorTemperature::Neutral);
    QVERIFY(!m_backend->active);
}

void NightLightTest::reversingAFadeStartsFromTheScreen()
{
    m_light->setMode(NightLight::Mode::Constant);
    m_light->setFadeDuration(5000);
    m_light->setEnabled(true);
    QCOMPARE(m_light->currentTemperature(), ColorTemperature::Neutral);
    // Switched off again before the fade moved the screen: nothing to fade
    // back from, so the outputs are let go at once.
    m_light->setEnabled(false);
    QCOMPARE(m_light->currentTemperature(), ColorTemperature::Neutral);
    QVERIFY(!m_backend->active);
    // A later fade-duration change has no stale fade to finish.
    m_light->setFadeDuration(0);
    QCOMPARE(m_light->currentTemperature(), ColorTemperature::Neutral);
}

void NightLightTest::productionConstructsWithoutWayland()
{
    // No QGuiApplication and no Wayland display: the object works as a state
    // model and reports itself unavailable.
    NightLight light;
    QVERIFY(!light.isAvailable());
    light.setMode(NightLight::Mode::Constant);
    light.setFadeDuration(0);
    light.setEnabled(true);
    QCOMPARE(light.currentTemperature(), NightLight::DefaultNightTemperature);
    QVERIFY(!light.isActive());
}

void NightLightTest::registerQmlTypesIsIdempotent()
{
    registerQmlTypes();
    registerQmlTypes();
}

QTEST_GUILESS_MAIN(NightLightTest)
#include "test_nightlight.moc"
