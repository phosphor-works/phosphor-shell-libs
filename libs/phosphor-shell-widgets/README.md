<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# PhosphorShellWidgets

The `Phosphor.Widgets` atom library: the spectrum primitives that draw each shell surface, and some interactive
atoms. It is pure QML. The `Theme`, `Tokens`, `Motion` and `Spectrum` singletons of
[`phosphor-theme`](../phosphor-theme/README.md) supply all of its theming. Thus a palette or motion change reaches
each widget with no change to the widget. The atoms implement the design in Lambent's
[`docs/visual-identity.md`](https://github.com/phosphor-works/Lambent/blob/main/docs/visual-identity.md).

## Responsibility

The library supplies the primitives of the visual identity: the rail, the stroke, the underline, the tabular value,
the placement miniature, the settle spring and the decoration slot. It also supplies a small set of interactive
atoms: button, slider, text field and card. Higher-level surfaces (the bar, launcher, control center, OSDs, toasts)
are built from these atoms. They do not restyle each surface again.

The library owns presentation only. It holds no business logic and no service bindings. A host connects the
`clicked` / `moved` signal of an atom to the controller that drives it.

## Key types

| Component | Role |
|---|---|
| `SpectrumRail` | The 2 px gradient rail on a screen edge. It samples the spectrum by position. `gleam` runs the idle sweep. `sliceStart`/`sliceEnd` show the lens of a scrolling strip. |
| `SpectrumStroke` | The 1 px inset stroke that gives chrome its edge. Rest opacity, full when `active`, white when `focused`. The hue comes from `t`. |
| `SpectrumUnderline` | A 2 px underline. Its length is a value and its hue is the state axis. `tick()` flashes it. |
| `TabularText` | A value in the mono face with tabular figures. `tickOnChange` underlines a changed digit. |
| `PlacementMiniature` | The placement map of a screen (zones, tiles or strip columns) as a miniature. `MiniatureEdges.js` morphs matched edges between modes. |
| `SettleAnimation` | The positional spring (`shell.settle`) for a `Behavior`. It retargets from the current velocity. |
| `DecorationSlot` | The place where a surface pack goes on chrome. It names the frame item and the surface path, and makes the host's decoration Component. |
| `PhosphorButton` | Button in four variants (`Filled`, `Tonal`, `Outlined`, `Text`). |
| `PhosphorSlider` | Continuous horizontal slider. The knob settles on the spring. Emits `moved`. |
| `PhosphorTextField` | Outlined single-line input with a placeholder. Focus lights the stroke. |
| `PhosphorCard` | Rounded surface container with a stroke for depth. Its children go in a padded content area. |
| `PhosphorRipple` | Shared interaction layer: the hover / press tint and the press ripple. |

## Use

```qml
import Phosphor.Widgets

PhosphorCard {
    elevation: 2

    ColumnLayout {
        PhosphorTextField { placeholderText: i18n("Name") }
        PhosphorSlider { from: 0; to: 100; value: 40; onMoved: (v) => model.level = v }
        PhosphorButton {
            text: i18n("Apply")
            variant: PhosphorButton.Filled
            onClicked: controller.apply()
        }
    }
}
```

For the disabled state of an atom, set `enabled: false`. The container and the content then use the disabled
opacities (`StateLayer.disabled_container` / `StateLayer.disabled_content`).

## Tests

`tests/` has a C++ test (`test_audiospectrum.cpp`) and QtQuickTest suites for the atoms, the decoration slot, the
navigator, the ripple clip and the spectrum lifetime. `examples/phosphor-widgets-kitchen-sink/` shows the widgets.

## Design notes

- **Tokens, not literals.** Each color reads `Theme.*` or `Spectrum.*`, each animation reads `Motion.*`, and each
  state opacity reads `StateLayer.*`. Live retinting works because the atoms index the `palette` map of the `Theme`
  singleton (a `NOTIFY`-backed property).
- **Color is a coordinate.** The hue of a stroke comes from `t`. Anything on the bar uses the rail axis (x over the
  screen width). Anything that shows a level uses the state axis (a value 0..1). The cells of a miniature use the
  structure axis. Focus and urgency are white and rose. These are the only colors that are not sampled.
- **Depth is a stroke, not a shadow.** Chrome has no drop shadow and no radius above 10 px
  (`Tokens.radius_edge/tile/container/mini`). `PhosphorCard.elevation` selects the surface tint tier and lights the
  stroke. The M3 shadow tiers in `Tokens` are for settings pages.
- **Enter fast, release slow.** A stroke goes to full in 90 ms with a very small overshoot, and releases over
  360 ms. Positions and sizes settle on the spring. Reduced motion halves the tails and removes the overshoot and
  the gleam.
- **Custom controls, not restyled `QtQuick.Controls`.** The atoms are built from primitives (`Rectangle`, `Text`,
  `TextInput`, handlers), so the theme controls all of their appearance.
- **The host owns the state.** `PhosphorSlider` and `PhosphorTextField` expose the value / text for the host to
  bind. The atoms emit intent. `PhosphorSlider` follows the `QtQuick.Controls.Slider` convention: set `value` as the
  initial position, and respond to `moved`.
- **Keyboard and focus.** Each interactive atom takes Tab focus when it is enabled. `PhosphorButton` activates on
  Space / Enter / Return. `PhosphorSlider` moves by `stepSize` on the arrow keys, and goes to the ends on
  Home / End. `PhosphorTextField` gives focus to its inner input.
- **The host owns the decoration.** `DecorationSlot` is a seam. The library names the frame item
  (`shaderAnchor: true`) and the surface path. The composition root supplies the Component that draws the pack
  chain. Thus the library never depends on the rendering stack of the shell process.

## Dependencies

- Qt6 ≥ 6.6 Core / Gui / Qml / Quick. `PhosphorRipple` uses `QtQuick.Shapes` (`Qt6::QuickShapes`).
  `SettleAnimation` uses `org.phosphor.animation` (`PhosphorMotionAnimation`).
- `phosphor-theme` (`Phosphor.Theme` QML module) for the singletons. In-tree builds link the theme QML plugin
  automatically. The module is static and in-tree only.
- `org.kde.kirigami` at runtime, for the app glyphs of `PlacementMiniature` (`Kirigami.Icon`). It is a
  `DEPENDENCIES` entry, never `IMPORTS`, because the `Theme` attached type of Kirigami would hide ours.
