// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// The Workspaces singleton and its list model over a backend this test
// drives: the QML-facing contract (roles, change-gated signals, a switch as
// a dataChanged rather than a reset, `supported` following the source)
// without a compositor. test_extworkspaceclient covers the protocol side.

#include <PhosphorShell/Workspaces.h>

#include "workspacebackend_p.h"

#include <QAbstractListModel>
#include <QSignalSpy>
#include <QtTest/QtTest>

using PhosphorShell::WorkspaceBackend;
using PhosphorShell::WorkspaceEntry;
using PhosphorShell::WorkspaceListModel;
using PhosphorShell::Workspaces;

namespace {
class FakeBackend : public WorkspaceBackend
{
public:
    bool isSupported() const override
    {
        return supported;
    }
    QList<WorkspaceEntry> workspaces() const override
    {
        return entries;
    }
    void activate(const QString& id) override
    {
        activated.append(id);
    }

    void setSupported(bool value)
    {
        supported = value;
        Q_EMIT supportedChanged();
    }
    void publish(const QList<WorkspaceEntry>& next)
    {
        entries = next;
        Q_EMIT workspacesChanged();
    }

    bool supported = false;
    QList<WorkspaceEntry> entries;
    QStringList activated;
};

WorkspaceEntry entry(const char* id, const char* name, bool active = false)
{
    return WorkspaceEntry{QString::fromLatin1(id), QString::fromLatin1(name), active, false};
}
} // namespace

