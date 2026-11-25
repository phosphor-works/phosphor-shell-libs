// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <PhosphorServiceNightLight/NightLight.h>

#include <PhosphorServiceNightLight/ColorTemperature.h>
#include <PhosphorServiceNightLight/GammaBackend.h>

#include "schedule.h"
#include "waylandgammabackend.h"

#include <QElapsedTimer>
#include <QSet>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace PhosphorServiceNightLight {

namespace {

// How often a scheduled transition moves the screen on. A 30-minute
// transition from 6500 K to 4500 K moves about 2 K in that time.
constexpr qint64 kTransitionStepMs = 2000;
// The longest the night light waits before reading the clock again, so a
// clock change or a resume from suspend is noticed within a minute even if
// nobody calls refresh().
constexpr qint64 kMaximumWaitMs = 60 * 1000;
// One fade frame.
constexpr int kFadeFrameMs = 16;
// A change of the target by at most this much is applied at once rather than
// faded: the steps of a scheduled transition.
constexpr int kSnapKelvin = 50;

} // namespace

class NightLight::Private
{
public:
    Private(NightLight* owner, std::unique_ptr<GammaBackend> gamma, std::function<QDateTime()> now)
        : q(owner)
        , backend(std::move(gamma))
        , clock(std::move(now))
    {
    }

    Schedule::Phase phaseAt(const QDateTime& now) const;
    void update(bool scheduleChanged = false);
    void setCurrent(int kelvin);
    void syncBackend();
    void stepFade();
    void scheduleNext(const QDateTime& now, const Schedule::Phase& phase);

    NightLight* q;
    std::unique_ptr<GammaBackend> backend;
    std::function<QDateTime()> clock;

    // The app's settings.
    bool enabled = false;
    Mode mode = Mode::Hours;
    int dayTemperature = ColorTemperature::Neutral;
    int nightTemperature = NightLight::DefaultNightTemperature;
    QTime eveningStart{18, 0};
    QTime morningStart{6, 0};
    int transitionDuration = NightLight::DefaultTransitionDuration;
    bool hasLocation = false;
    double latitude = 0.0;
    double longitude = 0.0;
    int fadeDuration = NightLight::DefaultFadeDuration;

    // Inhibitions. The temporary ones also end at resumeAt.
    QSet<uint> cookies;
    QSet<uint> temporaryCookies;
    uint nextCookie = 1;
    QDateTime resumeAt;

    // Derived state, as last reported.
    bool available = false;
    bool inhibited = false;
    bool active = false;
    int target = ColorTemperature::Neutral;
    int current = ColorTemperature::Neutral;
    QDateTime nextTransition;

    QTimer scheduleTimer;
    QTimer fadeTimer;
    QElapsedTimer fadeClock;
    int fadeFrom = ColorTemperature::Neutral;
};

Schedule::Phase NightLight::Private::phaseAt(const QDateTime& now) const
{
    switch (mode) {
    case Mode::Constant: {
        Schedule::Phase always;
        always.night = 1.0;
        return always;
    }
    case Mode::Hours:
        return Schedule::evaluateHours({eveningStart, morningStart, transitionDuration}, now);
    case Mode::Location:
        return hasLocation ? Schedule::evaluateLocation(latitude, longitude, now) : Schedule::Phase{};
    }
    return {};
}

void NightLight::Private::update(bool scheduleChanged)
{
    const QDateTime now = clock();
    const Schedule::Phase phase = phaseAt(now);

    if (!temporaryCookies.isEmpty()) {
        if (scheduleChanged) {
            resumeAt = phase.nextTransition;
        }
        if (resumeAt.isValid() && now >= resumeAt) {
            for (const uint cookie : std::as_const(temporaryCookies)) {
                cookies.remove(cookie);
            }
            temporaryCookies.clear();
            resumeAt = {};
        }
    }

    const bool nowInhibited = !cookies.isEmpty();
    const int nowTarget = (!enabled || nowInhibited)
        ? int(ColorTemperature::Neutral)
        : Schedule::mixTemperature(dayTemperature, nightTemperature, phase.night);
    const bool nowActive = available && enabled && !nowInhibited && phase.night > 0.0;
    const QDateTime nowNext = phase.nextTransition;

    const bool inhibitedChanged = nowInhibited != inhibited;
    const bool targetChanged = nowTarget != target;
    const bool activeChanged = nowActive != active;
    const bool nextChanged = nowNext != nextTransition;
    inhibited = nowInhibited;
    target = nowTarget;
    active = nowActive;
    nextTransition = nowNext;

    // A new target starts a fade from what is on screen now (so reversing a
    // change mid-fade turns back smoothly), unless the step is small enough to
    // apply at once. An unchanged target leaves a running fade alone.
    if (targetChanged || !fadeTimer.isActive()) {
        if (fadeDuration > 0 && std::abs(target - current) > kSnapKelvin) {
            fadeFrom = current;
            fadeClock.start();
            fadeTimer.start();
        } else {
            fadeTimer.stop();
            setCurrent(target);
        }
    }
    syncBackend();
    scheduleNext(now, phase);

    if (inhibitedChanged) {
        Q_EMIT q->inhibitedChanged();
    }
    if (targetChanged) {
        Q_EMIT q->targetTemperatureChanged();
    }
    if (activeChanged) {
        Q_EMIT q->activeChanged();
    }
    if (nextChanged) {
        Q_EMIT q->nextTransitionChanged();
    }
}

