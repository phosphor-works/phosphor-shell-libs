<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-service-icontheme

This library finds icons by the XDG Icon Theme Specification 0.13. It also gives the Qt image provider that lets QML
show the resulting `QImage` payloads through a `QUrl` source.

## Responsibility

- **`IconThemeResolver`** walks the themes as the specification says. It finds the active theme through
  `QIcon::themeName()`. The Qt platform plugin gets that name from Plasma, GTK or xsettings, as available. The
  resolver parses the `index.theme` of each theme and follows the `Inherits=` chains. It falls back to Hicolor, or to
  a direct file-system search for absolute paths. `iconForName(name, size, scale, extraThemeDir)` returns the
  best-match `QImage` by the distance algorithm of the specification.
- **`IconImageProvider`** is a Qt image provider at `image://phosphor-service-icontheme/`. A publisher (for example,
  a tray-item model that holds `QImage` payloads from D-Bus) calls `IconImageProvider::setImage(id, image)`. The
  consumer binds `Image.source` to a URL of the form `image://phosphor-service-icontheme/<id>?v=<cacheKey>`. Qt sends
  that URL back through `requestImage()`, which reads the QImage from the thread-safe static registry. The `?v=`
  suffix only makes QML's `Image` element fetch again when the QImage data changes. QML's `Image` reloads only when
  the URL string is different.

## Key types

| Type | Role |
| --- | --- |
| `IconThemeResolver` | Singleton XDG icon resolver. `iconForName`, and the static `decodePixmaps` for raw IconPixmap byte blobs. |
| `IconImageProvider` | `QQuickImageProvider` with a process-global registry. Publishers call `setImage` / `clearImage`. |

## Use

In the C++ composition root of the shell:

```cpp
#include <PhosphorServiceIconTheme/QmlRegistration.h>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    PhosphorServiceIconTheme::registerQmlTypes();          // exposes IconThemeResolver singleton
    QQmlApplicationEngine engine;
    PhosphorServiceIconTheme::installImageProvider(&engine); // mounts image://phosphor-service-icontheme/
    // ... load shell.qml
}
```

A QML consumer. The resolver finds an XDG icon. The image provider then gives the resulting QImage to QML's
`Image.source` as a URL:

```qml
import Phosphor.Service.IconTheme 1.0
import QtQuick

Image {
    // IconThemeResolver.iconForName returns a QImage, which Image.source
    // (a QUrl) cannot bind to directly. The C++ side must call
    // IconImageProvider::setImage(id, image) and then expose the URL
    // form below; the snippet here illustrates the QML end of that
    // contract.
    property string trayKey: "tray-audio-volume-high"
    source: "image://phosphor-service-icontheme/" + trayKey + "?v=" + cacheVersion
    property int cacheVersion: 0
    sourceSize: Qt.size(24, 24)
}
```

A publisher. This is usually inside a service that holds raw `QImage` payloads, for example the tray-item model of
phosphor-service-sni:

```cpp
#include <PhosphorServiceIconTheme/IconImageProvider.h>
#include <PhosphorServiceIconTheme/QmlRegistration.h>

const QString trayKey = QStringLiteral("tray-audio-volume-high");
PhosphorServiceIconTheme::IconImageProvider::setImage(trayKey, qimage);
// then expose the QML-bindable URL:
const QString url = QStringLiteral("image://")
    + QString::fromLatin1(PhosphorServiceIconTheme::imageProviderUrlHost())
    + QLatin1Char('/') + trayKey + QStringLiteral("?v=")
    + QString::number(qimage.cacheKey());
```

## Consumers

| Consumer | Use |
| --- | --- |
| `phosphor-service-sni` | Calls `IconThemeResolver::instance()->iconForName(...)` for XDG-themed tray icons. Sends raw IconPixmap blobs through `IconImageProvider::setImage` for QML binding. |
| `phosphor-service-notifications` | Calls `IconThemeResolver::instance()->iconForName(...)` to resolve notification icons. |

The library also registers the QML singleton (`IconThemeResolver` under `Phosphor.Service.IconTheme 1.0`) for bar
widgets and third-party shells.

## Design notes

- **Static registry, not one for each engine.** A shell can construct more than one `QQmlEngine` across reload
  cycles. The icon payload is valid in all of them. One coarse mutex is sufficient at human update rates.
- **Stable URL host segment.** The `imageProviderUrlHost()` accessor exists so that publishers do not hard-code the
  host string. A rename of the host then breaks the link openly. It does not fail silently at runtime.
- **`?v=` cache busting.** `Image` fetches again only when the URL changes. The publisher appends `?v=cacheKey()`,
  so a new `setImage` for the same id forces a new binding. `requestImage` removes the query before the lookup.

## Dependencies

- Qt6 ≥ 6.6 (Core, Gui, Qml, Quick)
