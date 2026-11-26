<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-service-session

A logind session manager for Phosphor-based desktop shells.

## Responsibility

The library gives the shell the system session and power actions over `org.freedesktop.login1`. The shell owns the
compositor and the session. Thus the library also manages the logind inhibitor locks. These locks let the shell
lock before the machine sleeps and own the power and lid keys. The library is the logind edge of the shell: it
covers capabilities, actions, inhibitors, and the session and sleep signals.

It has no UI of its own and binds no Wayland protocols. The logind D-Bus calls are private to the implementation.
The public API is Qt and QML types and an injectable `QDBusConnection` (the interface for tests and advanced
wiring).

- It reads the power capabilities (`CanSuspend` and the related methods) at the start and keeps them current.
- It starts the actions that the capabilities allow (lock, logout, suspend, hibernate, hybrid-sleep,
  suspend-then-hibernate, reboot, power-off, halt).
- It holds a logind delay inhibitor on `sleep`, so the shell can lock before suspend.
- It holds a block inhibitor on the power, suspend, hibernate and lid keys. Thus the shell decides what these keys
  do, not the logind defaults.
- It shows the `PrepareForSleep` signal of logind and the `Lock` / `Unlock` signals of the session.

This is more than a `systemctl` wrapper. A plugin shell (Quickshell / Noctalia) has no logind inhibitor integration.
Thus it cannot lock before a suspend that starts outside the shell (lid, `systemctl suspend`, idle), and it races
the lock surface. Because the shell owns the session, it closes that race.

## Key types

| Type | Role |
| --- | --- |
| `SessionHost` | The logind session host. Each capability is an `Availability` `Q_PROPERTY` (`canSuspend`, `canPowerOff`, ...). The actions are invokables (`lock()`, `logout()`, `suspend()`, `reboot()`, `powerOff()`, ...), each gated by its capability. `allowSleep()` is for the lock-before-sleep handshake. Signals: `aboutToSleep` / `lockRequested` / `unlockRequested` / `prepareForSleep`. `interactive` selects whether actions send authentication through a polkit agent. It is a plain instantiable QML type, not a singleton. |
| `SessionHost::Availability` | `Yes` / `No` / `NotApplicable` / `Challenge` / `Unknown`, parsed from the logind capability strings `yes` / `no` / `na` / `challenge`. The value is `Unknown` when logind is unreachable or the answer is not recognized. |

## Use

C++ shell composition root:

```cpp
#include <PhosphorServiceSession/QmlRegistration.h>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    PhosphorServiceSession::registerQmlTypes();
    // ... load shell.qml
}
```

A QML power menu, with the lock-before-sleep handshake connected to `phosphor-service-lock`. The two services stay
independent, and the shell connects them:

```qml
import Phosphor.Service.Lock 1.0
import Phosphor.Service.Session 1.0

SessionHost {
    id: session

    // Lock before the system sleeps: logind holds on our delay inhibitor until
    // we release it, so lock first and release once the surface is up.
    onAboutToSleep: lock.lock()
    onLockRequested: lock.lock()
}

LockService {
    id: lock
    onLockedChanged: if (locked) session.allowSleep()
}

// power-menu buttons bind enablement to the capability and call the action
Button {
    text: "Suspend"
    enabled: session.canSuspend === SessionHost.Yes || session.canSuspend === SessionHost.Challenge
    onClicked: session.suspend()
}
```

The handshake is `aboutToSleep` → `LockService.lock()` → `allowSleep()`. The shell connects it to
`phosphor-service-lock` in `SessionLockCoordinator.qml`. The lock surface is the lock screen of the shell.

`examples/phosphor-service-session-cli` is the worked example and the acceptance harness. It runs `status` and the
actions against a live logind:

```sh
# print the logind session capabilities
phosphor-service-session-cli status
# lock this session through logind
phosphor-service-session-cli lock
# suspend / reboot / power off (capability-gated; exits non-zero if unavailable)
phosphor-service-session-cli suspend
```

## Tests

| Test | What it covers |
| --- | --- |
| Smoke harness | Registration idempotency, inert construction, and the slot logic of the handshake and the session signals. |
| QML-engine facade load test | The QML module loads and the types work. |
| Behavior test against an in-process fake logind Manager and Session | Capability parsing, action routing with the interactive flag, refusal when a capability is missing, the inhibitor what and mode, the `PrepareForSleep` handshake over the bus, and lock, terminate and signal handling. |

## Design notes

- **Capabilities are methods, cached as properties.** logind gives `CanPowerOff` and the related values as `Manager`
  methods that return a string, not as D-Bus properties. `SessionHost` reads them asynchronously at construction and
  caches the parsed `Availability`. It reads them again on `refreshCapabilities()`. The answers change with
  inhibitor locks, lid state and swap availability.
- **Inhibitors are the point.** A delay inhibitor on `sleep` drives the lock-before-sleep handshake:
  1. On `PrepareForSleep(true)`, the host emits `aboutToSleep()` while it holds the inhibitor.
  2. The shell locks.
  3. When the lock surface is up, the shell calls `allowSleep()`.
  4. The host drops the inhibitor, and suspend continues with the session already locked.

  If the shell never confirms, a safety timeout drops the inhibitor. Thus a missing lock cannot block suspend after
  the `InhibitDelayMaxSec` of logind. A block inhibitor on
  `handle-power-key:handle-suspend-key:handle-hibernate-key:handle-lid-switch` gives the shell ownership of those
  keys. A `QDBusUnixFileDescriptor` holds each inhibitor fd, and teardown releases it.
- **Authentication goes through our own agent.** By default, actions pass `interactive=true` to logind. Thus an action
  that needs authorization (a `Challenge` capability) prompts through the polkit agent of the session
  (`phosphor-service-polkit`). The CLI sets `interactive=false`, because a development shell has no agent.
- **Complementary to `phosphor-service-lock`, not overlapping.** The lock service authenticates and owns the
  `ext-session-lock-v1` surface. This service owns the logind edge. The shell connects `aboutToSleep` /
  `lockRequested` to the lock service, and the `locked` of the lock service back to `allowSleep()`. Neither library
  depends on the other.
- **A single active host, not a model.** As for `phosphor-service-lock` / `phosphor-service-polkit` /
  `phosphor-service-idle`, the session is a single host with cached state, not a list model. There is no
  `ObjectManager`, because the Manager of logind is a single fixed object.

## Dependencies

- `phosphor-dbus` (private link) for the system-bus `Client`, the signal subscriptions and the `Inhibit` fd call.
- Qt6 >= 6.6 (Core, Qml, DBus). The CLI uses Qt6 Core only.
- A running logind (`org.freedesktop.login1`) on the system bus for the live path. Without it, the library loads
  inert: the capabilities are `Unknown`, the actions do nothing and log a warning, and the library takes no
  inhibitors.
- `phosphor-service-lock` and `phosphor-service-polkit` are collaborators that the shell connects. They are not link
  dependencies.