void NightLight::Private::setCurrent(int kelvin)
{
    if (kelvin == current) {
        return;
    }
    current = kelvin;
    backend->setWhitePoint(whitePoint(kelvin));
    Q_EMIT q->currentTemperatureChanged();
}

void NightLight::Private::syncBackend()
{
    // Held while enabled, and while a fade back to neutral is still on screen.
    backend->setActive(enabled || current != ColorTemperature::Neutral);
}

void NightLight::Private::stepFade()
{
    const double progress = fadeDuration > 0 ? double(fadeClock.elapsed()) / double(fadeDuration) : 1.0;
    if (progress >= 1.0) {
        fadeTimer.stop();
        setCurrent(target);
    } else {
        setCurrent(Schedule::fadeTemperature(fadeFrom, target, progress));
    }
    syncBackend();
}

void NightLight::Private::scheduleNext(const QDateTime& now, const Schedule::Phase& phase)
{
    qint64 wait = phase.transitioning ? kTransitionStepMs : kMaximumWaitMs;
    const auto until = [&wait, &now](const QDateTime& moment) {
        if (moment.isValid()) {
            wait = std::min(wait, std::max<qint64>(1, now.msecsTo(moment)));
        }
    };
    until(phase.transitionEnd);
    until(phase.nextTransition);
    if (!temporaryCookies.isEmpty()) {
        until(resumeAt);
    }
    scheduleTimer.start(int(wait));
}

NightLight::NightLight(QObject* parent)
    : NightLight(
          std::make_unique<WaylandGammaBackend>(),
          [] {
              return QDateTime::currentDateTime();
          },
          parent)
{
}

NightLight::NightLight(std::unique_ptr<GammaBackend> backend, std::function<QDateTime()> clock, QObject* parent)
    : QObject(parent)
    , d(std::make_unique<Private>(this, std::move(backend), std::move(clock)))
{
    Q_ASSERT(d->backend);
    Q_ASSERT(d->clock);
    d->available = d->backend->isAvailable();
    connect(d->backend.get(), &GammaBackend::availableChanged, this, [this] {
        const bool available = d->backend->isAvailable();
        if (available == d->available) {
            return;
        }
        d->available = available;
        Q_EMIT availableChanged();
        d->update();
    });

    d->scheduleTimer.setSingleShot(true);
    connect(&d->scheduleTimer, &QTimer::timeout, this, [this] {
        d->update();
    });
    d->fadeTimer.setInterval(kFadeFrameMs);
    connect(&d->fadeTimer, &QTimer::timeout, this, [this] {
        d->stepFade();
    });

    d->update();
}

NightLight::~NightLight() = default;

bool NightLight::isAvailable() const
{
    return d->available;
}

bool NightLight::isEnabled() const
{
    return d->enabled;
}

void NightLight::setEnabled(bool enabled)
{
    if (enabled == d->enabled) {
        return;
    }
    d->enabled = enabled;
    Q_EMIT enabledChanged();
    d->update();
}

NightLight::Mode NightLight::mode() const
{
    return d->mode;
}

void NightLight::setMode(Mode mode)
{
    if (mode == d->mode) {
        return;
    }
    d->mode = mode;
    Q_EMIT modeChanged();
    d->update(true);
}

int NightLight::dayTemperature() const
{
    return d->dayTemperature;
}

