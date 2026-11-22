# phosphor-shell-libs

LGPL Qt 6 libraries for building a Wayland desktop shell: the theme and popout foundation, the `Phosphor.*` QML
surfaces (bar, control center, launcher, OSD, notifications, lock, picker, dashboard) and the system service
libraries behind them. They build on [phosphor-libs](https://github.com/phosphor-works/phosphor-libs).

| Area | Libraries |
| --- | --- |
| Foundation | `phosphor-theme`, `phosphor-popout`, `phosphor-shell`, `phosphor-shell-widgets` |
| Surfaces | `phosphor-shell-bar`, `phosphor-shell-control-center`, `phosphor-shell-dashboard`, `phosphor-shell-launcher`, `phosphor-shell-lock`, `phosphor-shell-notifications`, `phosphor-shell-osd`, `phosphor-shell-picker`, `phosphor-shell-polkit`, `phosphor-shell-power` |
| Services | `phosphor-service-bluetooth`, `phosphor-service-brightness`, `phosphor-service-clipboard`, `phosphor-service-icontheme`, `phosphor-service-lock`, `phosphor-service-mpris`, `phosphor-service-network`, `phosphor-service-nightlight`, `phosphor-service-notifications`, `phosphor-service-pipewire`, `phosphor-service-polkit`, `phosphor-service-session`, `phosphor-service-sni`, `phosphor-service-upower` |

Each library lives in `libs/<name>/`. `examples/` holds demos and command-line acceptance harnesses for them.

## Build

Needs what phosphor-libs needs (CMake 3.23, 3.25 for the presets below, Qt 6.10 with its private headers,
extra-cmake-modules, Wayland), plus KF6 Kirigami, libxkbcommon, libpipewire, polkit-qt6 and PAM. The first configure
downloads the phosphor-libs commit that `cmake/PhosphorShellLibsDependencies.cmake` pins, and the archives that commit
pins, into `.cache/cpm`.

```sh
cmake --workflow --preset dev
```

That configures into `build/`, builds phosphor-libs and every library here, and runs the tests.
To build against a local phosphor-libs checkout, add `-DCPM_phosphor-libs_SOURCE=/path/to/phosphor-libs` to the
configure.

## Use from an app

An app adds phosphor-libs first, then this repository, both with [CPM](https://github.com/cpm-cmake/CPM.cmake) and
pinned to one commit each by archive URL and SHA-256. The app's phosphor-libs pin is then the one in use, and this
tree fetches nothing of its own:

```cmake
CPMAddPackage(NAME phosphor-shell-libs
              URL "https://github.com/phosphor-works/phosphor-shell-libs/archive/<commit>.tar.gz"
              URL_HASH SHA256=<sha256>)
```

To build the app against a local checkout instead, configure it with
`-DCPM_phosphor-shell-libs_SOURCE=/path/to/phosphor-shell-libs`.

## License

LGPL-2.1-or-later. See `COPYING.LESSER`.

Third-party files keep their own licences:

| File | Origin | Licence |
| --- | --- | --- |
| `libs/phosphor-service-nightlight/protocols/wlr-gamma-control-unstable-v1.xml` | [wlr-protocols](https://gitlab.freedesktop.org/wlroots/wlr-protocols) `unstable/`, commit `1106a4a1c57562ddd438c547da4c0d6dc150d624`, unchanged | HPND-sell-variant (in the file) |

The code wayland-scanner generates from a protocol file at build time carries that file's licence.
