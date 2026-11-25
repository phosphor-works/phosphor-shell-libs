// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <PhosphorShell/phosphorshell_export.h>

#include <QString>
#include <QUrl>

namespace PhosphorShell {

/// Finds the shell.qml an app runs, in the app's folder of the suite
/// (PhosphorPaths::appSubdir(), `phosphor-works/<app>`) under every XDG config
/// location, then every XDG data location. The app names that folder with
/// PhosphorPaths::setAppDirectoryName() before it builds a loader. When none
/// has a shell.qml, `bundledShell` is the fallback, such as the shell.qml of a
/// QML module linked into the binary (`qrc:/qt/qml/<Module>/<Path>/shell.qml`),
/// used when it exists. Empty: no fallback.
class PHOSPHORSHELL_EXPORT ShellLoader
{
public:
    explicit ShellLoader(const QUrl& bundledShell = QUrl());

    [[nodiscard]] QUrl resolve() const;
    /// Where the user's own shell tree goes: PhosphorPaths::appConfigDir().
    [[nodiscard]] QString shellConfigDir() const;

private:
    QUrl m_bundledShell;
};

} // namespace PhosphorShell
