<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-service-nightlight

A compositor-neutral night light for Phosphor-based desktop shells. It makes the screen warmer in the evening.

## Responsibility

The library sets the gamma ramps of each output through `wlr-gamma-control-unstable-v1`. It needs no compositor
D-Bus API, so it works on each compositor that offers that protocol. It owns the state that a night-light UI shows
and the schedule that drives it. It provides no UI.

The library does not own the user's settings. The app loads them, gives them to the library through setters and
saves what the user changes.

- It converts a color temperature to gamma ramps with a black-body model.
- It has three schedules: constant, fixed hours, and sunset to sunrise at a location. It calculates the sun times
  locally, with no network or location service.
- It makes scheduled transitions gradually, and uses a short fade for each change that the user makes.
- It supports inhibition, which includes a "pause until the next transition" form.

## Key types

| Type | Role |
| --- | --- |
| `NightLight` | The state model and controller. Settings: `enabled`, `mode` (`Constant`, `Hours`, `Location`), `dayTemperature`, `nightTemperature`, `eveningStart` / `morningStart` (`setHours()`), `transitionDuration`, the location (`setLocation()`), `fadeDuration`. State: `available`, `inhibited`, `active`, `targetTemperature`, `currentTemperature`, `nextTransition`. Actions: `inhibit(untilNextTransition)`, `uninhibit(cookie)`, `refresh()`. |
| `GammaBackend` | The interface that `NightLight` drives. The default is the `wlr-gamma-control` client. Tests and the backends of other compositors plug in through the injecting constructor of `NightLight`. |
| `whitePoint()`, `gammaTable()` | The color-temperature math, exported for previews. |

## Use

```cpp
#include <PhosphorServiceNightLight/NightLight.h>

using PhosphorServiceNightLight::NightLight;

auto* night = new NightLight(app);
// Hand over the saved settings, then switch on.
night->setMode(NightLight::Mode::Hours);
night->setHours(QTime(20, 0), QTime(6, 30));
night->setNightTemperature(4000);
night->setEnabled(true);
// Save what the user changes.
QObject::connect(night, &NightLight::nightTemperatureChanged, settings, [=] {
    settings->setNightTemperature(night->nightTemperature());
});
```

A "pause until tomorrow" button holds a temporary inhibition. The inhibition ends automatically when the next
scheduled transition starts:

```cpp
m_pause = night->inhibit(true);
// ...or ended early by the user:
night->uninhibit(m_pause);
```

`registerQmlTypes()` registers `NightLight` as an instantiable type in the `Phosphor.Service.NightLight` QML module.

## Tests

| Area | How it is tested |
| --- | --- |
| The schedule, the color math and the state transitions of `NightLight` | Unit tests with injected times and a fake backend. |
| The `wlr-gamma-control` client | No automated test. `examples/phosphor-service-nightlight-cli` drives it manually inside a compositor. |

Under a headless cage (wlroots 0.20), the CLI binds the manager and the output. It handles the `failed` event of the
headless output and the release on exit. To set ramps on an output that accepts them (`gamma_size`, then
`set_gamma`), you need a compositor on real hardware.

## Design notes

- **One gamma owner per output.** The protocol gives one client exclusive control of the gamma of an output. The
  night light takes the outputs while it is enabled. It releases them when it is disabled and the fade back to
  neutral is complete. The compositor then restores the original ramps of each output. If another program holds an
  output (gammastep, wlsunset), the compositor refuses with `failed`. That output then does not count toward
  `available` until `refresh()`.
- **Availability.** `available` is true when the compositor offers `zwlr_gamma_control_manager_v1` and at least one
  output has not refused. The night light does not take the outputs before the first enable, so it learns of a
  refusal only then. KWin does not offer the protocol, so the night light is unavailable on KWin.
- **Color.** `whitePoint()` integrates Planck's law over the CIE 1931 2° observer (the multi-lobe Gaussian fit of
  Wyman, Sloan and Shirley, JCGT 2(2), 2013). It converts XYZ to linear sRGB (IEC 61966-2-1) and divides by the
  same value at 6500 K. Thus 6500 K does not change the screen, and the curve is continuous on both sides of it.
  The factors are gamma-encoded with exponent 2.2 before they scale the identity ramp. Thus a factor dims the light,
  not the signal. The code comes from the published formulas. It uses no code or table from redshift, gammastep or
  wlsunset.
- **Sun.** The elevation of the sun comes from the low-precision solar coordinates of the Astronomical Almanac and
  the linearized Greenwich sidereal time. Day is the sun at or above -0.833° (the sunrise and sunset altitude).
  Night is the sun at or below -6° (the end of civil twilight). Between the two, the change is linear. A polar day
  or a polar night has no transitions.
- **Time.** `NightLight` reads the time from an injected clock (the system clock in production). It calculates
  again at each transition, every two seconds during a transition, and at least once a minute. Thus it notices clock
  changes and resume from suspend. `refresh()` calculates again immediately.
- **Threads.** Use the GUI thread only. The Wayland objects live on the default queue of the Qt display (reached
  through the public `QNativeInterface::QWaylandApplication`). Qt dispatches that queue on the GUI thread.

## Dependencies

- Qt6 (Core, Gui, Qml), libwayland-client and wayland-scanner.
- `protocols/wlr-gamma-control-unstable-v1.xml`, copied unchanged from
  [wlr-protocols](https://gitlab.freedesktop.org/wlroots/wlr-protocols) at commit
  `1106a4a1c57562ddd438c547da4c0d6dc150d624`. It has its own copyright and license (HPND-sell-variant). The code that
  wayland-scanner generates from it has the same license.
