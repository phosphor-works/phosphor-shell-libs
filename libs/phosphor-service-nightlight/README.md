<!-- SPDX-FileCopyrightText: 2026 fuddlesworth -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-service-nightlight

A compositor-neutral night light for Phosphor-based desktop shells.

## Responsibility

Warms the screen in the evening by setting each output's gamma ramps through
`wlr-gamma-control-unstable-v1`. It needs no compositor D-Bus API, so it works on
any compositor that offers that protocol. It owns the state a night-light UI
shows and the schedule that drives it. It does not own the user's settings: the
app loads them, hands them over through setters and saves what the user changes.
No UI is provided here.

- Colour temperature to gamma ramps, from a black-body model.
- Three schedules: constant, fixed hours, and sunset to sunrise at a location
  (sun times computed locally, no network or location service).
- Gradual scheduled transitions, and a short fade for every change the user makes.
- Inhibition, including a "pause until the next transition" form.

## Key types

| Type | Role |
| --- | --- |
| `NightLight` | The state model and controller. Settings: `enabled`, `mode` (`Constant`, `Hours`, `Location`), `dayTemperature`, `nightTemperature`, `eveningStart` / `morningStart` (`setHours()`), `transitionDuration`, the location (`setLocation()`), `fadeDuration`. State: `available`, `inhibited`, `active`, `targetTemperature`, `currentTemperature`, `nextTransition`. Actions: `inhibit(untilNextTransition)`, `uninhibit(cookie)`, `refresh()`. |
| `GammaBackend` | The seam `NightLight` drives. The default is the `wlr-gamma-control` client. Tests and other compositors' backends plug in through `NightLight`'s injecting constructor. |
| `whitePoint()`, `gammaTable()` | The colour-temperature math, exported for previews. |

## Typical use

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

A "pause until tomorrow" button holds a temporary inhibition, which ends by itself
when the next scheduled transition starts:

```cpp
m_pause = night->inhibit(true);
// ...or ended early by the user:
night->uninhibit(m_pause);
```

`registerQmlTypes()` registers `NightLight` as an instantiable type in the
`Phosphor.Service.NightLight` QML module.

## Design notes

- **One gamma owner per output.** The protocol gives one client exclusive control
  of an output's gamma. The night light takes the outputs while it is enabled and
  lets go when it is disabled and the fade back to neutral has finished, at which
  point the compositor restores each output's original ramps. If another program
  holds an output (gammastep, wlsunset), the compositor refuses with `failed`; that
  output stops counting toward `available` until `refresh()`.
- **Availability.** `available` is true when the compositor offers
  `zwlr_gamma_control_manager_v1` and at least one output has not refused. Before
  the first enable the outputs are not taken, so a refusal is only learned then.
  KWin does not offer the protocol, so on KWin the night light is unavailable.
- **Colour.** `whitePoint()` integrates Planck's law over the CIE 1931 2° observer
  (the multi-lobe Gaussian fit of Wyman, Sloan and Shirley, JCGT 2(2), 2013),
  converts XYZ to linear sRGB (IEC 61966-2-1) and divides by the same at 6500 K,
  so 6500 K leaves the screen unchanged and both sides of it are continuous. The
  factors are gamma-encoded with exponent 2.2 before scaling the identity ramp, so
  a factor dims light, not the signal. Written from the published formulas; no
  code or table from redshift, gammastep or wlsunset is used.
- **Sun.** The sun's elevation comes from the Astronomical Almanac's
  low-precision solar coordinates and the linearised Greenwich sidereal time. Day
  is the sun at or above -0.833° (the sunrise and sunset altitude), night at or
  below -6° (the end of civil twilight), linear between. A polar day or night has
  no transitions.
- **Time.** `NightLight` reads the time from an injected clock (the system's in
  production). It recomputes at each transition, every two seconds during one,
  and at least once a minute, so clock changes and resume from suspend are
  noticed. `refresh()` recomputes at once.
- **Threads.** GUI thread only. The Wayland objects live on the default queue of
  Qt's display (reached through the public `QNativeInterface::QWaylandApplication`),
  which Qt dispatches on the GUI thread.

## Dependencies

- Qt6 (Core, Gui, Qml), libwayland-client and wayland-scanner.
- `protocols/wlr-gamma-control-unstable-v1.xml`, vendored unchanged from
  [wlr-protocols](https://gitlab.freedesktop.org/wlroots/wlr-protocols) at commit
  `1106a4a1c57562ddd438c547da4c0d6dc150d624`. It carries its own copyright and
  licence (HPND-sell-variant), as does the code wayland-scanner generates from it.

## Status

The schedule, the colour math and `NightLight`'s state transitions are unit-tested
with injected times and a fake backend. The `wlr-gamma-control` client has no
automated test. `examples/phosphor-service-nightlight-cli` drives it by hand inside
a compositor; under a headless cage (wlroots 0.20) it binds the manager and the
output, and handles the headless output's `failed` and the release on exit. Setting
ramps on an output that accepts them (`gamma_size`, then `set_gamma`) needs a
compositor on real hardware.
