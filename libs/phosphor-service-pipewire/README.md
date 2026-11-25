<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-service-pipewire

A native PipeWire mixer for Phosphor-based desktop shells. It covers sinks, sources, streams for each app,
default-node switching, volume and mute.

## Responsibility

The library owns the PipeWire main loop on a dedicated `QThread`. It walks the `pw_registry` and gives sinks,
sources and streams to the shell as Qt and QML types. It has no UI. The shell decides how to show a volume mixer, an
OSD or a sound preferences page.

The library is non-visual (no `Qt::Gui` link). It runs in a single process: the loop runs inside the same
`QGuiApplication` that hosts the shell. The library hides all cross-thread work. Consumers see an API on the GUI
thread only, with queued signals from the loop thread behind it.

## Key types

| Type | Role |
| --- | --- |
| `PipeWireConnection` | The lifecycle owner. It holds the `pw_main_loop` thread, `pw_context` and `pw_core`. Properties: `connected`, `daemonAvailable`, `defaultSinkName`, `defaultSourceName`. Signals: `error(QString)`, `nodeAdded(PwNode*)`, `nodeRemoved(PwNode*)`. It sends the writes for each node (`writeVolumes`, `writeMuted`) and the WirePlumber metadata writes (`setDefaultSink`, `setDefaultSource`) to the loop thread. |
| `PwNode` | One audio node (sink, source, output stream or input stream). Properties: `id`, `name`, `nick`, `description`, `mediaClass`, `channelCount`, `volumes[]` (linear amplitudes), `muted`. Async setters: `setVolume(qreal)`, `setVolumes(QList<qreal>)`, `setMuted(bool)`. `PipeWireConnection` supplies the nodes. QML cannot create one. |
| `PwNodeModel` | Abstract `QAbstractListModel` that filters the nodes of a connection by `mediaClasses`. It has 10 fixed role names (9 user roles and `Qt::DisplayRole`): `node`, `id`, `name`, `nick`, `description`, `mediaClass`, `channelCount`, `volumes`, `muted`, and `Qt::DisplayRole` (`display`). `display` falls back through nick → description → name. |
| `PwSinkModel` / `PwSourceModel` / `PwStreamModel` | Subclasses with the common filters already applied. `PwStreamModel` includes both `Stream/Output/Audio` and `Stream/Input/Audio`. |
| `PipeWireHost` | QML singleton (`Phosphor.Service.PipeWire 1.0`) that owns the `PipeWireConnection` of the process and forwards each observable signal. It connects automatically when it is constructed. It has `connectToDaemon()`, `disconnectFromDaemon()` and `reconnect()` for explicit lifecycle control. |

## Use

C++ shell composition root:

```cpp
#include <PhosphorServicePipeWire/QmlRegistration.h>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    PhosphorServicePipeWire::registerQmlTypes();
    // ... QQmlApplicationEngine engine; engine.load(shellUrl); etc.
    return app.exec();
}
```

QML consumer (one volume slider for each sink):

```qml
import Phosphor.Service.PipeWire 1.0

ColumnLayout {
    Repeater {
        model: PwSinkModel { connection: PipeWireHost.connection }
        delegate: RowLayout {
            required property var model
            Label { text: model.display + (model.name === PipeWireHost.defaultSinkName ? " (default)" : "") }
            Slider {
                from: 0; to: 1
                value: model.volumes.length > 0 ? model.volumes[0] : 0
                onMoved: model.node.setVolume(value)
            }
            CheckBox {
                checked: model.muted
                text: "mute"
                onToggled: model.node.setMuted(checked)
            }
        }
    }
}
```

The CLI in `examples/phosphor-service-pipewire-cli/` is the reference C++ usage example and the acceptance harness.
Its subcommands include `phosphor-service-pipewire-cli list sinks`, `set-volume <target> <pct>`,
`set-default-sink <name>`, `mute <target>` and `default`.

## Tests

| Test | What it covers |
| --- | --- |
| `tests/test_smoke.cpp` | The connection lifecycle, the model role names and the rest of the public contract. It needs no PipeWire daemon. |
| `tests/test_audio_details.cpp` | Real routing, device writes and audio samples on a private, virtual PipeWire server. |

## Design notes

- **Threading.** The PipeWire `pw_main_loop` is not a Qt event loop. Each PipeWire API call must occur on the thread
  of the loop. The library owns one `QThread` named `PipeWireLoop`. Its `run()` body calls `pw_main_loop_run`
  directly, and Qt's `exec()` never runs there. Work from the GUI thread goes to the loop through `pw_loop_invoke`
  (the MT-safe dispatch that PipeWire documents). Events from the loop come back through
  `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`. Consumers never see a signal from a thread other than the
  GUI thread.
- **Pimpl and clean public headers.** `PipeWireConnection.h`, `PwNode.h`, `PwNodeModel.h` and `PipeWireHost.h` do
  not use libpipewire. Each `pw_*` and `spa_*` type is in `src/`. Consumers can use
  `target_link_libraries(... PhosphorServicePipeWire::PhosphorServicePipeWire)` without `libpipewire-0.3` in their
  own include set.
- **Linear amplitude on the surface.** `PwNode::volumes` is the linear amplitude of each channel, `[0.0, 1.0]` (the
  storage format of PipeWire). Cubic or perceptual curves for UI sliders belong to a higher layer. Values that go
  through the library and back do not lose precision.
- **No automatic reconnect.** A daemon restart (driver hotplug, kernel module reload) emits `error(QString)` and sets
  `connected` to false. The shell decides on a backoff and calls `connectToDaemon()` again. In QML,
  `PipeWireHost.reconnect()` does this in one call.
- **WirePlumber default-node switching.** This goes through the `default` metadata global.
  - Reads follow `default.audio.sink` / `default.audio.source` (the runtime defaults).
  - Writes go to `default.configured.audio.sink` / `default.configured.audio.source` (the persistent setting that
    WirePlumber promotes at the next reconcile).
  - If no metadata that WirePlumber manages is present, the connection silently ignores the write.
  - To detect that WirePlumber is absent, a shell writes with `setDefaultSink` / `setDefaultSource`. It then waits
    for the echo through `defaultSinkNameChanged` / `defaultSourceNameChanged` within a time budget.
  - The `cmdSetDefault` of the CLI uses this pattern. It reports "no WirePlumber default metadata observed" when the
    value was empty before the write and stayed empty until the deadline. It reports "timed out waiting for
    WirePlumber echo" when the value was set before the write but the write did not come back.
- **Context for each listener of node events.** Each `LoopNode` is the `data` of its own `pw_node_add_listener`
  registration. Thus the param and info callbacks get the node id and the owning `Private*` without a search of the
  hashmap on the loop side for each event.

## Dependencies

- `libpipewire-0.3` (PipeWire 1.0 or later).
- WirePlumber 0.5 at runtime for default-node switching. Without it, the library loads inert, and
  `defaultSinkName` / `defaultSourceName` stay empty.
- Qt6 ≥ 6.6 (Core, Qml).
- A running PipeWire daemon at runtime. Without one, the library loads inert and reports `daemonAvailable=false`.
