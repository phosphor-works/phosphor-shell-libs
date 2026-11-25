<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-popout

The central popout coordinator for Phosphor shells. It is the single arbiter for popout lifetime, focus and
exclusivity policy across all transient surfaces of the shell.

## Responsibility

A desktop shell has many transient surfaces, for example the control center, the launcher, the calendar, the
notification history, OSDs and tray menus. Each one needs a Wayland layer-shell surface, keyboard-focus arbitration,
screen affinity, exclusive-zone handling, dismissal on focus loss, and open and close animations. If each surface
has its own copy of this machinery, two popups can fight over a Wayland grab. All popout requests go through
`phosphor-popout`, so the policy is the same for each surface.

- **One arbiter, many transports.** The arbitration state machine is in `PopoutController`. `IPopoutTransport` hides
  the creation of the layer-shell surface. The shell's `LayerPopoutTransport` connects it to
  `phosphor-layer::SurfaceFactory` and gives each popout its own layer-shell overlay surface. The in-app demo
  transport is the acceptance harness. Tests inject a fake transport and drive the controller as pure logic, with
  no Wayland.
- **Exclusivity modes.** Cooperative is the default: one popout for each scope, and a new one replaces the previous
  one. Modal suppresses all Cooperative popouts in all scopes while it is open. Detached is independent of both. The
  state machine handles all modes through one `open` entry point.
- **Stable handles.** Each successful open returns an opaque handle, and `close` takes that handle. The logical
  identity is separate, in `popoutId`. `toggle("control-center")` works correctly for any handle that the open
  instance has. The controller does not give transport-specific identifiers to QML or to other consumers.

## Key types

| Type | Purpose |
|------|---------|
| `PhosphorPopout::PopoutRequest`     | Value type. Q_GADGET, registered as QML value type `popoutRequest`. Describes a popout to open: id, content, screen, anchor, exclusivity, scope, focus flags, props |
| `PhosphorPopout::ExclusiveMode`     | `Cooperative` / `Modal` / `Detached` arbitration modes. Q_ENUM_NS, available in QML as `ExclusiveMode.Cooperative` and so on |
| `PhosphorPopout::Anchor`            | The position on the screen where the popout anchors. Values are `BarLeft`, `BarCenter`, `BarRight`, `ScreenCenter`, `AtPointer`, `Custom`. Q_ENUM_NS |
| `PhosphorPopout::IPopoutService`    | Abstract service. Methods are `open`, `close`, `toggle`, `isOpen`, `closeAll` |
| `PhosphorPopout::IPopoutTransport`  | Transport seam. Implementations create, remove and track layer-shell surfaces. They tell the controller when a surface dismisses itself |
| `PhosphorPopout::PopoutController`  | Concrete `IPopoutService`. Owns the arbitration state machine and gives surface creation to an injected transport. Registered as a QML element, but uncreatable. Make it in C++ and publish it with `qmlRegisterSingletonInstance` |
| `Phosphor.Popout.PopoutHost` (QML)  | Transport-agnostic wrapper for popout content. Owns the open and close animation (opacity and scale), the backdrop dim and the click-outside dismissal. Both transports make this host around the content delegate |

### `PopoutHost.qml` properties and signals

| Member | Kind | Purpose |
|--------|-----------|---------|
| `contentComponent: Component` | property | A QML Component that the host makes inline with a Loader. Do not set it together with `contentItem` |
| `contentItem: Item`           | property | A pre-built Item that the host reparents under its content frame. If both are set, `contentItem` wins over `contentComponent` |
| `open: bool`                  | property | Drives the show and hide animation. The transport sets it to true after it parents the surface, and to false to start teardown |
| `dismissOnClickOutside: bool` | property | If true, a click on the backdrop sets `open = false`. The transport sets it from `PopoutRequest.dismissOnFocusLoss` |
| `keyboardFocus: bool`         | property | Whether the popout takes the keyboard while it is open. The transport sets it from `PopoutRequest.keyboardFocus`. It is separate from `dismissOnClickOutside`, because keyboard input and dismissal are different questions. A host with this property false gets no key events, so Escape does not dismiss it. A click outside is then its only way to dismiss itself |
| `backdropColor: color`        | property | Background dim color. The transport sets a scrim for Modal, a lighter dim for Cooperative, and transparent for Detached |
| `dismiss()`                   | function | Lets a content delegate at any depth close the popout without a walk up the parent chain. The transport injects a `_popoutHost` reference on the content, and the content calls `_popoutHost.dismiss()` |
| `dismissed()`                 | signal   | Emitted after the close animation ends, once for each open-then-close cycle. A transport that uses one host for many popouts thus gets a new `dismissed` each time. It also fires once on `Component.onDestruction` if the host was opened and is destroyed before the close-animation timer emits it. Thus the transport does not leak a handle when a popout is destroyed in the middle of a cycle. A host that is made and destroyed without `open` ever true does NOT fire `dismissed`. Thus a transport that pre-builds and discards surfaces sees no false `dismissed`. The transport's `dismissed` callback fires through this signal, so the bookkeeping agrees with the visual state |

