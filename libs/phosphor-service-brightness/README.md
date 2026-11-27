<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-service-brightness

This library gives Phosphor-based desktop shells the brightness of display backlights, keyboard backlights and
external monitors.

## Responsibility

The library exposes the backlights that have a brightness control as Qt and QML types. These are display panels
under `/sys/class/backlight`, keyboard backlights under `/sys/class/leds`, and external monitors over DDC/CI. It has
no UI. The shell decides how to show a brightness slider or an OSD.

For internal panels and keyboard backlights, the library reads from sysfs. A file watcher tracks external changes.
The library writes through logind's `org.freedesktop.login1.Session.SetBrightness`, so it needs no root and no udev
rule. The library shows only display and keyboard backlights. It skips the other `/sys/class/leds` entries
(capslock, power, RGB and trigger indicators), because they are not brightness controls.

External monitors have no `/sys/class/backlight` entry. The library reads them over DDC/CI on I2C, as a second
source in the same host, next to the sysfs devices. This source is optional at compile time (see "Design notes").

## Key types

| Type | Role |
| --- | --- |
| `BrightnessDevice` | One backlight or external monitor. `id` is a stable unique key: the sysfs name for a backlight, the I2C bus (for example `i2c-7`) for an external display. `name` is for people and is not unique: identical monitors share it. Also `kind` (Display / Keyboard / ExternalDisplay), `brightness`, `maxBrightness`, `percentage`. `setBrightness()` and `setPercentage()` write fire-and-forget through logind for a sysfs device, and go to the DDC worker for an external display. |
| `BrightnessHost` | Finds the brightness devices under a sysfs root and resolves the logind session for sysfs writes. When built with libddcutil, it also finds external monitors over DDC/CI. `deviceCount`, `deviceAt()`, `devices()`, and the signals `deviceAdded` / `deviceRemoved`. |
| `BrightnessDeviceModel` | `QAbstractListModel` over the devices of the host. It tracks the async `deviceAdded` / `deviceRemoved`, because external displays arrive after enumeration. Roles `device`, `id`, `name`, `kind`, `brightness`, `maxBrightness`, `percentage`. |

## Use

In the C++ composition root of the shell:

```cpp
#include <PhosphorServiceBrightness/QmlRegistration.h>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    PhosphorServiceBrightness::registerQmlTypes();
    // ... load shell.qml
}
```

A QML consumer with one brightness slider for each device:

```qml
import Phosphor.Service.Brightness 1.0

BrightnessHost { id: bright }
BrightnessDeviceModel { id: devices; host: bright }

Repeater {
    model: devices
    delegate: Slider {
        from: 0; to: maxBrightness; value: brightness
        onMoved: device.setBrightness(value)
    }
}
```

## Tests and demo

| Item | What it covers |
| --- | --- |
| Smoke harness | Enumeration, the `kbd_backlight` filter, the percentage math, divide-by-zero safety, live watcher updates, the model role names, the async add and remove of the model and its reset when the host is destroyed. Also the logind write path for the `backlight` and `leds` subsystems, and the route, clamp and read-back path of `ExternalDisplay`. It runs against a fake sysfs tree, a fake logind over a session-bus loopback and a recording setter (no I2C). It needs no root and no daemon. |
| `examples/phosphor-service-brightness-cli` | A CLI like `lambentctl` demo with the commands list, get and set. The live DDC/CI worker needs hardware, so this CLI checks it against real monitors. |

## Design notes

- **sysfs read, logind write.** The current value comes from the sysfs `brightness` attribute, and the range from
  `max_brightness`. A write goes through logind `Session.SetBrightness(subsystem, name, value)` with the matching
  subsystem: `backlight` for a display, `leds` for a keyboard. Thus the library needs no root and no udev rules. If
  no logind session is bound, a write does nothing and the library is read-only.
- **No optimistic writes.** `setBrightness` / `setPercentage` send the logind call. The file watcher then reads the
  applied value. The cached `brightness` and its NOTIFY change only after sysfs changes. External displays work the
  same way: the write goes to the DDC worker, and the cached value changes only when the worker reads it back.
- **External displays (DDC/CI), optional.** When the build finds `libddcutil` (ddcutil 2.x, through pkg-config), the
  host also finds external monitors over DDC/CI on I2C (VCP feature `0x10`). It shows them as `ExternalDisplay`
  devices. Without the dependency, the library builds the same way and shows no external displays. The sysfs and
  logind path does not change.
- **DDC/CI runs on a worker thread.** libddcutil I2C calls block and are slow. Enumeration and get/set therefore run
  on a dedicated worker `QThread`, which sends results back through queued signals.
- **DDC/CI access and refresh.** DDC/CI has no change notification, and logind does not mediate it. External
  displays therefore need `/dev/i2c-*` access (the `i2c` group and a udev rule), and the library polls them instead
  of watching them. Without I2C access, the library shows no external displays.
- **Brightness only, not LEDs.** The library covers display backlights and the `kbd_backlight` entries under
  `/sys/class/leds`. It skips every other LED (indicator, RGB, trigger). A general LED control is a different
  concern.
- **Divide-by-zero safe.** If a device has a `max_brightness` of 0 or none, the library shows a 0 percentage and
  ignores writes. It does not crash.
- **Injectable for testing.** The constructor takes the sysfs root and the `(connection, service, sessionPath)`.
  Thus tests run the full read and write path against a fake sysfs tree and a fake logind, with no root and no
  daemon.

## Dependencies

- Linux sysfs (`/sys/class/backlight`, `/sys/class/leds`).
- A running `org.freedesktop.login1` (logind) on the system bus, for the sysfs write path. Without it the library
  loads read-only and inert.
- `PhosphorDBus` (private link: the logind `SetBrightness` call).
- Qt6 ≥ 6.6 (Core, Qml, DBus). No QtGui.
- `libddcutil` (ddcutil 2.x), optional. If present, it enables the brightness of external monitors (DDC/CI). If
  absent, the build does not change.
