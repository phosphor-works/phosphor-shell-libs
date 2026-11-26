<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-shell

A declarative QML framework for layer-shell desktop shells, in the style of Quickshell. It gives the shell lifecycle
(XDG config discovery, QML engine management) and the QML window types that make layer-shell surfaces.

## Responsibility

`phosphor-shell` is the infrastructure layer between `phosphor-wayland` / `phosphor-layer` and the consumer QML
config. The first two know how to make a layer-shell surface. The config describes how the shell looks. A shell
binary makes a `ShellEngine` and points it at an XDG-located config tree. The engine then makes the user's QML,
which declares `PanelWindow`, `PopupWindow` and the other window types. Each window type makes the correct
layer-shell surface through `phosphor-layer`. Thus the QML author writes panels without wlr-protocol primitives.

`ShellEngine` also drives hot reload. It watches the config tree and the screen topology, and rebuilds the QML
engine when one of them changes. It saves the `PersistentProperties` state before the reload and restores it after.

Where it matters, the API is the same as the Quickshell API (`PanelWindow`, `Variants`, `LazyLoader`,
`PersistentProperties`, `Process`, `FileView`). Thus existing configs port with little change.

## Key types

| Type | Purpose |
|------|---------|
| `ShellEngine`            | Top-level lifecycle: QML engine, config discovery, hot reload. |
| `ShellLoader`            | Finds the app's `shell.qml` in the app's folder of the suite, `phosphor-works/<app>/` (PhosphorPaths), in the XDG config and data paths. If it finds none, it uses a bundled shell URL that the app supplies. |
| `PanelWindow`            | Layer-shell panel window with edge anchoring and an exclusive zone. |
| `PopupWindow`            | xdg-popup positioned against an anchor item, with in-place reposition. |
| `FloatingWindow`         | XDG-toplevel window for auxiliary UI that is not part of the shell. |
| `Variants`               | Lazy instantiation of declarative content for each screen. |
| `LazyLoader`             | Helper that delays the instantiation of expensive QML trees. |
| `PersistentProperties`   | Keeps property values across launches and hot reloads, for QML. |
| `Process`                | Sandboxed subprocess runner for QML. |
| `FileView`               | Watched-file reader for QML panels that read files (`/proc`, `/sys`, config files). |
| `SystemClock`            | Timer-driven clock (hours, minutes, seconds, date) with a configurable tick precision. |
| `WallpaperService`       | The shell's wallpaper store for each screen. It decodes images off the GUI thread. QML reaches it through `ShellGlobal.wallpaper`. `ShellEngine` keeps it in `<config>/<shellName>/wallpaper.json`. |
| `Environment`            | Cross-platform helpers for environment variables, the locale and the runtime directory. |
| `ShellGlobal`            | QML context object (`PhosphorShell`) with shell-wide config and runtime state. |
| `ScreenModel`            | Reactive list of physical screens, for `Variants` for each screen. |
| `Toplevels`              | `zwlr-foreign-toplevel-management-v1` consumer (window list for taskbars). |
| `Workspaces`             | `ext-workspace-v1` consumer (workspace list and switching for pagers). `supported` follows the global. |

## Use

The shell binary finds the config and starts the engine:

```cpp
#include <PhosphorShell/ShellEngine.h>
#include <PhosphorShell/ShellLoader.h>

PhosphorShell::ShellLoader loader(QUrl(QStringLiteral("qrc:/qt/qml/MyShell/shell.qml")));

PhosphorShell::ShellEngine engine(
    PhosphorShell::ShellEngine::Deps{
        .surfaceFactory = &factory,           // PhosphorLayer::SurfaceFactory
        .screenProvider = screenProvider,     // PhosphorLayer::IScreenProvider
        .shellName = QStringLiteral("myshell"),
    },
    &app);
engine.load(loader.resolve());
```

The config declares one panel for each screen:

```qml
import Phosphor.Shell
import QtQuick

Item {
    PerScreenPanels {
        model: PhosphorShell.screens

        delegate: PanelWindow {
            edge: PanelWindow.Top
            thickness: 32
        }
    }
}
```

## QML module

`ShellEngine` registers the `Phosphor.Shell` import. It also re-exports some types from sibling libraries, so shell
QML needs only one import:

- `ShaderBackground`: animated shader surface (from `phosphor-rendering`).
- `IdleInhibitor`: idle inhibition bound to a surface (from `phosphor-wayland`). Idle detection for the whole session
  is in `Phosphor.Service.Idle`'s `IdleService`.
- `ForeignToplevel`: one entry of the `Toplevels` window list (uncreatable, `Toplevels` makes it).

## Dependencies

- `Qt6::Core`, `Qt6::Gui`, `Qt6::Quick`, `Qt6::Qml`
- [`phosphor-paths`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-paths/README.md): the location of the app's shell tree and wallpaper store
- [`phosphor-layer`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-layer/README.md), [`phosphor-rendering`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-rendering/README.md), [`phosphor-shaders`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-shaders/README.md), [`phosphor-wayland`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-wayland/README.md)

An app names its folder with `PhosphorPaths::setAppDirectoryName()` at the top of `main()`. `ShellLoader` and the
wallpaper store read that name. The app names its surfaces with `ShellEngine::Deps::shellName`. Nothing here
assumes a particular shell. [Lambent](https://github.com/phosphor-works/Lambent) is one, and its `shell/` tree is a
working reference for consumers.

## See also

- [`phosphor-service-sni`](../phosphor-service-sni/README.md): StatusNotifierItem tray and dbusmenu.
- [`phosphor-service-mpris`](../phosphor-service-mpris/README.md): MPRIS2 media-player discovery and control.
- [`phosphor-service-upower`](../phosphor-service-upower/README.md): battery and power-supply readouts.
- [`phosphor-service-icontheme`](../phosphor-service-icontheme/README.md): XDG icon-theme resolver and Qt image provider that the tray uses.
- [`phosphor-layer`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-layer/README.md): the Role vocabulary that the window types use.
- [`phosphor-shell-patterns`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-shell-patterns/README.md): named Role recipes that the panels use.
