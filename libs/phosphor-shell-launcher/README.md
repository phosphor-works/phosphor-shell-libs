<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# PhosphorShellLauncher

The launcher: a spotlight-style search over installed applications, open windows, clipboard history, arithmetic
and shell commands. It follows
[Lambent's `docs/design/mockups-v2/launcher.svg`](https://github.com/phosphor-works/Lambent/blob/main/docs/design/mockups-v2/launcher.svg).

## Responsibility

The directory has two targets. The C++ core library (`PhosphorShellLauncher`) finds and ranks results, and any host
can link it without the UI. The `Phosphor.Launcher` QML module draws the surface. It is themed through
[`phosphor-theme`](../phosphor-theme/README.md) and built on the
[`phosphor-shell-widgets`](../phosphor-shell-widgets/README.md) atoms.

`PhosphorRegistry::ILauncherProvider` (in
[`phosphor-registry`](https://github.com/phosphor-works/phosphor-libs/blob/main/libs/phosphor-registry/README.md))
is the seam. A provider is a pure data source and owns no UI:

| Member | What it does |
| --- | --- |
| `setQuery()` | Gives the text to the provider. |
| `resultsChanged()` | Tells that `results()` now gives a different answer. |
| `activate(id, Primary \| Alternate)` | Does the action of a row. |

`ILauncherProviderFactory` makes the providers. Its `createProvider()` returns an `ILauncherProvider*`, not a bare
`QObject*`.

## Key types

| Component             | Role                                                                                                   |
|-----------------------|--------------------------------------------------------------------------------------------------------|
| `FuzzyMatcher`        | A port of fzf's default `FuzzyMatchV2` scorer, with fzf's constants and bonus table. Thus two candidates rank here as they rank in fzf. `perfectScore()` lets an exact-answer provider put itself above all fuzzy hits. |
| `DesktopEntry`        | Hand-written parser for a Desktop Entry group: locale fallback, list values, `\s` escapes, `TryExec`, `Exec` quoting and field-code removal. `DesktopEntryScanner` walks the applications directories. For each id, the first directory wins. |
| `LauncherModel`       | The one model that the surface binds. It has the rows of each provider for the query, ranked in each provider. Providers are in the order of their best row, and a filter can show one provider. It owns no providers. The host calls `addProvider()` for each. |
| `AppsProvider`        | Installed applications, scanned again when an applications directory changes. For the same text, name matches rank above keyword and generic-name matches. |
| `CalculatorProvider`  | A recursive-descent evaluator (`+ - * / % ^`, parentheses, `sqrt`, `abs`), not a JS engine. A bare number is not a calculation. Enter copies the answer. |
| `CommandProvider`     | Runs the typed line through `/bin/sh -c`. It offers a row only when the first word resolves on `PATH`. Its score is the floor, so an app always ranks above it. Alt+Enter runs it in a terminal. |
| `ClipboardProvider`   | History from `phosphor-service-clipboard`. It reaches the `history` model and `copy`/`remove` by name, so this library does not link the service. Rows are addressed by timestamp, not index. |
| `WindowsProvider`     | Open windows. It is duck-typed over any model with a `toplevel` role whose objects have `title`, `appId` and `activate()`. This keeps the Wayland stack out of this library. It is inert on a compositor without foreign-toplevel. |

There is no emoji provider. The tree has no emoji dataset, and a bundled one needs a data decision (a subset, or a
full CLDR-annotated set).

## Use

The host adds the providers to a `LauncherModel` and gives the model to QML:

```cpp
#include <PhosphorShellLauncher/AppsProvider.h>
#include <PhosphorShellLauncher/CalculatorProvider.h>
#include <PhosphorShellLauncher/LauncherModel.h>

using namespace PhosphorShellLauncher;

LauncherModel model;
model.addProvider(new AppsProvider(&model));        // the order of addProvider() is the pill order
model.addProvider(new CalculatorProvider(&model));
engine.rootContext()->setContextProperty(QStringLiteral("LauncherResults"), &model);
```

The surface binds the model:

```qml
import Phosphor.Launcher

Launcher {
    anchors.centerIn: parent
    results: LauncherResults   // the host's LauncherModel
    onActivated: close()
    onDismissed: close()
}
```

## The surface

`Launcher` draws into the item that is its parent and owns no window, like the other surfaces. The shell opens it as
a popout in the center of the screen. The demo puts it in a plain window. It takes a `results` model and reads
nothing else.

All keys go to the search field, so the focus never leaves it:

| Key | Action |
| --- | --- |
| Up/Down | Move the selection. |
| Return | Run the primary action. |
| Alt+Return | Run the alternate action, if the row has one. |
| Tab and Shift+Tab | Cycle the provider filter. |
| Escape | Emit `dismissed`. |

If the provider refuses an activation, the surface stays open.

## Tests

`tests/` has nine Qt Test suites over the core:

- The matcher orderings and the exact perfect score.
- The parser and the scanner over a fixture tree.
- The gates and the ranking of the pure-logic providers (apps, calculator, command).
- The two model-backed providers against fakes of the sources that they duck-type (clipboard, windows).
- The grouping, ordering, filter and activation of the model.

A QtQuickTest harness tests the keyboard contract of the surface against a fake model. Each claim with a
mutation-shaped failure mode has a test that fails under that mutation. The mutations are: no run-bonus inheritance
in the matcher, no first-directory-wins in the scanner, no best-row ordering in the model, no bare-number gate in
the calculator, no PATH gate for commands and no secondary-field penalty for apps.

The acceptance demo is `../../examples/phosphor-launcher-demo/`. It is a plain window that hosts the surface over
the real providers. Thus it lists the applications, the clipboard and the PATH of the running session.

## Dependencies

- Qt6 >= 6.6 Core for the core library, and Gui as a private dependency for the one clipboard call of the
  calculator. Qml / Quick / `QtQuick.Shapes` (`Qt6::QuickShapes`) and KF6 Kirigami for the QML module.
- `phosphor-registry` (`ILauncherProvider`, `ILauncherProviderFactory`) as a public dependency, because the provider
  contract is in the public headers of this library.
- `phosphor-theme` (`Phosphor.Theme`) for the tokens and Motion. `phosphor-shell-widgets` (`Phosphor.Widgets`) for the
  field, pill and row chrome. In-tree builds link their QML plugins automatically.
- At runtime, `xdg-terminal-exec` (or `$TERMINAL`) for the terminal actions of the apps and command providers. If it
  is not there, those activations warn and refuse. The launcher does not fail.
