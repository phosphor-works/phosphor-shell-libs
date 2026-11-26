<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-service-network

This library gives Phosphor-based desktop shells the NetworkManager (`org.freedesktop.NetworkManager`) device and
connectivity state.

## Responsibility

The library exposes the system bus `org.freedesktop.NetworkManager` interface as Qt and QML types. It covers the
interface, type and state of each device, the global connectivity level, and the networking and Wi-Fi radio
toggles. It has no UI. The shell decides how to show a connectivity icon, a device list or a Wi-Fi toggle.

The library is a D-Bus client that uses the generic `PhosphorDBus::Client` async helper. It observes the manager and
device state. Its write calls toggle the Wi-Fi radio, start a Wi-Fi scan, and connect to a saved profile or an access
point.

## Key types

| Type | Role |
| --- | --- |
| `NetworkDevice` | One `org.freedesktop.NetworkManager.Device`. `interfaceName`, `deviceType`, `state`, `managed`. |
| `NetworkHost` | Owns the device set and the manager state. `networkingEnabled`, `wirelessEnabled` (writable), `connectivity`, `primaryConnectionType`, `deviceCount`. Calls `scanWifi()`, `activateConnection()`, `connectToAccessPoint()`. Emits signals when a device is added or removed. |
| `NetworkDeviceModel` | `QAbstractListModel` over the devices of the host. Roles `device`, `interfaceName`, `deviceType`, `deviceState`, `managed`. |
| `AccessPoint` | One `org.freedesktop.NetworkManager.AccessPoint`. `ssid`, `strength`, `frequency`, `bssid`, `security`, `secured`. |
| `AccessPointModel` | `QAbstractListModel` over the scanned access points of one Wi-Fi device (bind `device`). Roles `accessPoint`, `ssid`, `strength`, `frequency`, `bssid`, `security`, `secured`. |
| `NetworkConnection` | One saved profile (`Settings.Connection`). `id`, `uuid`, `connectionType`. |
| `NetworkConnectionModel` | `QAbstractListModel` over the saved connections of NetworkManager. It loads them itself. Roles `connection`, `id`, `uuid`, `connectionType`. |

## Use

In the C++ composition root of the shell:

```cpp
#include <PhosphorServiceNetwork/QmlRegistration.h>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    PhosphorServiceNetwork::registerQmlTypes();
    // ... load shell.qml
}
```

A QML consumer with a connectivity indicator and a Wi-Fi toggle:

```qml
import Phosphor.Service.Network 1.0

NetworkHost { id: net }

Switch {
    text: "Wi-Fi"
    checked: net.wirelessEnabled
    onToggled: net.wirelessEnabled = checked
}

Label {
    text: net.connectivity === NetworkHost.Full ? "Online" : "Limited / offline"
}
```

A QML consumer with a list of devices:

```qml
import Phosphor.Service.Network 1.0

NetworkHost { id: net }
NetworkDeviceModel { id: devices; host: net }

Repeater {
    model: devices
    delegate: Label { text: interfaceName + " (type " + deviceType + ", state " + deviceState + ")" }
}
```

## Connect paths

| Call | Target |
| --- | --- |
| `activateConnection()` | A saved profile. |
| `connectToAccessPoint()` | An open or WPA-PSK network, through NM AddAndActivateConnection. |

`AccessPointModel` derives a security label for each access point.

## Tests and demo

| Item | What it covers |
| --- | --- |
| Smoke harness | The public contract (role names, enum wire constants, lifecycle), with no daemon. |
| `test_network_operations` | The live details, profile identity, connection settings and failures, a retry that updates the existing profile, and the deactivation of a late reply after a cancel. It runs against a fake NetworkManager fixture on D-Bus. |
| `examples/phosphor-service-network-cli` | A `phosphorctl`-style CLI demo with the commands status, list-devices, list-connections, list-aps, scan and connect. |

## Design notes

- **Async-only D-Bus.** The manager bootstrap (`Properties.GetAll`, `GetDevices`), the `Properties.GetAll` of each
  device, the `PropertiesChanged` fan-out and the writes (`Set WirelessEnabled`, `Device.Wireless.RequestScan`) all
  go through `QDBusPendingCallWatcher` / `PhosphorDBus::Client`. The GUI thread never blocks.
- **No optimistic writes.** `setWirelessEnabled` sends `Properties.Set`. NetworkManager then sends the change back
  through `PropertiesChanged`. The cached `wirelessEnabled` and its NOTIFY change only after the radio toggled. Thus
  the property always agrees with the daemon state.
- **Sparse wire enums.** The library maps `DeviceType` (3/4 unused, then a jump to `Wireguard=29`) and
  `DeviceState` (multiples of ten) value by value. An unknown raw value becomes `Unknown` / `UnknownState`. The
  library never casts it to an enumerator that it does not declare. `Connectivity` is contiguous 0..4 and clamped.
- **Path containment.** When a device is added or removed, the library rejects any object path outside
  `/org/freedesktop/NetworkManager/Devices/`. Thus a daemon that behaves badly cannot create a `NetworkDevice` on an
  invalid path.

## Dependencies

- Qt6 ≥ 6.6 (Core, Qml, DBus)
- `PhosphorDBus` (private link: the async method-call helper)
- A running `org.freedesktop.NetworkManager` on the system bus. Without it the library loads inert, with an empty
  device list and `UnknownConnectivity`.
