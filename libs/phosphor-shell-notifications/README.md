<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# PhosphorShellNotifications

The `Phosphor.Notifications` toast framework: the transient notification popups that a shell shows when an app posts
a notification. If the placement map knows the window that a toast concerns, the toast anchors under the band of
that window. If not, it goes in a stack in the center of the top edge of the screen. It is pure QML, themed through
[`phosphor-theme`](../phosphor-theme/README.md) and built on the
[`phosphor-shell-widgets`](../phosphor-shell-widgets/README.md) atoms.

## Responsibility

The framework owns the lifecycle and layout of toasts:

- It stacks toasts at the top edge of the screen (newest on top), or anchors them to the window that they concern.
- It shows up to `maxVisible` toasts at once in the screen stack, and queues the rest.
- It dismisses each toast automatically after a timeout. The timeout starts again when the pointer leaves.
- It animates the entry, the exit and the reflow.
- It gives a seam where a rules service can suppress toasts or change their timing.

The framework owns presentation, queueing and timing only. Notification *ingest* (the
`org.freedesktop.Notifications` server) is a separate concern. The host feeds toasts in with calls to `show()`. The
toast demo connects `phosphor-service-notifications` to that call.

## Key types

| Component   | Role                                                                                       |
|-------------|--------------------------------------------------------------------------------------------|
| `ToastHost` | The toast stack: screen-edge and window-anchored placement, a queue beyond `maxVisible`, dismiss and promote, reveal and reflow transitions, a seam for rules for each app. Takes `screenName`, `placementMap`, `decoration` and reports `inputRects` / `anchoredCount`. |
| `Toast`     | One toast card: optional image, app name, summary, rich-text body, close, urgency accent, auto-dismiss that pauses on hover. |

## Use

```qml
import Phosphor.Notifications

ToastHost {
    id: toasts
    anchors.fill: parent
    maxVisible: 4
    rules: dndRules   // optional, see below
}

// when a notification arrives:
toasts.show({
    appName: "Mail",
    summary: "New message",
    body: "From <b>Ada</b>",          // StyledText markup
    imageSource: "file:///.../avatar.png",
    urgency: 1,                        // 0 low, 1 normal, 2 critical
    timeout: 5000                      // 0 = sticky
})
```

`show()` returns the toast id, or `-1` if a rule suppressed the toast. `dismiss(id)` dismisses one toast. `clear()`
removes all toasts, for example before a session lock. `toastDismissed(id)` fires each time a toast leaves, if it
was visible or still in the queue.

## Tests

A QtQuickTest suite tests `ToastHost` and `Toast`. The acceptance demo is `../../examples/phosphor-toast-demo/`.
`phosphor-service-notifications` feeds it, so `notify-send` shows a toast.

## Design notes

- **Queue, do not flood.** The host shows at most `maxVisible` toasts. The rest wait in the queue and move up when
  a slot is free. The newest toast is on top.
- **Pause on hover.** Each `Toast` owns its auto-dismiss timer. The timer stops while the pointer is over the toast,
  so a toast that you read stays open.
- **Motion through the view.** `ToastHost` uses a `ListView`. Its `add` / `remove` / `displaced` transitions draw
  the band out and back and reflow the stack, all with the Motion tokens of `phosphor-theme`. Nothing slides in from
  the side.
- **Rich text and image.** `Toast.body` is `StyledText` (the body markup subset of freedesktop notifications).
  If `imageSource` is set, the toast shows an image or avatar.
- **Seam for rules for each app.** Assign `ToastHost.rules`, an object that has
  `evaluate(toast) -> { suppress: bool, timeout: int } | null`. ToastHost calls it before it shows each toast.
  `suppress` drops the toast, and `timeout` replaces the auto-dismiss time. A rules editor connects to this seam
  without a change to ToastHost.

## Dependencies

- Qt6 ≥ 6.6 Core / Gui / Qml / Quick. `QtQuick.Shapes` (`Qt6::QuickShapes`) for the close glyph.
- `phosphor-theme` (`Phosphor.Theme`) for the tokens, Motion and `StateLayer`. `phosphor-shell-widgets`
  (`Phosphor.Widgets`) for `SpectrumStroke`, `SettleAnimation` and `DecorationSlot`. In-tree builds link their QML
  plugins automatically. This module is static and in-tree only.
