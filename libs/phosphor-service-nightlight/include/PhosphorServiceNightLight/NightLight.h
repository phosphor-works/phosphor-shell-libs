// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <PhosphorServiceNightLight/phosphorservicenightlight_export.h>

#include <QDateTime>
#include <QObject>
#include <QTime>

#include <functional>
#include <memory>

namespace PhosphorServiceNightLight {

class GammaBackend;

/**
 * @brief A compositor-neutral night light.
 *
 * Warms the screen by setting every output's gamma ramps through
 * wlr-gamma-control-unstable-v1, so it works on any compositor that offers
 * that protocol (wlroots-based ones, among others) and needs no compositor
 * D-Bus API. The schedule is one of:
 *
 *   - `Constant`: the night temperature whenever the night light is enabled.
 *   - `Hours`: night from `eveningStart` to `morningStart` every day, with a
 *     gradual transition of `transitionDuration` seconds starting at each.
 *   - `Location`: night from sunset to sunrise at the location set with
 *     setLocation(), the sun's position computed locally. The evening
 *     transition runs from sunset to the end of civil twilight, the morning
 *     one from the start of civil twilight to sunrise.
 *
 * Changes the user makes (enabling, a new temperature, a new schedule, an
 * inhibition) fade over `fadeDuration` milliseconds. Scheduled transitions
 * follow the clock. Disabling fades back to neutral and then gives each output
 * its original ramps back.
 *
 * The library keeps no settings. The app loads its saved values, hands them
 * over through the setters and saves what the user changes. Every property has
 * a change signal, emitted only when the value changed.
 *
 * GUI thread only. Without a Wayland session (or with a compositor that does
 * not offer gamma control) the object constructs and works as a state model,
 * but `available` is false and nothing reaches the screen.
 */
class PHOSPHORSERVICENIGHTLIGHT_EXPORT NightLight : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(int dayTemperature READ dayTemperature WRITE setDayTemperature NOTIFY dayTemperatureChanged)
    Q_PROPERTY(int nightTemperature READ nightTemperature WRITE setNightTemperature NOTIFY nightTemperatureChanged)
    Q_PROPERTY(QTime eveningStart READ eveningStart NOTIFY hoursChanged)
    Q_PROPERTY(QTime morningStart READ morningStart NOTIFY hoursChanged)
    Q_PROPERTY(
        int transitionDuration READ transitionDuration WRITE setTransitionDuration NOTIFY transitionDurationChanged)
    Q_PROPERTY(bool hasLocation READ hasLocation NOTIFY locationChanged)
    Q_PROPERTY(double latitude READ latitude NOTIFY locationChanged)
    Q_PROPERTY(double longitude READ longitude NOTIFY locationChanged)
    Q_PROPERTY(int fadeDuration READ fadeDuration WRITE setFadeDuration NOTIFY fadeDurationChanged)
    Q_PROPERTY(bool inhibited READ isInhibited NOTIFY inhibitedChanged)
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(int targetTemperature READ targetTemperature NOTIFY targetTemperatureChanged)
    Q_PROPERTY(int currentTemperature READ currentTemperature NOTIFY currentTemperatureChanged)
    Q_PROPERTY(QDateTime nextTransition READ nextTransition NOTIFY nextTransitionChanged)

public:
    enum class Mode {
        Constant,
        Hours,
        Location,
    };
    Q_ENUM(Mode)

    /// The transition length used until the app sets one, in seconds.
    static constexpr int DefaultTransitionDuration = 30 * 60;
    /// The longest transition the library accepts, in seconds.
    static constexpr int MaximumTransitionDuration = 6 * 60 * 60;
    /// The fade length used until the app sets one, in milliseconds.
    static constexpr int DefaultFadeDuration = 1000;
    /// The longest fade the library accepts, in milliseconds.
    static constexpr int MaximumFadeDuration = 10000;
    /// The night temperature used until the app sets one, in kelvin.
    static constexpr int DefaultNightTemperature = 4500;

    /// The production night light: a wlr-gamma-control client on the session's
    /// Wayland display and the system clock.
    explicit NightLight(QObject* parent = nullptr);

    /// A night light driving @p backend, reading the time from @p clock (local
    /// time in production: the hours are wall-clock times in the clock's time
    /// representation). Takes ownership of @p backend. For another backend,
    /// and for tests.
    NightLight(std::unique_ptr<GammaBackend> backend, std::function<QDateTime()> clock, QObject* parent = nullptr);

    ~NightLight() override;

    /// Whether the compositor offers gamma control and at least one output
    /// accepts it. Until the night light is first enabled the outputs are not
    /// taken, so this is true when the protocol and an output are present; an
    /// output that refuses (another program holds its gamma, or it has no
    /// gamma ramps) stops counting from then on, until refresh().
    [[nodiscard]] bool isAvailable() const;

