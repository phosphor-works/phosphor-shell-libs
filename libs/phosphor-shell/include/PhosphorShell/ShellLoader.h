// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <PhosphorShell/phosphorshell_export.h>

#include <QString>
#include <QUrl>

namespace PhosphorShell {

/// Finds the shell.qml an app runs. `shellName` is the app's directory name:
/// the tree is looked up in `<config>/<shellName>/` and `<data>/<shellName>/`
/// for every XDG config and data location. When none has one, `bundledShell`
/// is the fallback, such as the shell.qml of a QML module linked into the
/// binary (`qrc:/qt/qml/<Module>/<Path>/shell.qml`), used when it exists.
/// Empty: no fallback.
class PHOSPHORSHELL_EXPORT ShellLoader
{
public:
    explicit ShellLoader(const QString& shellName, const QUrl& bundledShell = QUrl());

    [[nodiscard]] QUrl resolve() const;
    [[nodiscard]] QString shellConfigDir() const;

private:
    QString m_shellName;
    QUrl m_bundledShell;
};

} // namespace PhosphorShell
