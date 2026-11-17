// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <PhosphorShell/phosphorshell_export.h>

#include <QString>
#include <QUrl>

namespace PhosphorShell {

/// Finds the shell.qml an app runs. `shellName` is the app's directory name:
/// the tree is looked up in `<config>/<shellName>/` and `<data>/<shellName>/`
/// for every XDG config and data location, then in the qrc module
/// `Phosphor.Shell.Example` linked into the binary.
class PHOSPHORSHELL_EXPORT ShellLoader
{
public:
    explicit ShellLoader(const QString& shellName);

    [[nodiscard]] QUrl resolve() const;
    [[nodiscard]] QString shellConfigDir() const;

private:
    QString m_shellName;
};

} // namespace PhosphorShell
