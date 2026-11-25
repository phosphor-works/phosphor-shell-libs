<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-service-upower

UPower (`org.freedesktop.UPower`) battery and power-supply values for Phosphor-based desktop shells.

## Responsibility

The library gives the `org.freedesktop.UPower` interface on the system bus to the shell as Qt and QML types. This
includes the aggregate display device, the list of devices and the `OnBattery` flag. It has no UI. The shell decides
how to show a battery percentage, a charging icon or a low-power warning.

The library is only a D-Bus client. It does not write to UPower, because UPower has no writable interface for the
values that it reports. It only observes.

## Key types

| Type | Role |
| --- | --- |
| `UPowerDevice` | One physical power source. `percentage`, `state`, `type`, `timeToEmpty`, `timeToFull`, `iconName`, `isLaptopBattery`, `healthPercentage`. |
| `UPowerHost` | Owns the device set. `onBattery`, `displayDevice` (the UPower aggregate battery), `deviceCount`, and signals when a device is added or removed. |
| `UPowerDeviceModel` | `QAbstractListModel` over the devices of the host. Roles: `device`, `percentage`, `deviceState`, `deviceType`, `iconName`, `isLaptopBattery`. |

## Use

C++ shell composition root:

```cpp
#include <PhosphorServiceUPower/QmlRegistration.h>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    PhosphorServiceUPower::registerQmlTypes();
    // ... load shell.qml
}
```

QML consumer (indicator for one battery):

```qml
import Phosphor.Service.UPower 1.0

UPowerHost { id: battery }

Label {
    visible: battery.displayDevice !== null
    text: battery.displayDevice
        ? Math.round(battery.displayDevice.percentage) + "%"
        : ""
}
```

QML consumer (list of devices):

```qml
import Phosphor.Service.UPower 1.0

UPowerHost { id: battery }
UPowerDeviceModel { id: devices; host: battery }

Repeater {
    model: devices
    delegate: Label { text: iconName + " " + Math.round(percentage) + "%" }
}
```

## Device types

The `DeviceType` enum covers the full UPower 1.0 range, from `UnknownType = 0` to `BluetoothGeneric=28`. Each value
has the UPower number, for example `MediaPlayer=9`. Thus QML can resolve each device type that UPower reports.

## Design notes

- **Async-only D-Bus.** The startup calls (`GetAll`, `EnumerateDevices`, `GetDisplayDevice`) and the
  `PropertiesChanged` fan-out all go through `QDBusPendingCallWatcher`. The GUI thread never waits on UPower.
- **Derived-state transitions.** `isLaptopBattery` depends on `type` and `powerSupply`. `healthPercentage` depends on
  `energyFull` and `energyFullDesign`. The applyProps path keeps the previous derived value and emits the NOTIFY
  only when the result changed. Thus QML bindings do not update again on each property change of the device.
- **Per-property signals.** `UPowerDeviceModel` subscribes to the `NOTIFY` of each property (percentage, state,
  type, iconName, isLaptopBattery). External consumers bind the specific signal that they need.
- **Display device.** UPower returns one "display" device that aggregates all batteries of the system. Use
  `displayDevice` for indicators at the top of a bar. Use the model for power-management UIs that show each device.

## Dependencies

- Qt6 ≥ 6.6 (Core, Qml, DBus)
- A running `org.freedesktop.UPower` on the system bus