void NightLight::setDayTemperature(int kelvin)
{
    kelvin = ColorTemperature::bound(kelvin);
    if (kelvin == d->dayTemperature) {
        return;
    }
    d->dayTemperature = kelvin;
    Q_EMIT dayTemperatureChanged();
    d->update();
}

int NightLight::nightTemperature() const
{
    return d->nightTemperature;
}

void NightLight::setNightTemperature(int kelvin)
{
    kelvin = ColorTemperature::bound(kelvin);
    if (kelvin == d->nightTemperature) {
        return;
    }
    d->nightTemperature = kelvin;
    Q_EMIT nightTemperatureChanged();
    d->update();
}

QTime NightLight::eveningStart() const
{
    return d->eveningStart;
}

QTime NightLight::morningStart() const
{
    return d->morningStart;
}

bool NightLight::setHours(QTime eveningStart, QTime morningStart)
{
    if (!eveningStart.isValid() || !morningStart.isValid()) {
        return false;
    }
    const QTime evening(eveningStart.hour(), eveningStart.minute());
    const QTime morning(morningStart.hour(), morningStart.minute());
    if (evening == morning) {
        return false;
    }
    if (evening == d->eveningStart && morning == d->morningStart) {
        return true;
    }
    d->eveningStart = evening;
    d->morningStart = morning;
    Q_EMIT hoursChanged();
    d->update(true);
    return true;
}

int NightLight::transitionDuration() const
{
    return d->transitionDuration;
}

void NightLight::setTransitionDuration(int seconds)
{
    seconds = std::clamp(seconds, 0, MaximumTransitionDuration);
    if (seconds == d->transitionDuration) {
        return;
    }
    d->transitionDuration = seconds;
    Q_EMIT transitionDurationChanged();
    d->update(true);
}

bool NightLight::hasLocation() const
{
    return d->hasLocation;
}

double NightLight::latitude() const
{
    return d->latitude;
}

double NightLight::longitude() const
{
    return d->longitude;
}

bool NightLight::setLocation(double latitude, double longitude)
{
    if (!std::isfinite(latitude) || !std::isfinite(longitude) || latitude < -90.0 || latitude > 90.0
        || longitude < -180.0 || longitude > 180.0) {
        return false;
    }
    if (d->hasLocation && latitude == d->latitude && longitude == d->longitude) {
        return true;
    }
    d->hasLocation = true;
    d->latitude = latitude;
    d->longitude = longitude;
    Q_EMIT locationChanged();
    d->update(true);
    return true;
}

void NightLight::clearLocation()
{
    if (!d->hasLocation) {
        return;
    }
    d->hasLocation = false;
    d->latitude = 0.0;
    d->longitude = 0.0;
    Q_EMIT locationChanged();
    d->update(true);
}

int NightLight::fadeDuration() const
{
    return d->fadeDuration;
}

void NightLight::setFadeDuration(int milliseconds)
{
    milliseconds = std::clamp(milliseconds, 0, MaximumFadeDuration);
    if (milliseconds == d->fadeDuration) {
        return;
    }
    d->fadeDuration = milliseconds;
    Q_EMIT fadeDurationChanged();
    if (milliseconds == 0 && d->fadeTimer.isActive()) {
        d->stepFade();
    }
}

bool NightLight::isInhibited() const
{
    return d->inhibited;
}

bool NightLight::isActive() const
{
    return d->active;
}

int NightLight::targetTemperature() const
{
    return d->target;
}

int NightLight::currentTemperature() const
{
    return d->current;
}

QDateTime NightLight::nextTransition() const
{
    return d->nextTransition;
}

uint NightLight::inhibit(bool untilNextTransition)
{
    while (d->nextCookie == 0 || d->cookies.contains(d->nextCookie)) {
        ++d->nextCookie;
    }
    const uint cookie = d->nextCookie++;
    d->cookies.insert(cookie);
    if (untilNextTransition) {
        if (d->temporaryCookies.isEmpty()) {
            d->resumeAt = d->phaseAt(d->clock()).nextTransition;
        }
        d->temporaryCookies.insert(cookie);
    }
    d->update();
    return cookie;
}

void NightLight::uninhibit(uint cookie)
{
    if (!d->cookies.remove(cookie)) {
        return;
    }
    d->temporaryCookies.remove(cookie);
    if (d->temporaryCookies.isEmpty()) {
        d->resumeAt = {};
    }
    d->update();
}

void NightLight::refresh()
{
    d->backend->retry();
    d->update();
}

} // namespace PhosphorServiceNightLight
