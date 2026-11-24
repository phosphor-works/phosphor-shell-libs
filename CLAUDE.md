<!-- SPDX-FileCopyrightText: 2026 fuddlesworth -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-shell-libs

The Phosphor shell libraries: Qt 6, C++20 and QML. The theme and popout foundation, the `Phosphor.*` QML surfaces
(bar, control center, launcher, OSD, notifications, lock, picker, dashboard, polkit, power) and the
`phosphor-service-*` system service libraries a Wayland desktop shell is built from. Lambent is the consumer. The
tree builds on phosphor-libs and nothing else of the suite.

The rules every phosphor-works repository follows come from the workspace repository, imported below. This file adds
phosphor-shell-libs' own. The shared Claude Code tooling (the command guard, `phosphor-works:build`,
`phosphor-works:debug`, `phosphor-works:code-audit`, the shared reviewers, the lesson index) comes from the
`phosphor-works` plugin, which `.claude/settings.json` enables and `workspace/tools/setup` installs.

@../workspace/claude/conventions.md

## Layout

- Each library is `libs/<name>/`, added from the root CMakeLists.txt in dependency order. Public headers are in
  `libs/<name>/include/<Namespace>/` and are included as `<Namespace/Header.h>` (`<PhosphorTheme/PaletteStore.h>`).
  The C++ namespace matches the include folder (`PhosphorTheme`, `PhosphorServiceNightLight`, `PhosphorShellLauncher`).
- A shared library has CMake target `<Namespace>` with the alias `<Namespace>::<Namespace>` and an export header
  from `generate_export_header`, `<Namespace>/<lowercase>_export.h` with the macro `<UPPERCASE>_EXPORT`
  (`PHOSPHORTHEME_EXPORT`). `phosphor-shell-launcher` differs: its header is generated under `include/` and the
  macro comes from `BASE_NAME PHOSPHORSHELLLAUNCHER`. Copy a neighbour's block when you add a library.
- A QML module is a STATIC `<Namespace>Qml` target (alias `<Namespace>::<Namespace>Qml`) with QML in
  `libs/<name>/qml/Phosphor/<Module>/`. The surfaces (`phosphor-shell-bar` and the other `phosphor-shell-*`
  surface libraries) are QML modules only, with no shared library and no install rules.
- `examples/` holds demos and command-line acceptance harnesses, built only when the folder exists. The Nix flake
  strips it from its source tree. Its files are `GPL-3.0-or-later`.
- `cmake/PhosphorShellLibsDependencies.cmake` pins phosphor-libs. `nix/package.nix` repeats that pin.

## Layers

- phosphor-libs is the only suite dependency. Nothing here links phosphor-engine-libs, phosphor-components or an
  app, and no library code names Lambent. A setting or path that belongs to the app comes in from the app
  (`phosphor-service-nightlight` takes its settings through setters).
- Foundation: `phosphor-theme`, then `phosphor-popout`, then `phosphor-shell`, then `phosphor-shell-widgets`.
  The surfaces build on the theme and widgets (some also on `phosphor-shell`, Kirigami or phosphor-wayland).
- A service library links phosphor-libs and Qt only. `phosphor-service-icontheme` is the one exception inside the
  services: `phosphor-service-sni` and `phosphor-service-notifications` link it, so it configures first.
- Services and surfaces do not link each other. The app connects a service to the surface that shows it.
- Keep the `add_subdirectory` order in the root CMakeLists.txt in step with these links.

## Compositor neutrality

The libraries assume no particular compositor, as Lambent's CLAUDE.md says for the shell. They use standard and
wlroots-family Wayland protocols, and report a missing global as a normal state (`supported` or `available` false),
never a crash.

- Workspaces are ext-workspace-v1 (`libs/phosphor-shell/protocols/`). Toplevels are wlr-foreign-toplevel. Night light
  is wlr-gamma-control (`libs/phosphor-service-nightlight/protocols/`). Lock is ext-session-lock-v1 through
  phosphor-wayland. Clipboard history is wlr-data-control through phosphor-wayland.
- Nothing new talks to `org.kde.KWin`, reads `kwinrc` or needs a KWin-only protocol. On KWin some features are
  unavailable (night light, the toplevel list), and that is the intended behavior.
- A vendored protocol XML keeps its own license. Add each one to the third-party table in `README.md`.

## License

Every library file, tests included, is `LGPL-2.1-or-later`. The demos under `examples/` are `GPL-3.0-or-later`. Code
that moves in from an app or an example is a relicense (see the conventions).

## How apps consume it

- Lambent pins one commit in `cmake/LambentDependencies.cmake` and adds this tree with CPM after phosphor-libs. The
  app's phosphor-libs pin is then the one in use: `phosphor_shell_libs_add_phosphor_libs()` sees `PhosphorIpc` and
  fetches nothing.
- Build an app against a local checkout with `-DCPM_phosphor-shell-libs_SOURCE=<dir>`, or use the workspace
  superbuild (`cmake --workflow --preset lambent` in `../workspace`), which sets that for every library.
