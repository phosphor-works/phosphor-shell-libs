<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-service-sni

A system-tray host for Phosphor-based desktop shells. It implements `org.kde.StatusNotifierItem` /
`org.kde.StatusNotifierWatcher` and the `com.canonical.dbusmenu` context-menu model.

## Responsibility

The library registers the well-known `org.kde.StatusNotifierWatcher` service on the session bus. It advertises hosts
through `RegisterStatusNotifierHost`. It watches `NameOwnerChanged` to find tray items that appear and disappear. It
gives each live item and its context menu to QML as a Qt model.

It has no UI. The shell decides how to show the tray slot (capsule, dock, panel widget, free-floating overlay).

## Key types

| Type | Role |
| --- | --- |
| `StatusNotifierHost` | The host of one shell. It registers the watcher service if it is absent, declares itself as a host and owns the live items. |
| `StatusNotifierWatcher` | Implements `org.kde.StatusNotifierWatcher`, so apps and other hosts can find the host. |
| `StatusNotifierItem` | Live proxy for one tray item. It gives the icon (`QImage`), tooltip, status and menu path. The URL forms are role data on `StatusNotifierItemModel`. |
| `StatusNotifierItemModel` | `QAbstractListModel` over the items of the host. The roles include id/title/status/iconUrl/menuPath. |
| `DBusMenuModel` | `QAbstractListModel` for one level of a `com.canonical.dbusmenu` tree. Cascaded popups bind a new model for each level. The QML side prefers this to a hierarchical model. |

## Use

C++ shell composition root. Mount the icon image provider on each `QQmlEngine` that the shell constructs. If the
shell hot-reloads (a new engine for each reload), the hook for each engine keeps the provider mounted after the
rebuild:

```cpp
#include <PhosphorServiceSni/QmlRegistration.h>
#include <PhosphorServiceIconTheme/QmlRegistration.h>
#include <PhosphorShell/ShellEngine.h>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    PhosphorServiceSni::registerQmlTypes();
    PhosphorServiceIconTheme::registerQmlTypes();

    // For a one-shot QQmlApplicationEngine, install once after
    // construction:
    //   QQmlApplicationEngine engine;
    //   PhosphorServiceIconTheme::installImageProvider(&engine);
    //
    // For a hot-reloading shell that builds a fresh QQmlEngine per
    // reload, register the install as an engine hook so it fires on
    // every rebuild:
    PhosphorShell::ShellEngine engine{...};
    engine.addEngineHook([](QQmlEngine* qmlEngine) {
        PhosphorServiceIconTheme::installImageProvider(qmlEngine);
    });
    // ... load shell.qml
}
```

QML consumer:

```qml
import Phosphor.Service.Sni 1.0

StatusNotifierHost { id: tray }
StatusNotifierItemModel { id: items; host: tray }

Repeater {
    model: items
    delegate: Image {
        source: iconUrl       // already an image://phosphor-service-icontheme/ URL
        sourceSize: Qt.size(20, 20)
    }
}
```

## Design notes

- **Watcher election.** The watcher service has a well-known bus name (`org.kde.StatusNotifierWatcher`). It is
  normal that several shells race for it during a compositor restart. The shell that gets the name owns the watcher.
  The other shells observe `NameOwnerChanged` and use the winner. `StatusNotifierHost` does this automatically. Thus
  if you write two shells, both see the tray.
- **Icon publishing.** An item can supply an `IconName` (a theme lookup through `phosphor-service-icontheme`), an
  `IconThemePath` (a custom directory for icons that are not in the system), inline `IconPixmap` blobs, or a
  combination. The model makes one URL for each item from all three, in the form
  `image://phosphor-service-icontheme/<service|path[#variant]>?v=<cacheKey>`. QML's `Image` element compares only
  URL strings. Thus the `?v=` cache key forces it to load the image again when the data changes.
- **`com.canonical.dbusmenu`.** Menus are hierarchical and lazy. `AboutToShow` fills a level on demand, and `Event`
  sends an activation. `DBusMenuModel` is a flat `QAbstractListModel` over one level of the tree. Cascaded popups
  bind a new model with its root at the id of the parent row. The QML side shows submenus as cascading popups. A
  flat model for each level fits this better than a tree-shaped model.
- **dbustypes forced-include.** The generated D-Bus interface headers use custom marshalled types
  (`DBusImageList`, `DBusToolTip`, `DBusMenuLayoutItem`) but do not `#include` `dbustypes.h`. The build forces the
  include into each TU (`-include src/dbustypes.h`). Thus AUTOMOC makes valid output for the generated headers alone.
  The unity build is disabled for the same reason. The `-include` flag works only with GCC and Clang. The project
  targets Linux/Qt6, so it does not support MSVC.

## Dependencies

- Qt6 ≥ 6.6 (Core, Gui, Qml, DBus)
- `phosphor-service-icontheme` (XDG icon-theme resolver and Qt image provider)
- A running session bus
