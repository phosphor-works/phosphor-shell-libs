// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <PhosphorShell/ShellLoader.h>

#include <PhosphorPaths/SuiteDirectories.h>

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace PhosphorShell;

/// ShellLoader reads shell.qml from the app's folder of the suite, which
/// PhosphorPaths names and validates (its tests pin the name check that used
/// to be ShellLoader's). These tests pin where the loader looks and what it
/// falls back to.
class TestShellLoader : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void initTestCase()
    {
        // A deterministic config location, so the outcome does not depend on
        // the developer's own shell. A name no installed shell uses: test
        // mode does not redirect the system data dirs (XDG_DATA_DIRS).
        QStandardPaths::setTestModeEnabled(true);
        PhosphorPaths::setAppDirectoryName(QStringLiteral("phosphorshell-test-no-such-shell-xyz"));
    }

    void cleanup()
    {
        QDir(PhosphorPaths::appConfigDir()).removeRecursively();
    }

    void theConfigDirIsTheAppsFolder()
    {
        const ShellLoader loader;
        QCOMPARE(loader.shellConfigDir(), PhosphorPaths::appConfigDir());
        QVERIFY(
            loader.shellConfigDir().endsWith(QLatin1String("/phosphor-works/phosphorshell-test-no-such-shell-xyz")));
        // Nothing there yet.
        QCOMPARE(loader.resolve(), QUrl());
    }

    void theUsersShellIsFoundInTheAppsFolder()
    {
        const QString path = PhosphorPaths::appConfigDir() + QStringLiteral("/shell.qml");
        QVERIFY(QDir().mkpath(PhosphorPaths::appConfigDir()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QCOMPARE(ShellLoader().resolve(), QUrl::fromLocalFile(path));
    }

    /// With nothing on disk, the app's bundled shell is the answer, but only
    /// when it exists. No bundle and a missing bundle both mean no shell.
    void theBundledShellIsTheFallback()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("shell.qml"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();

        QCOMPARE(ShellLoader(QUrl::fromLocalFile(path)).resolve(), QUrl::fromLocalFile(path));
        QCOMPARE(ShellLoader(QUrl::fromLocalFile(dir.filePath(QStringLiteral("missing.qml")))).resolve(), QUrl());
        QCOMPARE(ShellLoader(QUrl(QStringLiteral("qrc:/qt/qml/No/Such/Module/shell.qml"))).resolve(), QUrl());
        QCOMPARE(ShellLoader().resolve(), QUrl());
    }
};

QTEST_MAIN(TestShellLoader)
#include "test_shellloader.moc"
