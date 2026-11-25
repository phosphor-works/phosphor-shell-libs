<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-theme

The active design-token store for Phosphor shells, with M3, ANSI 16 and brand-gradient extensions. It has a
hot-reloadable `PaletteStore`, a matugen subprocess wrapper, a `{{token[.field]}}` template renderer and the
`Phosphor.Theme` QML module.

## Responsibility

The shell binds to colors through tokens, not literals. `phosphor-theme` owns the live token map. It keeps that map
in sync with matugen output, user-edited JSON and in-process producers.

- **One token map for each engine.** `PaletteStore` is registered as a `QML_SINGLETON`. Its built-in default is the
  canonical Phosphor dark palette. QML reads it through the `Theme` singleton of `Phosphor.Theme`. C++ reads it
  through `IThemeService::palette()`.
- **Three input paths write the same map.** JSON load through `loadFromFile` and `QFileSystemWatcher`. In-process
  push through `applyTokens(QVariantMap)`. The matugen subprocess through `MatugenRunner::paletteReady`, which feeds
  `applyTokens`. You can test each path in isolation.
- **Safe for atomic renames.** The store handles editors that save through a temp file and a rename. The watcher
  re-arms on each `fileChanged`, so vim and emacs both cause exactly one reload for each save.
- **Token name contract.** `TokenNames::*` are the canonical strings. Consumers use these constants, not literal map
  keys. Thus a renamed token causes a compile error at each call site.

## Key types

| Type | Purpose |
|------|---------|
| `PhosphorTheme::IThemeService`    | Abstract service. Methods are `palette()`, `token()`, `loadFromJson()`, `loadFromFile()`, `applyTokens()`, `resetToDefaults()` |
| `PhosphorTheme::TokenNames`       | `constexpr` token name strings. Covers the M3 surface ramp, accents, status ramp, and brand-gradient stops |
| `PhosphorTheme::PaletteStore`     | Concrete `IThemeService` with built-in dark defaults, JSON parsing, file watching, and QML singleton registration |
| `PhosphorTheme::MatugenRunner`    | `QProcess` wrapper around `matugen image <wp> --json hex`. Emits `paletteReady(tokens, wallpaper)` and `failed(wp, reason)` |
| `PhosphorTheme::TemplateEngine`   | Static renderer for `{{token[.field]}}` substitution. Supports the `hex`, `hexa`, `r`, `g`, `b`, `alpha`, `rgb`, and `rgba` field variants |
| `Phosphor.Theme.Theme`            | QML singleton. Color tokens by name such as `Theme.primary`, `Theme.on_surface`, `Theme.brand_stop_0`. Bindings re-evaluate on `paletteChanged` |
| `Phosphor.Theme.Tokens`           | Non-color tokens. Spacing, radius, elevation, typography |
| `Phosphor.Theme.Motion`           | M3 duration tokens plus bezier easing curves named `standard`, `emphasized`, `decelerated`, `accelerated` |
| `Phosphor.Theme.StateLayer`       | Interactive-state opacities. Properties are `hover`, `focus`, `pressed`, `dragged`, `disabled_content`, `disabled_container` |

## Use

**C++: load a palette and react to changes.**

```cpp
#include <PhosphorTheme/PaletteStore.h>

using namespace PhosphorTheme;

PaletteStore store;                              // built-in dark defaults active
const QString palettePath = "/path/to/palette.json";
store.loadFromFile(palettePath);                 // arms QFileSystemWatcher

QObject::connect(&store, &PaletteStore::paletteChanged, [&]() {
    qDebug() << "palette reloaded:" << store.palette().size() << "tokens";
});

QObject::connect(&store, &PaletteStore::loadError,
                 [](const QString &path, const QString &reason) {
                     qWarning() << "load failed:" << path << reason;
                 });
```

**QML: bind to tokens.** The named accessors read the tracked `palette` map, so bindings update live when the store
reloads. "Design notes" explains the binding-tracking model.

```qml
import QtQuick
import Phosphor.Theme

Rectangle {
    color: Theme.surface_container
    border.color: Theme.outline_variant
    radius: Tokens.radius_m

    Text {
        text: "Themed"
        color: Theme.on_surface
        font.pixelSize: Tokens.font_size_label_l
        font.family: Tokens.font_family
    }

    Behavior on color {
        ColorAnimation {
            duration: Motion.duration_medium_2
            easing: Motion.standard
        }
    }
}
```

**Retint from the wallpaper with matugen.**

```cpp
#include <PhosphorTheme/MatugenRunner.h>
#include <PhosphorTheme/PaletteStore.h>

MatugenRunner runner;
PaletteStore store;

QObject::connect(&runner, &MatugenRunner::paletteReady,
                 &store, [&](const QVariantMap &tokens, const QString &) {
                     store.applyTokens(tokens);   // same merge semantics as loadFromJson
                 });

runner.run(QStringLiteral("/path/to/wallpaper.jpg"));
```

**Render templates for external apps.**

```cpp
#include <PhosphorTheme/TemplateEngine.h>

// gtk-3.0.css.template contains: @define-color accent {{primary.rgb}};
PhosphorTheme::TemplateEngine::renderFile(
    QStringLiteral("/path/to/gtk-3.0.css.template"),
    QStringLiteral("/path/to/gtk-3.0/gtk.css"),
    store.palette());
```

## Design notes

- **QML bindings track property reads, not method calls.** The `Theme.*` accessors index into the `palette`
  Q_PROPERTY, which has `NOTIFY paletteChanged`. They never call `PaletteStore.token("...")`. That overload is a
  `Q_INVOKABLE`, which QML's dependency tracker cannot see. A binding on it does not re-evaluate when the palette
  reloads. The same rule applies in all QML that uses this module. Use `Theme.<name>`, not `paletteStore.token(name)`.
- **One singleton for each engine, not a process global.** `PaletteStore` is a `QML_SINGLETON`. The engine owns one
  instance for each `QQmlEngine`. If a test or a shell needs a different service, it registers an `IThemeService`
  instance with `qmlRegisterSingletonInstance` before it imports the `Phosphor.Theme` module.
- **Merge, not replace.** `loadFromJson` and `applyTokens` insert the supplied tokens over the active map. A token
  that the new payload does not have keeps its previous value. Matugen does not give brand-gradient and ANSI status
  colors. Thus those Phosphor extensions keep their previous values when the wallpaper changes.
- **Two JSON shapes.** The wrapped form is `{ "tokens": { ... } }`. The flat form is `{ "primary": "...", ... }` at
  the top level. The wrapper leaves room for sibling metadata. If both forms could match, the wrapper wins.
- **Matugen schema variants.** The parser reads wrapped `colors.{dark,light}`, single-mode `colors.{token}` and bare
  mode-at-root layouts. Thus the runner works with the matugen versions that are in use.
- **Unknown tokens in templates stay visible.** `TemplateEngine` keeps the `{{...}}` placeholder in the output and
  logs to `qWarning`. An unknown `.field` value falls back to hex with a warning. It does not stop the render.

## Dependencies

- `QtCore`, `QtGui`, `QtQml`. No Phosphor dependencies. This is a leaf library.
- An optional runtime dependency is the external `matugen` binary on `$PATH`. Only `MatugenRunner` needs it. The
  library never starts matugen on its own.

## See also

- `examples/phosphor-theme-demo/`, a GUI swatch sheet that hot-reloads from disk and from in-memory pushes.
- `examples/phosphor-theme-cli/`, a headless driver with the `set-wallpaper`, `dump`, `render-template` and `cycle`
  subcommands.
