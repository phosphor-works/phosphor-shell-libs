<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-service-lock

This library is a session-lock and authentication service for Phosphor-based desktop shells.

## Responsibility

The library authenticates the session user through PAM. It coordinates the `ext-session-lock-v1` lock state with
the compositor. It is the policy and state-machine layer over `phosphor-wayland`'s `SessionLock` client (an
`ext-session-lock-v1` client) and a PAM authentication backend. It composes them, and its public interface does not
bind the protocol or talk to PAM. Thus its public types are clean Qt/QML types, with no Wayland or PAM types.

- Verify the password of the session user through PAM, off the GUI thread.
- Ask the compositor to lock the session, and track the lock lifecycle.
- Release the lock after authentication succeeds.

This service owns authentication and the lock lifecycle. It does not render. The lock *surfaces* (the graphics on
each output while the session is locked) are a shell concern. [phosphor-shell-lock](../phosphor-shell-lock/)
provides them as `Phosphor.Lock.LockSurface`.

## Key types

| Type | Role |
| --- | --- |
| `LockService` | The lock host. `lock()` asks the compositor to lock (state `Locking` → `Locked`). `unlock(password)` authenticates the session user (`Authenticating`). On success it releases the lock (`Unlocked`, `unlocked()`). On failure `authenticationFailed()` fires and the state goes back to `Locked`. Exposes `supported`, `state` and `locked`. It is a plain QML type that you can instantiate, not a singleton. |
| `PamAuthenticator` / `IAuthenticator` | The standalone credential check. `authenticate(user, password)` runs PAM off the GUI thread and reports `succeeded()` / `failed(reason)`. The PAM service name is configurable (default `login`). Use it to verify a password without the compositor lock. |

## Use

In the C++ composition root of the shell:

```cpp
#include <PhosphorServiceLock/QmlRegistration.h>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    PhosphorServiceLock::registerQmlTypes();
    // ... load shell.qml
}
```

A QML lock screen. A lock surface of the shell drives the service:

```qml
import Phosphor.Service.Lock 1.0

LockService {
    id: lock
    onUnlocked: console.log("session unlocked")
    onAuthenticationFailed: (reason) => console.log("denied:", reason)
}

// request the lock when the session should be secured
Component.onCompleted: lock.lock()

// the password field submits an unlock attempt
TextField {
    echoMode: TextInput.Password
    onAccepted: lock.unlock(text)
}
```

The CLI is also the worked example and the acceptance harness:

```sh
# verify a password for the current user through PAM (prompts; echo disabled)
phosphor-service-lock-cli authenticate
# authenticate a specific user against a specific PAM stack
phosphor-service-lock-cli authenticate alice --service phosphor-lock
# report whether the compositor advertises ext-session-lock-v1
phosphor-service-lock-cli supported
```

## Lock states

`LockService` goes through the states `Unlocked → Locking → Locked → Authenticating`. A successful unlock releases
the lock. A failed attempt keeps the session locked.

## Tests and demo

The `examples/phosphor-service-lock-cli` demo gives `authenticate` (PAM, prints success or failure) and `supported`
against a live session. Four test binaries cover the deterministic behavior with no compositor and no valid
credentials:

| Test | What it covers |
| --- | --- |
| Smoke harness | Registration idempotency and inert construction. |
| QML-engine load test | The QML type loads in an engine. |
| PAM authenticator test | The configured service, fast failure, the concurrency guard, and real PAM rejection of a user that does not exist. |
| State-machine test | The full lock policy on fakes. |

## Design notes

- **Composes the foundation primitive.** The `ext-session-lock-v1` client is in `phosphor-wayland`
  (`SessionLock`). This library links it privately and adds the authentication and lock state machine. It binds no
  protocols itself. The client holds the protocol teardown rules: never destroy a locked session, and keep the
  guarantee that the session stays locked if the client dies.
- **Authenticates off the GUI thread.** `PamAuthenticator` runs the blocking PAM transaction (`pam_authenticate` +
  `pam_acct_mgmt`) on the global thread pool. It sends the result back to the GUI thread. Thus a PAM module that
  sleeps (faildelay, a network backend) never stops the event loop. One transaction runs at a time.
- **Password handling.** The password answers only the echo-off prompt of PAM. It moves to one owner, and the
  library wipes it (`explicit_bzero`) after the transaction. The library never logs it.
- **Seams for testability.** An `ISessionLock` seam and the `IAuthenticator` seam drive the lock state machine.
  Thus unit tests check the full lock policy with fakes and no live compositor or PAM stack. The policy is request,
  locked, authenticate, unlock, with a retry after an authentication failure and a teardown that the compositor
  starts.
- **One active lock, not a model.** The lock is one state, exposed as a `State` enum and not as a list model. This is
  the same as `phosphor-service-polkit` / `phosphor-service-idle`, and different from `phosphor-service-clipboard`.
- **Lock surfaces.** A real lock screen must create an `ext_session_lock_surface_v1` for each output before the
  compositor shows the locked frame. That rendering layer is a shell concern. The compositor decides by its own
  policy when to confirm the lock.

## Dependencies

- `phosphor-wayland` (private link: the `SessionLock` `ext-session-lock-v1` client and the vendored protocol code).
  No separate `wayland-protocols` dependency.
- PAM (`libpam`, private link) for authentication.
- Qt6 >= 6.6 (Core, Qml, and Concurrent privately for the PAM transaction off the GUI thread). The CLI also uses Qt6
  Gui for `QGuiApplication`.
- A compositor that advertises `ext-session-lock-v1`, for the live lock path. Without it the library loads inert, and
  authentication still works.

## See also

- [phosphor-shell-lock](../phosphor-shell-lock/): the lock surface and lock-screen QML.
