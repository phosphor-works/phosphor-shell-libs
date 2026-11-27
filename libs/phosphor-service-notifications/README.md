<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-service-notifications

The `org.freedesktop.Notifications` (Desktop Notifications Specification 1.3) server for Phosphor-based desktop
shells.

## Responsibility

This library is the notification daemon. It owns the well-known name and answers it. It is not a client of another
daemon. It has no UI. It stores the notifications, manages their lifecycle and gives them to a shell as Qt and QML
types.

- It acquires `org.freedesktop.Notifications` on the session bus. It answers the four methods (`Notify`,
  `CloseNotification`, `GetCapabilities`, `GetServerInformation`) and sends the signals (`NotificationClosed`,
  `ActionInvoked`, `ActivationToken`).
- It owns the notification lifecycle: id allocation, `replaces_id` reuse, expiry for each urgency, user dismissal,
  action invocation and close reasons.
- It decodes the hint set (urgency, category, desktop-entry, image-data / image-path, resident, transient,
  suppress-sound, value) into a typed `Notification`. It shows the live set through a `NotificationModel`.

Toasts, the notification center, rules for each app and markup rendering are not part of this library. A shell
supplies them. `phosphor-shell-notifications` has the toast and notification center surfaces.

## Key types

| Type | Role |
| --- | --- |
| `NotificationServer` | Owns the bus name and the notification lifecycle. It is the forwarding target of the generated `org.freedesktop.Notifications` adaptor. `dismissNotification` / `invokeAction` are `Q_INVOKABLE` for a UI. They are never exported on the bus. |
| `Notification` | One decoded live notification (summary / body / actions / urgency / image / hints). A `replaces_id` changes it in place. |
| `NotificationModel` | `QAbstractListModel` over the live notifications of a server (bind its `server` property). The rows follow add, replace and close. |

## Use

C++ shell composition root:

```cpp
#include <PhosphorServiceNotifications/QmlRegistration.h>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    PhosphorServiceNotifications::registerQmlTypes();
    // ... load shell.qml
}
```

QML consumer (one toast for each live notification):

```qml
import Phosphor.Service.Notifications 1.0

NotificationServer { id: notifications }
NotificationModel { id: model; server: notifications }

ListView {
    model: model
    delegate: ToastDelegate {
        summary: model.summary
        body: model.body
        critical: model.urgency === Notification.Critical
        onActioned: (key) => notifications.invokeAction(model.id, key, activationToken)
        onDismissed: notifications.dismissNotification(model.id)
    }
}
```

`examples/phosphor-service-notifications-cli` is a CLI like `lambentctl` with the subcommands watch, send, close
and info. It runs as the daemon and logs the notifications that it receives.

## Capabilities

`GetCapabilities` advertises these capabilities:

| Capability | Advertised |
| --- | --- |
| `body` | Yes |
| `actions` | Yes |
| `icon-static` | Yes |
| `persistence` | Yes |
| `body-images` | Yes |
| `inline-reply` | Yes |
| `body-markup` | No. The library has no markup renderer. |

## Tests

Three test binaries test the contract over a private peer bus. They need no session daemon.

| Test | What it covers |
| --- | --- |
| Smoke harness (direct calls) | Id allocation and `replaces_id`, hint decode, the expiry, dismiss and action lifecycle, the model roles and add / replace / close under `QAbstractItemModelTester`. |
| QML-engine facade load test | The QML module loads and the types work. |
| Peer-to-peer wire test | Real marshalling, which includes the decode of the `image-data` struct and the rejection of input that is too large. |

## Design notes

- **Generated adaptor, not direct export.** `qt6_add_dbus_adaptor` generates the interface from
  `src/dbus/org.freedesktop.Notifications.xml`. All four methods reply synchronously. Thus a hand-written dispatch
  with a delayed reply is not necessary, as it is for the `Agent1` of `phosphor-service-bluetooth`.
- **Name conflict is inert, with opt-in takeover.** Only one process can own the name. When another daemon (dunst /
  mako / Plasma) holds it, the server reports `nameAcquired() == false` and stays inert. `acquireName(true)` (the
  `--replace` of the CLI) takes over an owner that allows replacement. It is never a forced default.
- **Image decode, once.** The `image-data` `(iiibiiay)` hint is demarshalled inline (`QDBusArgument`) into a
  deep-copied `QImage`. If there is no `image-data`, `image-path` loads a file or names an icon-theme icon. This is
  the only `Qt::Gui` use of the library, and it is limited to `QImage`. The raw hint map stays on `Notification` for
  advanced bindings. The `image` property and model role is a `QImage`, not an `Image.source`. To paint it, a QML
  delegate needs a `QQuickImageProvider` keyed on the notification id. A shell toast supplies one.
- **Markup stays raw.** The library stores `body` markup and never renders it.
- **Dependency injection for tests.** `NotificationServer` takes an injectable `(QDBusConnection, serviceName)`. Thus
  the ingest path runs over a private peer bus with no real session daemon and no name conflict. The peer-to-peer
  wire test covers the `image-data` decode, which needs real marshalling.

## Dependencies

- A free `org.freedesktop.Notifications` name on the session bus. If another notification daemon runs, this server
  stays inert. To take over the name, use `--replace`.
- `PhosphorServiceIconTheme` (private link): it resolves an `image-path` that is an icon name.
- Qt6 ≥ 6.6 (Core, Qml, DBus, Gui). The library uses Gui only to decode `QImage`.
