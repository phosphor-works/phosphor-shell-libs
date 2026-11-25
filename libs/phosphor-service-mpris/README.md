<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-service-mpris

This library finds and controls MPRIS2 (`org.mpris.MediaPlayer2.*`) media players for Phosphor-based desktop shells.

## Responsibility

The library watches the session bus for MPRIS players. It shows their metadata and playback state, and sends
transport controls to them. It has no UI. The shell decides how to show a now-playing card, a transport bar or a
media pop-out.

## Key types

| Type | Role |
| --- | --- |
| `MprisPlayer` | One player. Identity, track metadata, playback state, position, volume, loop / shuffle and capability flags. Invokables for play / pause / stop / togglePlaying / next / previous / seek / setPosition / raise / quit. |
| `MprisHost` | Watches the session bus for `org.mpris.MediaPlayer2.*` names. Owns the live `MprisPlayer` set. Emits `playerAdded` / `playerRemoved`. |
| `MprisPlayerModel` | `QAbstractListModel` over `MprisHost`. Roles `player`, `identity`, `playbackState`, `trackTitle`, `trackArtist`, `artUrl`. |

## Use

In the C++ composition root of the shell:

```cpp
#include <PhosphorServiceMpris/QmlRegistration.h>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    PhosphorServiceMpris::registerQmlTypes();
    // ... load shell.qml
}
```

A QML consumer:

```qml
import Phosphor.Service.Mpris 1.0

MprisHost { id: mpris }
MprisPlayerModel { id: players; host: mpris }

Repeater {
    model: players
    delegate: Label {
        text: identity + " · " + trackTitle + " · " + trackArtist
    }
}
```

## Design notes

- **Async D-Bus.** All property fetches go through `QDBusPendingCallWatcher`. A player that is slow to respond never
  blocks the GUI thread.
- **PropertiesChanged + NameOwnerChanged.** `NameOwnerChanged` for `org.mpris.MediaPlayer2.*` services finds the
  players. `PropertiesChanged` brings the property updates. Both connect through `QDBusConnection::connect` with a
  SLOT() string, because the Qt D-Bus API has no overload for these signals that takes a lambda.
- **Row mirror.** `MprisPlayerModel` keeps its own `QList<MprisPlayer*>` and does not index into the list of the
  host. Thus the `beginInsertRows` / `endRemoveRows` boundaries of the model always enclose the real change. This
  stays true independent of when the host emits its add and remove signals relative to its own list.

## Dependencies

- Qt6 ≥ 6.6 (Core, Qml, DBus)
- A running session bus with one or more `org.mpris.MediaPlayer2.*` players
