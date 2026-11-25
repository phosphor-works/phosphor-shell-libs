<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# PhosphorShellControlCenter

The `Phosphor.ControlCenter` tile surface: the quick-settings panel that a shell opens from the bar. It holds
toggles for network, bluetooth, audio, brightness and other settings. It is pure QML, themed through
[`phosphor-theme`](../phosphor-theme/README.md) and built on the
[`phosphor-shell-widgets`](../phosphor-shell-widgets/README.md) atoms.

## Responsibility

The surface owns the rail list, the drill-in detail view and the routing between them. It knows how a control tile
*behaves*. It does not know what a given tile controls.

A `provider` backed by a `Registry<IControlCenterTileFactory>` supplies the tiles. In the factory contract, a null
return means "unavailable in this environment" (no service, no hardware). It is not an error.

The built-in tiles are in this library, and they import `Phosphor.Service.*` modules. But the library **links** no
service. The host process registers those modules before it loads QML, so they resolve at runtime. Each service
has a hand-written `registerQmlTypes()`. Lambent's
[`lambent/src/main.cpp`](https://github.com/phosphor-works/Lambent/blob/main/lambent/src/main.cpp) is an example.
The service-bound widgets of the bar use the same arrangement. If a host registers only some services, the tiles
for those services work and the other tiles are inert.

## Key types

| Component       | Role                                                                                          |
|-----------------|-----------------------------------------------------------------------------------------------|
| `ControlCenter` | Rail list and detail routing. Makes tiles from a provider, owns their teardown, applies the layout hint of each tile, and enforces the rule of one detail view at a time. |
| `Tile`          | Chrome for a **toggle**: a 52 px rail with an icon, a label, a live readout, a 2 px state-axis underline and an optional detail chevron. Spans the pane. |
| `SliderTile`    | Chrome for a **range** (volume, brightness): the same rail. The underline is the slider, and a tap on the icon mutes. Spans the pane. |
| `DetailPanel`   | The drill-in view: a titled surface with a back control that slides over the rails. The content comes from the tile. |

### Built-in tiles

| Tile | Binds to | Shape |
|---|---|---|
| `NetworkTile`    | `NetworkHost.wirelessEnabled` (the radio, never NM's global switch) | toggle |
| `BluetoothTile`  | the first `BluetoothAdapter.powered`                                | toggle |
| `IdleTile`       | an injected `IdleService`, holding one inhibition cookie            | toggle |
| `AudioTile`      | the default `PwNode` sink's volume and mute                         | slider |
| `BrightnessTile` | the first Display-kind `BrightnessDevice`                           | slider |

The host does the layout. A tile declares `spansRow`, and `ControlCenter` stretches it to the pane. Thus a tile
does not need the width of the pane. Each built-in control is a rail that spans. If a tile from a provider declares
a different value, `ControlCenter` uses that value.

No built-in tile supplies a detail view (`detailTitle` and `detailContent`). `hasDetail` comes from whether that
content exists, so these tiles show no chevron.

## Use

```qml
import Phosphor.ControlCenter

ControlCenter {
    anchors.fill: parent
    provider: controlCenterController   // Registry-backed, from the shell
    tileIds: ["network", "bluetooth", "audio", "brightness"]
}
```

A tile uses the shared chrome. In this example, `NetworkList` is a view that the tile author supplies. The library
does not have it.

```qml
Tile {
    id: root

    // NetworkHost is registered as a creatable type, not a singleton, so
    // the tile declares its own.
    NetworkHost {
        id: host
    }

    iconName: host.wirelessEnabled ? "network-wireless" : "network-wireless-disconnected"
    label: qsTr("Wi-Fi")
    sublabel: host.connectivity === NetworkHost.Full ? qsTr("Connected") : qsTr("Not connected")
    active: host.wirelessEnabled
    // The chevron appears because there is a detail view behind it.
    // `hasDetail` is derived from this, not set.
    detailTitle: qsTr("Wi-Fi")
    detailContent: Component {
        NetworkList {
            host: host
        }
    }
    // No local latch: write the property and let the service's own change
    // signal drive `active` back.
    onToggled: host.wirelessEnabled = !host.wirelessEnabled
}
```

## Presentation is the host's choice

Like `OSDHost` and `ToastHost`, `ControlCenter` draws into the item that is its parent and owns no surface. The
shell decides how it appears:

- As an engine-placed pane: a real toplevel that the placement engine positions, tethered to the chip of the bar
  (the spectrum design, A2 §4).
- Painted into the surface of the bar, as the floating fallback on an output with no placement engine.
- In a standalone layer-shell popout that `PopoutController` opens.

No choice changes this module. To change the presentation, change the wiring in the shell.

The choice does decide whether the keyboard works. In the bar socket, this surface has no window. The bar asks for
no keyboard interactivity (`keyboardFocus: PanelWindow.None`, which is normal for a panel). Thus the compositor
sends no key to it. The Tab order of the tiles, the Escape of the detail panel and all key handlers of this module
are inert there. In a standalone popout, all of them work. Pointer interaction is the same in each case, and the
keyboard is not the only path to any function here.

## Tiles do not self-latch

The service that the tile binds to drives `Tile.active`. `toggled` only *asks* the service to change. A tile that
latched locally would show a state that the system never reached when a request failed or another client changed
the state. Thus `active` follows the echo of the service.

## Tests

`tests/` is a QtQuickTest harness for the registry-agnostic contract of the host: the order in which tiles are made,
a provider that returns null, the detail-view routing and the rebuild path. The provider in those tests is a plain
QML object with `createTile(id, parent)`. This is the same seam that the shell fills with a registry-backed
controller.

The acceptance demo is `../../examples/phosphor-control-center-demo/`. It is a plain window that hosts the rails
over the real services. A tile whose service is missing on the machine shows that it is unavailable, and does not
disappear.

## Dependencies

- Qt6 >= 6.6 Core / Gui / Qml / Quick and KF6 Kirigami, for the tile icons. This module is QML only. It builds no
  C++ library.
- `phosphor-theme` (`Phosphor.Theme`) for the tokens, Motion and the state-layer opacities. `phosphor-shell-widgets`
  (`Phosphor.Widgets`) for the slider and the ripple. In-tree builds link their QML plugins automatically.
- No service dependency. The tiles bind the `Phosphor.Service.*` types that their host registered, so this module
  never links them.
