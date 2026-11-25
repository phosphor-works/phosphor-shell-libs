// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// FontFaces resolves each role to an installed family: the first candidate
// the font database knows, else the caller's fallback, so Tokens never
// binds a family that does not exist.

#include <PhosphorTheme/FontFaces.h>

#include <QFontDatabase>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTest>

using PhosphorTheme::FontFaces;

class TestFontFaces : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void resolvePrefersTheFirstInstalledCandidate()
    {
        // A family the database confirms with hasFamily(), which is what
        // resolve() asks. families() can also list a family hasFamily()
        // rejects: on a system with few fonts it names the "Sans Serif"
        // alias, so the first listed entries are not proof of anything.
        QString installed;
        const QStringList families = QFontDatabase::families();
        for (const QString& family : families) {
            if (QFontDatabase::hasFamily(family)) {
                installed = family;
                break;
            }
        }
        if (installed.isEmpty()) {
            QSKIP("no installed font family");
        }
        // A name no database carries first, then a real family: the real
        // one wins, and the fallback is untouched.
        const QStringList candidates{QStringLiteral("Phosphor Nonexistent Face 9f3a"), installed};
        QCOMPARE(FontFaces::resolve(candidates, QStringLiteral("fallback")), installed);
    }

    void resolveFallsBackWhenNothingIsInstalled()
    {
        const QStringList candidates{QStringLiteral("Phosphor Nonexistent Face 9f3a"),
                                     QStringLiteral("Phosphor Nonexistent Face 7c21")};
        QCOMPARE(FontFaces::resolve(candidates, QStringLiteral("fallback")), QStringLiteral("fallback"));
    }

    void facesAreNeverEmpty()
    {
        FontFaces faces;
        QVERIFY(!faces.ui().isEmpty());
        QVERIFY(!faces.mono().isEmpty());
        // The preferred flags agree with the database.
        QCOMPARE(faces.uiPreferred(), QFontDatabase::hasFamily(FontFaces::uiCandidates().first()));
        QCOMPARE(faces.monoPreferred(), QFontDatabase::hasFamily(FontFaces::monoCandidates().first()));
    }

    void refreshOnlySignalsOnChange()
    {
        FontFaces faces;
        QSignalSpy spy(&faces, &FontFaces::facesChanged);
        faces.refresh();
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestFontFaces)

#include "test_fontfaces.moc"
