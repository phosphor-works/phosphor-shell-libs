<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# phosphor-service-clipboard

This library is a clipboard-history service for Phosphor-based desktop shells.

## Responsibility

The library watches the session clipboard, keeps a de-duplicated, capped history on disk, and can apply any entry
again. It is the policy, history and persistence layer over `phosphor-wayland`'s `ClipboardDevice` (a
`wlr-data-control` client). It composes the device and does not bind the protocol itself. Thus its public interface
is a clean Qt/QML type, with no Wayland types. It shows the history as a model, and the shell shows the picker. The
library has no UI.

- Watch the clipboard independent of keyboard focus (`wlr-data-control`).
- Keep a de-duplicated, capped history that stays after a restart.
- Never read or keep a sensitive selection (password-manager hints).
- Apply a history entry to the clipboard again on request.

## Key types

| Type | Role |
| --- | --- |
| `ClipboardService` | The clipboard-history host. Exposes `history` (a list model, most recent first, with the roles `preview` / `mimeType` / `offeredTypes` / `timestamp`), `count` and `supported`. `copy(index)` applies an entry again. `remove(index)` and `clear()` remove entries. It loads from disk when it is constructed, and saves on every change. It is a plain QML type that you can instantiate, not a singleton. |

## Use

In the C++ composition root of the shell:

```cpp
#include <PhosphorServiceClipboard/QmlRegistration.h>

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    PhosphorServiceClipboard::registerQmlTypes();
    // ... load shell.qml
}
```

A QML clipboard picker that binds the history and pastes the selected entry:

```qml
import Phosphor.Service.Clipboard 1.0

ClipboardService {
    id: clipboard
}

ListView {
    model: clipboard.history
    delegate: ItemDelegate {
        required property int index
        required property string preview
        text: preview
        onClicked: clipboard.copy(index) // re-apply this entry to the clipboard
    }
}
```

The CLI is also the worked example and the acceptance harness:

```sh
# watch the clipboard and log each new entry
phosphor-service-clipboard-cli watch
# print the persisted history
phosphor-service-clipboard-cli list
# re-apply history entry 2 (serves the selection until Ctrl+C)
phosphor-service-clipboard-cli copy 2
```

## History limits

| Item | Value |
| --- | --- |
| Entries kept | 100 by default, most recent first. Past the cap, the oldest entry goes. |
| Duplicates | A new copy of an entry moves the entry to the front. |
| Sensitive selections | The library drops a selection with `x-kde-passwordManagerHint` before it reads it. |
| Location | A JSON index and SHA-256 content blobs under `~/.local/share/phosphor-works/clipboard`. |

## Tests and demo

The `examples/phosphor-service-clipboard-cli` demo gives `watch` / `list` / `copy` against a live session. Four
test binaries cover the deterministic behavior with no `wlr-data-control`:

| Test | What it covers |
| --- | --- |
| Smoke harness | Registration idempotency and inert construction. |
| QML-engine load test | The QML type loads in an engine. |
| History-model unit test | The preferred-MIME selection, the move to the front on a duplicate, cap eviction, the sensitive drop and preview generation. |
| Store unit test | The text and binary round trip, sensitive exclusion, orphan-blob pruning and recovery from a corrupt index. |

## Design notes

- **Composes the foundation primitive.** The `wlr-data-control` client is in `phosphor-wayland`
  (`ClipboardDevice`). This library links it privately and adds the history, de-duplication, persistence and model
  layer. It binds no protocols itself.
- **Reads asynchronously.** The library reads each new selection from a pipe on the event loop. `receive` returns
  immediately, and the bytes arrive through a signal. Thus a large or slow producer never blocks the UI. An
  `IClipboardSource` seam drives the model, so unit tests check its policy with a fake source and no compositor.
- **Never persist secrets.** The library drops a selection that has a sensitivity hint (the KDE
  `x-kde-passwordManagerHint`) completely. It never reads, keeps or writes it. The on-disk store also refuses
  sensitive entries as a second defense. `phosphor-service-polkit` has the same rule: never store the secret.
- **On-disk history.** The store is a JSON index and one content blob for each entry under
  `~/.local/share/phosphor-works/clipboard`. A blob is named by its SHA-256, so identical content shares a blob.
  Writes are atomic, and the store removes orphaned blobs. The history loads at startup and saves on every change.
- **A model, not a single host.** A clipboard history is a list. The facade therefore exposes a
  `QAbstractItemModel`, the host-backed-model shape of `phosphor-service-notifications`. This is different from the
  single-active-item shape of `phosphor-service-polkit` / `phosphor-service-idle`.

## Dependencies

- `phosphor-wayland` (private link: the `ClipboardDevice` `wlr-data-control` client and the vendored protocol code).
  No separate `wayland-protocols` dependency.
- Qt6 >= 6.6 (Core, Qml). The CLI also uses Qt6 Gui for `QGuiApplication`.
- A compositor that advertises `wlr-data-control-unstable-v1`, for the live path. Without it the library loads
  inert, and the saved history still loads and reads.
