<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# PhosphorShellOsd

The `Phosphor.OSD` on-screen-display framework: the transient overlays that a shell shows for volume, brightness,
mic mute, caps lock and similar events. It is pure QML, themed through
[`phosphor-theme`](../phosphor-theme/README.md) and built on the
[`phosphor-shell-widgets`](../phosphor-shell-widgets/README.md) atoms.

## Responsibility

The framework owns the lifecycle of the transient OSD surface. It shows one OSD at a time, holds it for a timeout,
hides it, and sends triggers to the correct screen. It debounces and deduplicates repeated triggers. Thus a turning
volume key keeps one OSD alive and refreshed, and does not stack surfaces.

The framework owns presentation and timing only. Delegates supply the OSD *content*: the four built-ins here, or
delegates from plugins. The host supplies the *triggers*: a hotkey handler, an IPC call or a service signal.

## Key types

| Component       | Role                                                                                       |
|-----------------|--------------------------------------------------------------------------------------------|
| `OSDHost`       | OSD surface manager for one screen: one OSD at a time, hold timer, debounce and dedupe, hide, screen routing. Takes `placementMap`, `edgeMargin` and `topInset`, and reports `anchoredToWindow`. |
| `OSDCard`       | Shared chrome: a screen-edge band that draws out along the edge, with a glyph, a label and optional progress. It is not a card in the center. |
| `VolumeOSD`     | Speaker glyph and progress. A muted cross at zero. `value` is 0..100.                        |
| `BrightnessOSD` | Sun glyph and progress. `value` is 0..100.                                                   |
| `MicOSD`        | Microphone glyph. `active` true = muted (red slash and label).                               |
| `CapsLockOSD`   | Caps-lock glyph. `active` true = on (primary tint and label).                                |

## Use

```qml
import Phosphor.OSD

OSDHost {
    id: osd
    anchors.fill: parent
    screenName: "DP-1"
    provider: registryProvider   // createOSD(kind, parent) -> Item
}

// elsewhere, on a volume key / IPC call:
osd.show("volume", 62, undefined, "DP-1")
```

`provider` is any object that has `createOSD(kind, parent) -> Item`. In the shell, a `Registry<IOSDFactory>` backs
it. The OSD demo sets this up and registers the four built-ins as `IOSDFactory` instances. A delegate has `value`
(0..100), `active` (bool) or both. `OSDHost` sets the properties that the delegate has.

The host owns the lifetime of the delegate and destroys it on a swap or a hide. Thus a provider MUST return a
**destroyable** item. A C++ factory (the `IOSDFactory` path) must set `QQmlEngine::setObjectOwnership(item,
QQmlEngine::JavaScriptOwnership)` before it returns the item. A QObject-parented item has `CppOwnership` by default.
Then the host's `destroy()` throws ("indestructible object"), and a ghost stays behind the next OSD.

For more than one screen:

1. Make one `OSDHost` for each monitor (through the `PerScreenPanels` of `Phosphor.Shell`).
2. Give each host its `screenName`.
3. Send the same trigger to all hosts.

The `targetScreen` argument of `show()` routes the trigger. `""` or no argument means all screens.

## Tests

A QtQuickTest suite tests `OSDHost`, the band chrome and the four built-in OSDs. The acceptance demo is
`../../examples/phosphor-osd-demo/`, and `phosphorctl call osd.show` drives it.

## Design notes

- **Debounce and dedupe.** A repeated `show()` of the kind that is already visible updates the delegate and starts
  the hold timer again. It does not make a new delegate. Thus a held key streams smoothly through one surface. A
  different kind replaces the delegate.
- **Lifecycle through states.** `OSDHost` drives a `shown`/`hidden` state pair. The transitions animate the
  `reveal` of the band (its length along the edge) with the Motion tokens of `phosphor-theme`, with no scale and no
  fade. The end of the hide transition destroys the delegate and emits `hidden(kind)`. The hold timer changes
  `shown -> hidden`. A kind swap also emits `hidden(previousKind)` synchronously from `show()` for the outgoing OSD.
  Thus each `shown` has a matching `hidden`.
- **A provider seam, not a hard registry dependency.** `OSDHost` takes a `provider` object. Thus you can test the
  framework with a fake, and the registry wiring (IOSDFactory) is in the host or the demo, not in the QML.
- **Routing is data, not topology.** Screens are addressed by name, so unit tests can test routing without real
  monitors.

## Dependencies

- Qt6 ≥ 6.6 Core / Gui / Qml / Quick. `QtQuick.Shapes` (`Qt6::QuickShapes`) for the OSD glyphs.
- `phosphor-theme` (`Phosphor.Theme`) for the tokens and Motion. `phosphor-shell-widgets` (`Phosphor.Widgets`) for
  `TabularText`, `SettleAnimation` and `DecorationSlot`. In-tree builds link their QML plugins automatically. This
  module is static and in-tree only.