- After a change merges here, move Lambent's pin (VERSION, URL, FILE and SHA256 together, in its dependencies file and
  `packaging/nix/package.nix`). When this tree needs a newer phosphor-libs, move this repository's pin and Lambent's
  phosphor-libs pin together.
- Lambent links each STATIC module with its generated plugin target (`PhosphorShellBarQml` and
  `PhosphorShellBarQmlplugin`). A new module needs both in the app.

## As a subproject

- The root CMakeLists.txt sets `PHOSPHOR_SHELL_LIBS_TOP_LEVEL`. Only the top level sets the C++ standard, ccache,
  ECM and the KDE settings, `QT_NO_CAST_FROM_ASCII`, `CMAKE_UNITY_BUILD`, `ENABLE_PCH`, `BUILD_TESTING` and the
  feature summary. As a subproject the app's values are in force. An `option()` outside that block (such as
  `PHOSPHOR_THEME_QML_INSTALL`) is a cache entry in the app's tree too.
- The Qt lookups (private modules included) run at this scope before phosphor-libs is added, so its IMPORTED
  targets are visible to every library here. Keep new `find_package` calls for shared targets in that block.
- Test helpers come from phosphor-libs through `PHOSPHOR_LIBS_CMAKE_DIR` (`PhosphorTestIsolation`,
  `PhosphorLibTesting`, `PhosphorTestPch`). Each tests `CMakeLists.txt` includes `PhosphorTestIsolation.cmake` with
  an `EXISTS` guard on that variable. Keep that form, not a relative path.
- The top level calls `phosphor_share_test_pch()` once, for this tree's tests and phosphor-libs'. An app that adds
  this tree makes the same call itself.
- A library's tests build when `BUILD_TESTING` is on, so Lambent's suite runs this tree's tests too.

## Build and test

```sh
cmake --workflow --preset dev            # configure into build/, build, test
ctest --test-dir build -R <name>         # one test
```

`phosphor-works:build` has the rest: the memory cap, the non-unity and no-PCH gate, the warning check and the offline
archives. The first configure downloads the pinned phosphor-libs into `.cache/cpm`. CI runs the `dev` workflow on
Arch with `QT_QPA_PLATFORM=offscreen`, a clang-format and qmlformat lint over the changed files, and the Nix flake.
This repository has no lefthook setup, so run `clang-format -i` and `qmlformat -i` on the files you change.

## QML

- List every QML file of a module in its `qt_add_qml_module`. Each list sets `QT_RESOURCE_ALIAS` to the basename, so
  the resource path matches the URI. Without it, a module's QML singletons do not resolve inside the same module.
- Put a module the QML needs under `DEPENDENCIES`, not `IMPORTS`. An `IMPORTS` entry injects that module's
  unqualified names into every file, and Kirigami's `Theme` then shadows the `Phosphor.Theme` singleton.
- Colors, motion and state layers come from `Phosphor.Theme` (`Theme`, `Tokens`, `Motion`, `StateLayer`).
- Only `phosphor-theme` also builds an installed plugin (`PhosphorThemeQmlShared`, option
  `PHOSPHOR_THEME_QML_INSTALL`, default ON). The other modules are STATIC and link into the app.

## Where things are

| Library | What it is |
| --- | --- |
| `phosphor-theme` | Palette and appearance stores, matugen runner, `Phosphor.Theme` singletons |
| `phosphor-popout` | Popout coordinator: lifetime, focus and exclusivity, `Phosphor.Popout` |
| `phosphor-shell` | Shell infrastructure and `Phosphor.Shell` (`PerScreen`, popup and floating windows, toplevels, workspaces) |
| `phosphor-shell-widgets` | `Phosphor.Widgets`, the M3 atom library |
| `phosphor-shell-bar` | `Phosphor.Bar`, the connected-corner bar |
| `phosphor-shell-control-center` | `Phosphor.ControlCenter` tile surface |
| `phosphor-shell-dashboard` | `Phosphor.Dashboard`, dashboard grid and keybind cheatsheet |
| `phosphor-shell-launcher` | Launcher providers, matcher, `.desktop` scanner, `Phosphor.Launcher` |
| `phosphor-shell-lock` | `Phosphor.Lock` lockscreen surface |
| `phosphor-shell-notifications` | `Phosphor.Notifications` toasts and cards |
| `phosphor-shell-osd` | `Phosphor.OSD` on-screen displays |
| `phosphor-shell-picker` | `Phosphor.Picker` wallpaper and theme strip |
| `phosphor-shell-polkit` | `Phosphor.Polkit` authentication prompt |
| `phosphor-shell-power` | `Phosphor.Power` session menu |
| `phosphor-service-*` | One system service each: bluetooth, brightness, clipboard, icontheme, lock, mpris, network, nightlight, notifications, pipewire, polkit, session, sni, upower |
| Demos and CLIs | `examples/` |
| phosphor-libs pin | `cmake/PhosphorShellLibsDependencies.cmake`, `nix/package.nix` |