    /// The user's on/off switch. Default false.
    [[nodiscard]] bool isEnabled() const;
    void setEnabled(bool enabled);

    /// The schedule. Default Hours.
    [[nodiscard]] Mode mode() const;
    void setMode(Mode mode);

    /// The day temperature, kelvin, clamped to [ColorTemperature::Minimum,
    /// ColorTemperature::Maximum]. Default ColorTemperature::Neutral (6500 K,
    /// the screen unchanged).
    [[nodiscard]] int dayTemperature() const;
    void setDayTemperature(int kelvin);

    /// The night temperature, kelvin, clamped like the day temperature.
    /// Default DefaultNightTemperature.
    [[nodiscard]] int nightTemperature() const;
    void setNightTemperature(int kelvin);

    /// The Hours schedule's evening start (the transition toward night begins).
    /// Default 18:00.
    [[nodiscard]] QTime eveningStart() const;
    /// The Hours schedule's morning start (the transition toward day begins).
    /// Default 06:00.
    [[nodiscard]] QTime morningStart() const;
    /// Set both hours. Seconds are dropped. Returns false, changing nothing,
    /// when either time is invalid or the two are the same minute.
    Q_INVOKABLE bool setHours(QTime eveningStart, QTime morningStart);

    /// The requested length of each Hours transition, seconds, clamped to
    /// [0, MaximumTransitionDuration]. Default DefaultTransitionDuration. When
    /// the night or the day is short the transitions run shorter, at most half
    /// of the shorter of the two, so they never overlap.
    [[nodiscard]] int transitionDuration() const;
    void setTransitionDuration(int seconds);

    /// Whether a location is set. Without one the Location schedule is
    /// permanent day.
    [[nodiscard]] bool hasLocation() const;
    /// Degrees north. 0 without a location.
    [[nodiscard]] double latitude() const;
    /// Degrees east. 0 without a location.
    [[nodiscard]] double longitude() const;
    /// Set the location for the Location schedule. Returns false, changing
    /// nothing, when the latitude is outside [-90, 90] or the longitude
    /// outside [-180, 180] (or either is not a number).
    Q_INVOKABLE bool setLocation(double latitude, double longitude);
    /// Forget the location.
    Q_INVOKABLE void clearLocation();

    /// How long a user change fades, milliseconds, clamped to
    /// [0, MaximumFadeDuration]. 0 applies changes at once. Default
    /// DefaultFadeDuration.
    [[nodiscard]] int fadeDuration() const;
    void setFadeDuration(int milliseconds);

    /// Whether at least one inhibition is held. While inhibited the screen
    /// fades to neutral; `enabled` keeps the user's choice.
    [[nodiscard]] bool isInhibited() const;

    /// Whether the night temperature is at least partly in effect: available,
    /// enabled, not inhibited, and the schedule is in its night or in a
    /// transition.
    [[nodiscard]] bool isActive() const;

    /// The temperature the screen is heading for now, kelvin. Neutral while
    /// disabled or inhibited.
    [[nodiscard]] int targetTemperature() const;

    /// The temperature on the screen now, kelvin. Differs from the target
    /// during a fade.
    [[nodiscard]] int currentTemperature() const;

    /// When the next scheduled transition starts (the one after any transition
    /// in progress). Invalid in Constant mode, in Location mode without a
    /// location, and in a polar day or night.
    [[nodiscard]] QDateTime nextTransition() const;

    /// Hold the night light off. Returns a cookie for uninhibit(), never 0.
    /// With @p untilNextTransition the inhibition also ends by itself when the
    /// next scheduled transition starts (recomputed when the schedule changes),
    /// the shape of a "pause until tomorrow" action. In Constant mode there is
    /// no next transition, so it lasts until uninhibit().
    Q_INVOKABLE uint inhibit(bool untilNextTransition = false);

    /// Release the inhibition @p cookie. Unknown cookies are ignored.
    Q_INVOKABLE void uninhibit(uint cookie);

    /// Re-read the clock and try again on outputs whose gamma control failed.
    /// For a "Try again" action, and after the system resumes from suspend.
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void availableChanged();
    void enabledChanged();
    void modeChanged();
    void dayTemperatureChanged();
    void nightTemperatureChanged();
    void hoursChanged();
    void transitionDurationChanged();
    void locationChanged();
    void fadeDurationChanged();
    void inhibitedChanged();
    void activeChanged();
    void targetTemperatureChanged();
    void currentTemperatureChanged();
    void nextTransitionChanged();

private:
    Q_DISABLE_COPY_MOVE(NightLight)

    class Private;
    std::unique_ptr<Private> d;
};

} // namespace PhosphorServiceNightLight
