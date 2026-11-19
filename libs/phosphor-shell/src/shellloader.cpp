// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <PhosphorShell/ShellLoader.h>

#include <PhosphorPaths/SuiteDirectories.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLatin1String>
#include <QLoggingCategory>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcShellLoader, "phosphorshell.shellloader")

namespace PhosphorShell {

ShellLoader::ShellLoader(const QUrl& bundledShell)
    : m_bundledShell(bundledShell)
{
}

QUrl ShellLoader::resolve() const
{
    // PhosphorPaths validated the name when the app set it, so it is one
    // directory name and the folder cannot leave the XDG roots.
    const QString appSubdir = PhosphorPaths::appSubdir();
    const QStringList configPaths = QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation);
    // Also probe DataLocation so a bundled example shipped to
    // ${DATADIR}/phosphor-works/<app>/ can be the fallback when the user hasn't
    // installed a custom shell.qml in their config dir yet.
    const QStringList dataPaths = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    QStringList searched = configPaths;
    searched.append(dataPaths);

    for (const QString& basePath : std::as_const(searched)) {
        // QDir::filePath joins normally and collapses any trailing slash on
        // basePath, avoiding `/foo//<app>/shell.qml`. The filesystem
        // tolerates the doubled slash but it leaks into log output and
        // breaks symbolic comparisons of the resolved path.
        const QString shellDir = QDir(basePath).filePath(appSubdir);
        const QString candidate = QDir(shellDir).filePath(QStringLiteral("shell.qml"));
        if (QFileInfo::exists(candidate)) {
            return QUrl::fromLocalFile(candidate);
        }
    }

    // The app's own fallback, typically a module linked into the binary, so
    // a fresh system without a shell tree on disk still runs. Only when it
    // is there: a missing bundle is the same "no shell" as no bundle.
    if (m_bundledShell.isEmpty()) {
        return {};
    }
    const bool qrc = m_bundledShell.scheme() == QLatin1String("qrc");
    if (qrc || m_bundledShell.isLocalFile()) {
        const QString path = qrc ? QLatin1Char(':') + m_bundledShell.path() : m_bundledShell.toLocalFile();
        if (!QFile::exists(path)) {
            qCWarning(lcShellLoader) << "The bundled shell does not exist:" << m_bundledShell;
            return {};
        }
    }
    return m_bundledShell;
}

QString ShellLoader::shellConfigDir() const
{
    return PhosphorPaths::appConfigDir();
}

} // namespace PhosphorShell
