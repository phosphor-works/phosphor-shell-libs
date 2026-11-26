<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-shell-libs

phosphor-shell-libs is a set of LGPL Qt 6 libraries for building a Wayland desktop shell. It holds the theme and
popout foundation, the `Phosphor.*` QML surfaces (bar, control center, launcher, OSD, notifications, lock, picker,
dashboard) and the system service libraries behind them. The libraries build on
[phosphor-libs](https://github.com/phosphor-works/phosphor-libs).

## Libraries

Each library is in `libs/<name>/`. The table has one row for each library, in three areas.

| Library | Area | Description |
| --- | --- | --- |
| [`phosphor-theme`](libs/phosphor-theme/README.md) | Foundation | The design-token store, with a hot-reloadable `PaletteStore`, a matugen wrapper and the `Phosphor.Theme` QML module. |
| [`phosphor-popout`](libs/phosphor-popout/README.md) | Foundation | The coordinator that controls the lifetime, focus and exclusivity of every popout in the shell. |
| [`phosphor-shell`](libs/phosphor-shell/README.md) | Foundation | The declarative QML framework that runs the shell and gives the QML window types for layer-shell surfaces. |
| [`phosphor-shell-widgets`](libs/phosphor-shell-widgets/README.md) | Foundation | The `Phosphor.Widgets` atoms: the spectrum primitives that draw every shell surface, and some interactive atoms. |
| [`phosphor-shell-bar`](libs/phosphor-shell-bar/) | Surfaces | The `Phosphor.Bar` module: `BarHost`, the `Slot` left, center and right regions, and the built-in bar widgets. |
| [`phosphor-shell-control-center`](libs/phosphor-shell-control-center/README.md) | Surfaces | The `Phosphor.ControlCenter` quick-settings panel, with its tiles for network, Bluetooth, audio and other settings. |
| [`phosphor-shell-dashboard`](libs/phosphor-shell-dashboard/) | Surfaces | The `Phosphor.Dashboard` module: the full-screen `Dashboard` placement map and the `Cheatsheet` shortcut reference. |
| [`phosphor-shell-launcher`](libs/phosphor-shell-launcher/README.md) | Surfaces | The spotlight-style launcher that searches applications, windows, clipboard history, arithmetic and shell commands. |
| [`phosphor-shell-lock`](libs/phosphor-shell-lock/) | Surfaces | The `Phosphor.Lock` module: the ext-session-lock window, the lock-screen layout and a shared authentication controller. |
| [`phosphor-shell-notifications`](libs/phosphor-shell-notifications/README.md) | Surfaces | The `Phosphor.Notifications` toasts that the shell shows when an app sends a notification. |
| [`phosphor-shell-osd`](libs/phosphor-shell-osd/README.md) | Surfaces | The `Phosphor.OSD` on-screen displays for volume, brightness, microphone mute, caps lock and similar events. |
| [`phosphor-shell-picker`](libs/phosphor-shell-picker/) | Surfaces | The `Phosphor.Picker` wallpaper and theme strip, which previews a palette on the live desktop. |
| [`phosphor-shell-polkit`](libs/phosphor-shell-polkit/) | Surfaces | The `Phosphor.Polkit` authentication prompt and its controls. |
| [`phosphor-shell-power`](libs/phosphor-shell-power/) | Surfaces | The `Phosphor.Power` module with `PowerMenu`, the session menu that the shell opens as a modal popout. |
| [`phosphor-service-bluetooth`](libs/phosphor-service-bluetooth/README.md) | Services | BlueZ (`org.bluez`) adapter and device state, with pairing. |
| [`phosphor-service-brightness`](libs/phosphor-service-brightness/README.md) | Services | Display and keyboard backlight brightness. |
| [`phosphor-service-clipboard`](libs/phosphor-service-clipboard/README.md) | Services | A clipboard history that removes duplicates, has a size limit and is kept on disk. |
| [`phosphor-service-icontheme`](libs/phosphor-service-icontheme/README.md) | Services | XDG icon theme lookup, and the Qt image provider that gives the icons to QML. |
| [`phosphor-service-lock`](libs/phosphor-service-lock/README.md) | Services | Session lock and PAM authentication of the session user. |
| [`phosphor-service-mpris`](libs/phosphor-service-mpris/README.md) | Services | MPRIS2 (`org.mpris.MediaPlayer2.*`) media-player discovery and control. |
| [`phosphor-service-network`](libs/phosphor-service-network/README.md) | Services | NetworkManager (`org.freedesktop.NetworkManager`) devices, connectivity and Wi-Fi connections. |
| [`phosphor-service-nightlight`](libs/phosphor-service-nightlight/README.md) | Services | A compositor-neutral night light that sets the gamma ramps of each output. |
| [`phosphor-service-notifications`](libs/phosphor-service-notifications/README.md) | Services | The `org.freedesktop.Notifications` server (Desktop Notifications Specification 1.3). |
| [`phosphor-service-pipewire`](libs/phosphor-service-pipewire/README.md) | Services | A native PipeWire mixer for sinks, sources and application streams, with volume, mute and default devices. |
| [`phosphor-service-polkit`](libs/phosphor-service-polkit/README.md) | Services | A PolicyKit authentication agent, built on the `polkit-qt6` binding. |
| [`phosphor-service-session`](libs/phosphor-service-session/README.md) | Services | logind session and power actions over `org.freedesktop.login1`. |
| [`phosphor-service-sni`](libs/phosphor-service-sni/README.md) | Services | The system-tray host for `org.kde.StatusNotifierItem`, with `com.canonical.dbusmenu` menus. |
| [`phosphor-service-upower`](libs/phosphor-service-upower/README.md) | Services | UPower (`org.freedesktop.UPower`) battery and power-supply state. |

`examples/` holds demos and command-line acceptance harnesses for the libraries. To build them, configure with
`-DPHOSPHOR_SHELL_LIBS_BUILD_EXAMPLES=ON`.

## Build

The build needs what phosphor-libs needs: CMake 3.23 (3.25 for the presets below), Qt 6.10 with its private headers,
extra-cmake-modules and Wayland. It also needs KF6 Kirigami, libxkbcommon, libpipewire, polkit-qt6 and PAM.

The first configure downloads the phosphor-libs commit that `cmake/PhosphorShellLibsDependencies.cmake` pins. It also
downloads the archives that this commit pins. The downloads go into `.cache/cpm`.

```sh
cmake --workflow --preset dev
```

This command configures into `build/`, builds phosphor-libs and every library here, and runs the tests.

To build against a local phosphor-libs checkout, add `-DCPM_phosphor-libs_SOURCE=/path/to/phosphor-libs` to the
configure.

## Use from an app

An app adds phosphor-libs first and then this repository, both with [CPM](https://github.com/cpm-cmake/CPM.cmake).
The app pins each one to one commit by archive URL and SHA-256. The app's phosphor-libs pin is then the one in use,
and this tree fetches nothing of its own.

```cmake
CPMAddPackage(NAME phosphor-shell-libs
              URL "https://github.com/phosphor-works/phosphor-shell-libs/archive/<commit>.tar.gz"
              URL_HASH SHA256=<sha256>)
```

To build the app against a local checkout, configure it with
`-DCPM_phosphor-shell-libs_SOURCE=/path/to/phosphor-shell-libs`.

## Contributing

The suite conventions are in the [workspace](https://github.com/phosphor-works/workspace) repository, in
`claude/conventions.md`. `CLAUDE.md` gives the layout and the rules of this repository. `lefthook install` sets up
the Git hooks from `lefthook.yml`. The hooks format the staged files and check the Conventional Commits message.

## License

The libraries are LGPL-2.1-or-later. The text is in `COPYING.LESSER`. The example programs in `examples/` are
`GPL-3.0-or-later`, as their headers say. See `examples/LICENSE`.

Third-party files keep their own licenses:

| File | Origin | License |
| --- | --- | --- |
| `libs/phosphor-service-nightlight/protocols/wlr-gamma-control-unstable-v1.xml` | [wlr-protocols](https://gitlab.freedesktop.org/wlroots/wlr-protocols) `unstable/`, commit `1106a4a1c57562ddd438c547da4c0d6dc150d624`, unchanged | HPND-sell-variant (in the file) |
| `libs/phosphor-shell/protocols/ext-workspace-v1.xml` | [wayland-protocols](https://gitlab.freedesktop.org/wayland/wayland-protocols) `staging/ext-workspace/` | HPND-sell-variant (in the file) |

At build time, wayland-scanner generates code from a protocol file. That code has the license of the protocol file.
