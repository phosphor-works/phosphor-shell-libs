<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-service-polkit

A PolicyKit authentication agent for Phosphor-based desktop shells, built on the `polkit-qt6` binding.

## Responsibility

When an application requests a privileged action, `polkitd` calls the authentication agent that is registered for
the session. The agent drives the PAM conversation that authenticates the user. This library is that agent. It has
no UI. It shows the request and gives a respond and cancel path. A shell shows the authentication dialog.

- It registers with `polkitd` as the authentication agent of the session. This is opt-in, because the agent
  intercepts each authentication.
- It shows the active authentication request (action, message, identities and each PAM prompt). It drives the
  `polkit-qt6` `Agent::Session` that answers the request.
- It sends the response of the user directly to PAM and does not keep it.

The authentication dialog is not part of this library. `phosphor-shell-polkit` has the dialog surfaces.

## Key types

| Type | Role |
| --- | --- |
| `PolkitAgent` | Registers as the authentication agent of the session. It shows the active request and gives a respond and cancel path. It wraps the `Agent::Listener` of `polkit-qt6` privately, so its public API has no polkit-qt types. |
| `AuthRequest` | One decoded authentication request that polkit waits on (action / message / icon / details / identities, and the selected identity). The agent decodes the `initiateAuthentication` call of polkit into it. |

## Use

C++ shell composition root:

```cpp
#include <PhosphorServicePolkit/QmlRegistration.h>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    PhosphorServicePolkit::registerQmlTypes();
    // ... load shell.qml
}
```

QML authentication dialog (drives one request to completion):

```qml
import Phosphor.Service.Polkit 1.0

PolkitAgent {
    id: agent
    Component.onCompleted: registerAgent()

    onAuthenticationRequested: (request) => authDialog.show(request)
    // promptRequested is an EVENT, not a property change: it fires once per PAM
    // prompt INCLUDING a same-text retry after a wrong answer. Drive the dialog's
    // input field from it (not from a binding on request.prompt, which a
    // same-text retry would not re-notify). echo is false for secrets.
    onPromptRequested: (prompt, echo) => authDialog.askFor(prompt, echo)
    onAuthenticationCompleted: (gained) => authDialog.finish(gained)
}

AuthDialog {
    id: authDialog
    // request.message + request.identities choose who; askFor() shows each PAM
    // prompt as it arrives.
    onAuthenticate: agent.authenticate()
    onAnswered: (text) => agent.respond(text)   // straight to PAM, never stored
    onDismissed: agent.cancel()
}
```

The conversation has these steps:

1. `authenticate()` starts the PAM conversation.
2. The PAM prompt shows on `AuthRequest`.
3. `respond()` sends the answer directly to PAM. The library does not keep the secret.
4. `completed` / `cancel` resolve the result of polkit one time only.

## Example program

`examples/phosphor-service-polkit-cli` is the worked example and the acceptance harness. It runs as the agent. It
logs the requests, answers the prompts and tests the live path against `pkexec`.

```sh
# register as the session agent and answer prompts (stop the desktop agent first)
phosphor-service-polkit-cli
# in another terminal, trigger an action:
pkexec true
```

## Tests

The tests need no `polkitd`.

| Test | What it covers |
| --- | --- |
| C++ smoke harness | Inert construction, a failed registration, and the guards on the active request, `authenticate`, `respond` and `cancel`. |
| QML-engine facade load test | The QML module loads and `PolkitAgent` works in QML. |
| Decode unit test | The decode over real polkit-qt `Details` / `Identity` values. |
| Lifecycle test | The conversation steps with a fake authentication session. |

## Design notes

- **Wraps polkit-qt privately.** `PolkitAgent` is a plain `QObject`. The `PolkitQt1::Agent::Listener` that it
  subclasses is in the `.cpp`. polkit-qt6 is a private link, so consumers do not include or link it.
- **Registration is explicit.** `registerAgent()` makes the object the agent of the session. The constructor has no
  side effects. Only one agent serves a session. When the agent of the desktop (KDE / GNOME) already holds it, the
  registration fails and the object stays inert (`registered() == false`).
- **Show the request, never the secret.** The response of the user goes directly into
  `Agent::Session::setResponse`. The library never stores, logs or echoes it.
- **Dependency injection for tests.** The agent takes an injectable session id and object path. Thus the tests
  register against a synthetic session with no real `polkitd`. They cannot intercept the authentications of the
  tester.

## Dependencies

- `polkit-qt6` (Core + Agent), found through `find_package(PolkitQt6-1)`. The library does not use the GUI
  action-button widgets of polkit-qt6.
- A running `polkitd` on the system bus for the live path. Without it, the library loads inert.
- Qt6 ≥ 6.6 (Core, Qml).