## Use

**C++. Connect a controller to a transport and send a request.**

```cpp
#include <PhosphorPopout/PhosphorPopout.h>
#include <QObject>

using namespace PhosphorPopout;

MyPopoutTransport transport(...);                 // implements IPopoutTransport
PopoutController controller(&transport);          // transport must outlive the controller

QObject::connect(&controller, &PopoutController::popoutOpened,
                 [](const QString& id, const QString& handle) {
                     qDebug() << "opened" << id << "->" << handle;
                 });

QQmlComponent* controlCenterComponent = ...;      // caller owns; keep alive until popoutClosed

PopoutRequest req;
req.popoutId = QStringLiteral("control-center");
req.content = controlCenterComponent;
req.anchor = Anchor::BarRight;                    // BarLeft / BarCenter / BarRight / ScreenCenter
                                                  // / AtPointer / Custom. Custom uses req.customAnchor.
req.exclusive = ExclusiveMode::Cooperative;
const QString handle = controller.open(req);      // empty if rejected
```

**QML. Toggle a popout from a button.** Register the controller with `qmlRegisterSingletonInstance` in the engine
setup, so that all of the UI uses the same arbiter. The example registers the singleton as `Popouts`.

```qml
import QtQuick
import QtQuick.Controls
import Phosphor.Popout

ToolButton {
    text: qsTr("Toggle calendar")

    Component {
        id: calendarComponent
        Rectangle { implicitWidth: 320; implicitHeight: 220; color: "white" }
    }

    onClicked: Popouts.toggle({
        "popoutId": "calendar",
        "content": calendarComponent,
        "anchor": PhosphorPopout.Anchor.BarCenter,
        "exclusive": PhosphorPopout.ExclusiveMode.Cooperative
    })
}
```

Two rules apply to that call. If you break one, the call fails only at runtime.

1. The object literal converts to a `popoutRequest` because the value type is declared `QML_STRUCTURED_VALUE`.
   Without that macro, QML cannot convert a JS object to the gadget, and the call raises
   `TypeError: Passing incompatible arguments to C++ functions from JavaScript is not allowed`.
2. Reach `Anchor` and `ExclusiveMode` THROUGH `PhosphorPopout`. `QML_ELEMENT` on a `Q_NAMESPACE` publishes the
   namespace as the QML type name, and does not put the enums in scope unqualified. A bare `Anchor.BarCenter` is a
   `ReferenceError`.

`tests/test_popout_qml_enums.cpp` tests both rules.

## Arbitration

- **Cooperative for each scope.** A second `Cooperative` request in the same scope closes the prior popout and then
  opens the new one. Different scopes are independent. The default scope is `"default"`. For one cooperative popout
  on each output, a shell sets `scope = "screen-DP-1"`.
- **Modal suppresses all Cooperative popouts.** A `Modal` request closes all Cooperative popouts in all scopes.
  While a Modal is open, the controller rejects new Cooperative requests, and open returns an empty string. Modals
  stack: a second Modal does not close the first. The controller does NOT restore Cooperative popouts when the Modal
  closes. The user asked for the Modal, and a reopened popout would take the focus from the item that the user
  focused after the dismissal.
- **Detached ignores arbitration.** Detached popouts stay open across cooperative swaps and modal opens. They close
  only on an explicit close, or when their surface dismisses itself.
- **Same-id collision.** The controller rejects a second `open` with a popoutId that is already open, for each
  ExclusiveMode. To open a new instance, a caller closes the old handle first.

## Dependencies

- `QtCore`, `QtGui`, `QtQml`, `QtQuick`. The C++ core links the first three. The QML module also links Quick.
- The Qt module `Phosphor.Theme`. PopoutHost.qml uses the `Motion` durations and easings for its animation timing.
  The C++ core has no Phosphor link-time dependency.
- The QML module target (`PhosphorPopout::PhosphorPopoutQml`) is the in-tree linkable artifact. Out-of-tree
  consumption through `find_package(PhosphorPopout)` gives only the C++ core (`PhosphorPopout::PhosphorPopout`).
  An in-tree consumer that links `PhosphorPopout::PhosphorPopoutQml` also gets the PhosphorTheme QML plugin. Thus
  the runtime `import Phosphor.Theme` in PopoutHost.qml resolves with no more setup in the consumer.

## See also

- `phosphor-libs/libs/phosphor-layer/`. The policy layer over `wlr-layer-shell` that the shell's
  `LayerPopoutTransport` uses.