class TestWorkspaces : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    /// The role names are the delegate's entire contract: a rename would
    /// break every `required property` silently, since QML reports a
    /// missing role as undefined rather than an error.
    void roleNamesArePinned()
    {
        FakeBackend backend;
        Workspaces workspaces(&backend);
        const QHash<int, QByteArray> roles = workspaces.model()->roleNames();
        QCOMPARE(roles.value(WorkspaceListModel::IdRole), QByteArray("workspaceId"));
        QCOMPARE(roles.value(WorkspaceListModel::NameRole), QByteArray("name"));
        QCOMPARE(roles.value(WorkspaceListModel::IsActiveRole), QByteArray("isActive"));
        QCOMPARE(roles.size(), 3);
    }

    /// QML binds `Repeater { model: Workspaces.model }` unconditionally, so
    /// the model exists even with nothing behind it.
    void withoutABackendEverythingIsEmpty()
    {
        Workspaces workspaces(static_cast<WorkspaceBackend*>(nullptr));
        QVERIFY(workspaces.model());
        QVERIFY(!workspaces.isSupported());
        QCOMPARE(workspaces.count(), 0);
        QCOMPARE(workspaces.activeIndex(), 0);
        QVERIFY(workspaces.activeId().isEmpty());
        QCOMPARE(workspaces.model()->rowCount(), 0);
        workspaces.switchTo(QStringLiteral("anything"));
    }

    /// `supported` follows the source as its global comes and goes.
    void supportedFollowsTheBackend()
    {
        FakeBackend backend;
        Workspaces workspaces(&backend);
        QSignalSpy supported(&workspaces, &Workspaces::supportedChanged);
        QVERIFY(!workspaces.isSupported());
        backend.setSupported(true);
        QCOMPARE(supported.count(), 1);
        QVERIFY(workspaces.isSupported());
        backend.setSupported(false);
        QCOMPARE(supported.count(), 2);
        QVERIFY(!workspaces.isSupported());
    }

    void modelAndActiveFollowTheList()
    {
        FakeBackend backend;
        Workspaces workspaces(&backend);
        QSignalSpy active(&workspaces, &Workspaces::activeChanged);
        QSignalSpy count(&workspaces, &Workspaces::countChanged);
        auto* model = workspaces.model();

        backend.publish({entry("a", "Mail"), entry("b", "Code", true), entry("c", "")});
        QCOMPARE(count.count(), 1);
        QCOMPARE(active.count(), 1);
        QCOMPARE(workspaces.count(), 3);
        QCOMPARE(workspaces.activeId(), QStringLiteral("b"));
        QCOMPARE(workspaces.activeName(), QStringLiteral("Code"));
        QCOMPARE(workspaces.activeIndex(), 2);
        QCOMPARE(model->rowCount(), 3);
        QCOMPARE(model->data(model->index(0), WorkspaceListModel::IdRole).toString(), QStringLiteral("a"));
        QCOMPARE(model->data(model->index(1), WorkspaceListModel::IsActiveRole).toBool(), true);
        // An unnamed workspace still gets a label, numbered by position.
        QCOMPARE(model->data(model->index(2), WorkspaceListModel::NameRole).toString(), QStringLiteral("Workspace 3"));

        // The same list again: nothing to announce.
        backend.publish(backend.entries);
        QCOMPARE(count.count(), 1);
        QCOMPARE(active.count(), 1);

        // A workspace added before the active one keeps its id and moves
        // its position, which is still an activeChanged.
        backend.publish({entry("z", "New"), entry("a", "Mail"), entry("b", "Code", true), entry("c", "")});
        QCOMPARE(workspaces.activeId(), QStringLiteral("b"));
        QCOMPARE(workspaces.activeIndex(), 3);
        QCOMPARE(active.count(), 2);
        QCOMPARE(count.count(), 2);
    }

    /// A switch re-styles two rows. A reset would rebuild every pager
    /// delegate and restart its transition.
    void aSwitchIsTwoDataChangesNotAReset()
    {
        FakeBackend backend;
        backend.entries = {entry("a", "One", true), entry("b", "Two"), entry("c", "Three")};
        Workspaces workspaces(&backend);
        auto* model = workspaces.model();
        QSignalSpy reset(model, &QAbstractItemModel::modelReset);
        QSignalSpy changed(model, &QAbstractItemModel::dataChanged);

        backend.publish({entry("a", "One"), entry("b", "Two"), entry("c", "Three", true)});
        QCOMPARE(reset.count(), 0);
        QCOMPARE(changed.count(), 2);
        QCOMPARE(changed.at(0).at(0).value<QModelIndex>().row(), 0);
        QCOMPARE(changed.at(1).at(0).value<QModelIndex>().row(), 2);
        QCOMPARE(changed.at(1).at(2).value<QList<int>>(), QList<int>{WorkspaceListModel::IsActiveRole});
        QCOMPARE(workspaces.activeId(), QStringLiteral("c"));

        // A rename is a different list.
        backend.publish({entry("a", "Uno"), entry("b", "Two"), entry("c", "Three", true)});
        QCOMPARE(reset.count(), 1);

        // No workspace active at all.
        backend.publish({entry("a", "Uno"), entry("b", "Two"), entry("c", "Three")});
        QCOMPARE(workspaces.activeIndex(), 0);
        QVERIFY(workspaces.activeId().isEmpty());
    }

    void switchToForwardsKnownShapes()
    {
        FakeBackend backend;
        Workspaces workspaces(&backend);
        workspaces.switchTo(QString());
        workspaces.switchTo(QStringLiteral("b"));
        QCOMPARE(backend.activated, QStringList{QStringLiteral("b")});
    }

    /// The global vanishing empties everything in one update.
    void losingTheSourceEmptiesTheModel()
    {
        FakeBackend backend;
        backend.supported = true;
        backend.entries = {entry("a", "One", true)};
        Workspaces workspaces(&backend);
        QSignalSpy count(&workspaces, &Workspaces::countChanged);
        QSignalSpy active(&workspaces, &Workspaces::activeChanged);
        backend.publish({});
        backend.setSupported(false);
        QCOMPARE(count.count(), 1);
        QCOMPARE(active.count(), 1);
        QCOMPARE(workspaces.model()->rowCount(), 0);
        QVERIFY(!workspaces.isSupported());
    }
};

QTEST_GUILESS_MAIN(TestWorkspaces)
#include "test_workspaces.moc"
