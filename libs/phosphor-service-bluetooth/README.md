<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-service-bluetooth

This library gives Phosphor-based desktop shells the BlueZ (`org.bluez`) adapter and device state, and Bluetooth
pairing.

## Responsibility

The library exposes the system bus `org.bluez` interface as Qt and QML types. It covers the power and discovery
state of each adapter, the pairing and connection state of each device, and the pairing agent. It has no UI. The
shell decides how to show an adapter toggle, a device list or a pairing dialog.

The library is a D-Bus client. It uses the generic `PhosphorDBus::Client` async helper and
`PhosphorDBus::ObjectManager`, because BlueZ is ObjectManager-rooted. It observes the adapter and device state and
gives the write calls for discovery, connect, trust and pair. It is also a D-Bus *server* for the `org.bluez.Agent1`
pairing agent.

## Key types

| Type | Role |
| --- | --- |
| `BluetoothAdapter` | One `org.bluez.Adapter1`. Properties `address`, `name`, `alias`, `powered`, `discoverable`, `pairable`, `discovering`. Calls `setPowered()`, `setDiscoverable()`, `startDiscovery()`, `stopDiscovery()`, `removeDevice()`. |
| `BluetoothDevice` | One `org.bluez.Device1`. Properties `address`, `name`, `alias`, `icon`, `paired`, `trusted`, `blocked`, `connected`, `rssi`, `adapter`, `uuids`. Calls `connectDevice()`, `disconnectDevice()`, `setTrusted()`, `setBlocked()`, `pair()`, `cancelPairing()`. |
| `BluetoothHost` | Owns the adapter and device sets through an ObjectManager observer, and registers the pairing agent. Properties `adapterCount`, `deviceCount`, `agent`. Calls `adapterAt()`, `deviceAt()`. Emits signals when an adapter or a device is added or removed. |
| `BluetoothAdapterModel` | `QAbstractListModel` over the adapters of the host. Roles `adapter`, `address`, `name`, `alias`, `powered`, `discoverable`, `pairable`, `discovering`. |
| `BluetoothDeviceModel` | `QAbstractListModel` over the devices of the host. It can show only the devices of one `adapter`. Roles `device`, `address`, `name`, `alias`, `icon`, `paired`, `trusted`, `blocked`, `connected`, `rssi`, `adapter`, `uuids`. |
| `BluetoothAgent` | `org.bluez.Agent1` (KeyboardDisplay). Emits `pinCodeRequested`, `passkeyRequested`, `confirmationRequested`, `authorizationRequested`, `serviceAuthorizationRequested` and the display signals. The consumer answers with `respondPinCode()`, `respondPasskey()`, `respondConfirmation()` or `rejectRequest()`. |

## Use

In the C++ composition root of the shell:

```cpp
#include <PhosphorServiceBluetooth/QmlRegistration.h>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    PhosphorServiceBluetooth::registerQmlTypes();
    // ... load shell.qml
}
```

A QML consumer with an adapter power toggle and a device list:

```qml
import Phosphor.Service.Bluetooth 1.0

BluetoothHost { id: bt }
BluetoothDeviceModel { id: devices; host: bt }

Switch {
    text: "Bluetooth"
    enabled: bt.adapterCount > 0
    checked: bt.adapterCount > 0 && bt.adapterAt(0).powered
    onToggled: if (bt.adapterCount > 0) bt.adapterAt(0).setPowered(checked)
}

Repeater {
    model: devices
    delegate: Label { text: (alias || name || address) + (connected ? " — connected" : "") }
}
```

A QML pairing dialog that the agent drives:

```qml
import Phosphor.Service.Bluetooth 1.0

BluetoothHost { id: bt }

Connections {
    target: bt.agent
    function onConfirmationRequested(devicePath, passkey, requestId) {
        // show a dialog displaying `passkey`, then:
        //   bt.agent.respondConfirmation(requestId, accepted)
    }
}
```

## Tests and demo

| Item | What it covers |
| --- | --- |
| Smoke harness | The public contract: model role names, the lifecycle, the request and response state machine of the agent, and one delayed-reply round trip. It runs over a session-bus loopback, with no daemon. |
| `phosphor-dbus` | The ObjectManager observer has its own loopback test. Other services (notifications, session) use the same observer. |
| `examples/phosphor-service-bluetooth-cli` | A CLI like `lambentctl` demo with the commands status, list-adapters, list-devices, power, scan, pair, connect, disconnect, trust, untrust and remove. |

## Design notes

- **ObjectManager-rooted enumeration.** `PhosphorDBus::ObjectManager` at the BlueZ root finds the adapters and
  devices (`GetManagedObjects` and `InterfacesAdded` / `InterfacesRemoved`). The library does not use signals for
  each interface. When an adapter goes, its child devices go too, by path prefix.
- **Async-only D-Bus.** The initial walk, the `PropertiesChanged` tracking of each object and every write
  (`Properties.Set`, `StartDiscovery`, `Connect`, `Pair`, `RemoveDevice`) go through `QDBusPendingCallWatcher` /
  `PhosphorDBus::Client`. The GUI thread never blocks. The initial properties come in the ObjectManager
  enumeration, so a domain object needs no round trip when it is constructed.
- **No optimistic writes.** `setPowered` / `setDiscoverable` / `setTrusted` / `setBlocked` send `Properties.Set`.
  BlueZ then sends the change back through `PropertiesChanged`. The cached value and its NOTIFY change only after
  the daemon applied the change. Thus the property always agrees with the daemon state.
- **The agent is a D-Bus server.** `BluetoothAgent` implements `org.bluez.Agent1` directly on the QObject
  (`ExportAllSlots` + `QDBusContext`), not through a generated adaptor. This lets `setDelayedReply` apply to its
  calls. An interactive callback defers its reply and emits a request signal with a `requestId`. A consumer answers
  it later. Each reply goes exactly once (a value, an empty accept, or `org.bluez.Error.Rejected`). `Cancel`
  answers every open request with `Canceled`.
- **Best-effort default agent.** The host registers the agent with `AgentManager1.RegisterAgent` (KeyboardDisplay)
  and asks to be the default agent. If `RequestDefaultAgent` fails because another agent has the slot, the error is
  not fatal. Pairing still works as a non-default agent.

## Dependencies

- Qt6 ≥ 6.6 (Core, Qml, DBus)
- `PhosphorDBus` (private link: the async method-call helper and the ObjectManager observer)
- A running `org.bluez` on the system bus. Without it the library loads inert, with empty adapter and device lists
  and no crash.
