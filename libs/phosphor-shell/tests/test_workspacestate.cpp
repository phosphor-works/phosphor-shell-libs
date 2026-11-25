// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// WorkspaceState: the ext-workspace-v1 client state without the wire. The
// setters stand in for the protocol's events, commit() for its `done`.

#include "workspacestate_p.h"

#include <QtTest/QtTest>

using PhosphorShell::WorkspaceEntry;
using PhosphorShell::WorkspaceState;

class TestWorkspaceState : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void nothingIsPublishedBeforeDone()
    {
        WorkspaceState state;
        const auto key = state.addWorkspace();
        state.setId(key, QStringLiteral("one"));
        state.setName(key, QStringLiteral("One"));
        state.setState(key, WorkspaceState::StateActive);
        QVERIFY(state.workspaces().isEmpty());
        QCOMPARE(state.keyFor(QStringLiteral("one")), WorkspaceState::Key(0));

        QVERIFY(state.commit());
        QCOMPARE(state.workspaces().size(), 1);
        const WorkspaceEntry entry = state.workspaces().first();
        QCOMPARE(entry.id, QStringLiteral("one"));
        QCOMPARE(entry.name, QStringLiteral("One"));
        QVERIFY(entry.active);
        QVERIFY(!entry.urgent);
        QCOMPARE(state.keyFor(QStringLiteral("one")), key);

        // A done with nothing new reports no change.
        QVERIFY(!state.commit());
    }

    void aSwitchIsOneAtomicUpdate()
    {
        WorkspaceState state;
        const auto a = state.addWorkspace();
        const auto b = state.addWorkspace();
        state.setId(a, QStringLiteral("a"));
        state.setId(b, QStringLiteral("b"));
        state.setState(a, WorkspaceState::StateActive);
        QVERIFY(state.commit());

        // Deactivating one and activating the other arrive as two events.
        // Between them nothing is published, so no reader sees zero (or
        // two) active workspaces.
        state.setState(a, 0);
        QVERIFY(state.workspaces().at(0).active);
        state.setState(b, WorkspaceState::StateActive | WorkspaceState::StateUrgent);
        QVERIFY(state.commit());
        QVERIFY(!state.workspaces().at(0).active);
        QVERIFY(state.workspaces().at(1).active);
        QVERIFY(state.workspaces().at(1).urgent);
    }

    void withoutAProtocolIdTheKeyStandsIn()
    {
        WorkspaceState state;
        const auto first = state.addWorkspace();
        const auto second = state.addWorkspace();
        state.setName(first, QStringLiteral("Same"));
        state.setName(second, QStringLiteral("Same"));
        QVERIFY(state.commit());
        const auto entries = state.workspaces();
        QCOMPARE(entries.size(), 2);
        QVERIFY(!entries.at(0).id.isEmpty());
        QVERIFY(entries.at(0).id != entries.at(1).id);
        QCOMPARE(state.keyFor(entries.at(0).id), first);
        QCOMPARE(state.keyFor(entries.at(1).id), second);
    }

    void hiddenWorkspacesAreLeftOut()
    {
        WorkspaceState state;
        const auto shown = state.addWorkspace();
        const auto hidden = state.addWorkspace();
        state.setId(shown, QStringLiteral("shown"));
        state.setId(hidden, QStringLiteral("hidden"));
        state.setState(hidden, WorkspaceState::StateHidden | WorkspaceState::StateActive);
        QVERIFY(state.commit());
        QCOMPARE(state.workspaces().size(), 1);
        QCOMPARE(state.workspaces().first().id, QStringLiteral("shown"));
        QCOMPARE(state.keyFor(QStringLiteral("hidden")), WorkspaceState::Key(0));

        state.setState(hidden, 0);
        QVERIFY(state.commit());
        QCOMPARE(state.workspaces().size(), 2);
    }

    void orderedByGroupThenCoordinatesThenAppearance()
    {
        WorkspaceState state;
        const auto left = state.addGroup();
        const auto right = state.addGroup();
        const auto loose = state.addWorkspace(); // in no group
        const auto r1 = state.addWorkspace();
        const auto l2 = state.addWorkspace();
        const auto l1 = state.addWorkspace();
        const auto lPlain = state.addWorkspace(); // in a group, no coordinates
        state.setId(loose, QStringLiteral("loose"));
        state.setId(r1, QStringLiteral("r1"));
        state.setId(l2, QStringLiteral("l2"));
        state.setId(l1, QStringLiteral("l1"));
        state.setId(lPlain, QStringLiteral("lPlain"));
        state.setCoordinates(r1, {0});
        state.setCoordinates(l2, {1, 0});
        state.setCoordinates(l1, {0, 3});
        state.enterGroup(right, r1);
        state.enterGroup(left, l2);
        state.enterGroup(left, l1);
        state.enterGroup(left, lPlain);
        QVERIFY(state.commit());

        QStringList ids;
        for (const auto& entry : state.workspaces()) {
            ids.append(entry.id);
        }
        QCOMPARE(ids,
                 (QStringList{QStringLiteral("l1"), QStringLiteral("l2"), QStringLiteral("lPlain"),
                              QStringLiteral("r1"), QStringLiteral("loose")}));

        // Leaving a group, or the group going away, sends the workspace to
        // the end.
        state.leaveGroup(left, l1);
        state.removeGroup(right);
        QVERIFY(state.commit());
        ids.clear();
        for (const auto& entry : state.workspaces()) {
            ids.append(entry.id);
        }
        QCOMPARE(ids,
                 (QStringList{QStringLiteral("l2"), QStringLiteral("lPlain"), QStringLiteral("r1"),
                              QStringLiteral("l1"), QStringLiteral("loose")}));
    }

    void removalIsPublishedAtTheNextDone()
    {
        WorkspaceState state;
        const auto a = state.addWorkspace();
        const auto b = state.addWorkspace();
        state.setId(a, QStringLiteral("a"));
        state.setId(b, QStringLiteral("b"));
        QVERIFY(state.commit());

        state.removeWorkspace(a);
        // Still listed, but there is nothing left to activate.
        QCOMPARE(state.workspaces().size(), 2);
        QCOMPARE(state.keyFor(QStringLiteral("a")), WorkspaceState::Key(0));
        QVERIFY(state.commit());
        QCOMPARE(state.workspaces().size(), 1);
        QCOMPARE(state.workspaces().first().id, QStringLiteral("b"));

        // Events for a removed workspace change nothing.
        state.setName(a, QStringLiteral("ghost"));
        QVERIFY(!state.commit());
    }

    void keysAreNeverReused()
    {
        WorkspaceState state;
        const auto a = state.addWorkspace();
        state.removeWorkspace(a);
        const auto b = state.addWorkspace();
        QVERIFY(a != b);
        QVERIFY(state.addGroup() != b);
    }

    void clearDropsThePublishedList()
    {
        WorkspaceState state;
        QVERIFY(!state.clear());
        const auto a = state.addWorkspace();
        state.setId(a, QStringLiteral("a"));
        QVERIFY(state.commit());
        QVERIFY(state.clear());
        QVERIFY(state.workspaces().isEmpty());
        QCOMPARE(state.keyFor(QStringLiteral("a")), WorkspaceState::Key(0));
        QVERIFY(!state.commit());
    }
};

QTEST_GUILESS_MAIN(TestWorkspaceState)
#include "test_workspacestate.moc"
