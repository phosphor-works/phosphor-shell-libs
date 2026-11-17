// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// ExtWorkspaceClient against a compositor in this process: a libwayland
// server display serving ext_workspace_manager_v1 from this file, connected
// to the client over a socket pair. Both ends run on the test thread, and
// pump() moves the messages across, so every protocol exchange here is the
// real wire format without a compositor installed.

#include "extworkspaceclient_p.h"

#include "ext-workspace-v1-server-protocol.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

#include <wayland-client.h>
#include <wayland-server.h>

#include <poll.h>
#include <sys/socket.h>

using PhosphorShell::ExtWorkspaceClient;
using PhosphorShell::WorkspaceEntry;

namespace {

class FakeCompositor
{
public:
    FakeCompositor()
    {
        display = wl_display_create();
        int fds[2];
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds) != 0) {
            qFatal("socketpair failed");
        }
        client = wl_client_create(display, fds[0]);
        clientDisplay = wl_display_connect_to_fd(fds[1]);
    }

    ~FakeCompositor()
    {
        wl_display_disconnect(clientDisplay);
        wl_display_destroy(display);
    }

    /// Move requests to the server and events back until both are quiet.
    void pump()
    {
        for (int round = 0; round < 8; ++round) {
            wl_display_flush(clientDisplay);
            wl_event_loop_dispatch(wl_display_get_event_loop(display), 0);
            wl_display_flush_clients(display);
            while (wl_display_prepare_read(clientDisplay) != 0) {
                wl_display_dispatch_pending(clientDisplay);
            }
            pollfd fd{wl_display_get_fd(clientDisplay), POLLIN, 0};
            if (poll(&fd, 1, 0) > 0) {
                wl_display_read_events(clientDisplay);
            } else {
                wl_display_cancel_read(clientDisplay);
            }
            wl_display_dispatch_pending(clientDisplay);
        }
    }

    void advertise()
    {
        global = wl_global_create(display, &ext_workspace_manager_v1_interface, 1, this, bind);
    }

    void withdraw()
    {
        wl_global_destroy(global);
        global = nullptr;
    }

    wl_resource* addGroup()
    {
        auto* group = wl_resource_create(client, &ext_workspace_group_handle_v1_interface, 1, 0);
        wl_resource_set_implementation(group, &groupImpl, this, nullptr);
        ext_workspace_manager_v1_send_workspace_group(manager, group);
        ext_workspace_group_handle_v1_send_capabilities(group, 0);
        return group;
    }

    wl_resource* addWorkspace(const char* id, const char* name, std::initializer_list<uint32_t> coordinates,
                              uint32_t state)
    {
        auto* workspace = wl_resource_create(client, &ext_workspace_handle_v1_interface, 1, 0);
        wl_resource_set_implementation(workspace, &workspaceImpl, this, workspaceDestroyed);
        ext_workspace_manager_v1_send_workspace(manager, workspace);
        if (id) {
            ext_workspace_handle_v1_send_id(workspace, id);
        }
        ext_workspace_handle_v1_send_name(workspace, name);
        wl_array array;
        wl_array_init(&array);
        for (const uint32_t value : coordinates) {
            *static_cast<uint32_t*>(wl_array_add(&array, sizeof(uint32_t))) = value;
        }
        ext_workspace_handle_v1_send_coordinates(workspace, &array);
        wl_array_release(&array);
        ext_workspace_handle_v1_send_state(workspace, state);
        ext_workspace_handle_v1_send_capabilities(workspace, EXT_WORKSPACE_HANDLE_V1_WORKSPACE_CAPABILITIES_ACTIVATE);
        return workspace;
    }

    void done()
    {
        ext_workspace_manager_v1_send_done(manager);
    }

    wl_display* display = nullptr;
    wl_client* client = nullptr;
    wl_display* clientDisplay = nullptr;
    wl_global* global = nullptr;
    wl_resource* manager = nullptr;
    QList<wl_resource*> activated;
    int commits = 0;
    int stops = 0;
    int workspacesDestroyed = 0;

private:
    static FakeCompositor* self(wl_resource* resource)
    {
        return static_cast<FakeCompositor*>(wl_resource_get_user_data(resource));
    }

    static void bind(wl_client* client, void* data, uint32_t version, uint32_t id)
    {
        auto* compositor = static_cast<FakeCompositor*>(data);
        compositor->manager = wl_resource_create(client, &ext_workspace_manager_v1_interface, version, id);
        wl_resource_set_implementation(compositor->manager, &managerImpl, compositor, managerDestroyed);
    }

    static void managerDestroyed(wl_resource* resource)
    {
        if (auto* compositor = self(resource); compositor->manager == resource) {
            compositor->manager = nullptr;
        }
    }

    static void workspaceDestroyed(wl_resource* resource)
    {
        ++self(resource)->workspacesDestroyed;
    }

    static void destroy(wl_client*, wl_resource* resource)
    {
        wl_resource_destroy(resource);
    }

    static void commit(wl_client*, wl_resource* resource)
    {
        ++self(resource)->commits;
    }

    static void stop(wl_client*, wl_resource* resource)
    {
        ++self(resource)->stops;
        ext_workspace_manager_v1_send_finished(resource);
        wl_resource_destroy(resource);
    }

    static void createWorkspace(wl_client*, wl_resource*, const char*)
    {
    }

    static void activate(wl_client*, wl_resource* resource)
    {
        self(resource)->activated.append(resource);
    }

    static void ignore(wl_client*, wl_resource*)
    {
    }

    static void assign(wl_client*, wl_resource*, wl_resource*)
    {
    }

    static constexpr struct ext_workspace_manager_v1_interface managerImpl = {
        .commit = commit,
        .stop = stop,
    };
    static constexpr struct ext_workspace_group_handle_v1_interface groupImpl = {
        .create_workspace = createWorkspace,
        .destroy = destroy,
    };
    static constexpr struct ext_workspace_handle_v1_interface workspaceImpl = {
        .destroy = destroy,
        .activate = activate,
        .deactivate = ignore,
        .assign = assign,
        .remove = ignore,
    };
};

QStringList namesOf(const QList<WorkspaceEntry>& entries)
{
    QStringList names;
    for (const auto& entry : entries) {
        names.append(entry.name);
    }
    return names;
}

} // namespace

class TestExtWorkspaceClient : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void withoutADisplayItIsInert()
    {
        ExtWorkspaceClient client(nullptr);
        QVERIFY(!client.isSupported());
        QVERIFY(client.workspaces().isEmpty());
        client.activate(QStringLiteral("anything"));
    }

    /// `supported` follows the global: absent, announced, removed, and
    /// announced again.
    void supportedFollowsTheGlobal()
    {
        FakeCompositor compositor;
        ExtWorkspaceClient client(compositor.clientDisplay);
        QSignalSpy supported(&client, &ExtWorkspaceClient::supportedChanged);
        compositor.pump();
        QVERIFY(!client.isSupported());

        compositor.advertise();
        compositor.pump();
        QVERIFY(client.isSupported());
        QVERIFY(compositor.manager);
        QCOMPARE(supported.count(), 1);

        compositor.withdraw();
        compositor.pump();
        QVERIFY(!client.isSupported());
        QCOMPARE(supported.count(), 2);
        // Let go of a manager the compositor has not finished.
        QCOMPARE(compositor.stops, 1);

        compositor.advertise();
        compositor.pump();
        QVERIFY(client.isSupported());
        QCOMPARE(supported.count(), 3);
    }

    /// Nothing shows before `done`, then the whole batch at once, ordered
    /// by coordinates rather than by announcement.
    void theListChangesOnlyAtDone()
    {
        FakeCompositor compositor;
        ExtWorkspaceClient client(compositor.clientDisplay);
        QSignalSpy changed(&client, &ExtWorkspaceClient::workspacesChanged);
        compositor.advertise();
        compositor.pump();

        auto* group = compositor.addGroup();
        auto* second = compositor.addWorkspace("second", "Two", {1}, 0);
        auto* first = compositor.addWorkspace(nullptr, "One", {0}, EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE);
        compositor.addWorkspace("hidden", "Hidden", {2}, EXT_WORKSPACE_HANDLE_V1_STATE_HIDDEN);
        ext_workspace_group_handle_v1_send_workspace_enter(group, second);
        ext_workspace_group_handle_v1_send_workspace_enter(group, first);
        compositor.pump();
        QVERIFY(client.workspaces().isEmpty());
        QCOMPARE(changed.count(), 0);

        compositor.done();
        compositor.pump();
        QCOMPARE(changed.count(), 1);
        const auto entries = client.workspaces();
        QCOMPARE(namesOf(entries), (QStringList{QStringLiteral("One"), QStringLiteral("Two")}));
        QVERIFY(entries.at(0).active);
        QVERIFY(!entries.at(1).active);
        QCOMPARE(entries.at(1).id, QStringLiteral("second"));
        // No id from the compositor, but still one to address it by.
        QVERIFY(!entries.at(0).id.isEmpty());

        // A switch is two state events and one done: one update.
        ext_workspace_handle_v1_send_state(first, 0);
        ext_workspace_handle_v1_send_state(second, EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE);
        compositor.done();
        compositor.pump();
        QCOMPARE(changed.count(), 2);
        QVERIFY(!client.workspaces().at(0).active);
        QVERIFY(client.workspaces().at(1).active);
    }

    void activateSendsActivateThenCommit()
    {
        FakeCompositor compositor;
        ExtWorkspaceClient client(compositor.clientDisplay);
        compositor.advertise();
        compositor.pump();
        auto* workspace = compositor.addWorkspace("target", "Target", {}, 0);
        compositor.done();
        compositor.pump();

        client.activate(QStringLiteral("unknown"));
        compositor.pump();
        QVERIFY(compositor.activated.isEmpty());
        QCOMPARE(compositor.commits, 0);

        client.activate(QStringLiteral("target"));
        compositor.pump();
        QCOMPARE(compositor.activated, QList<wl_resource*>{workspace});
        QCOMPARE(compositor.commits, 1);
    }

    void aRemovedWorkspaceIsDestroyedAndDropped()
    {
        FakeCompositor compositor;
        ExtWorkspaceClient client(compositor.clientDisplay);
        compositor.advertise();
        compositor.pump();
        auto* gone = compositor.addWorkspace("gone", "Gone", {0}, 0);
        compositor.addWorkspace("kept", "Kept", {1}, 0);
        compositor.done();
        compositor.pump();
        QCOMPARE(client.workspaces().size(), 2);

        ext_workspace_handle_v1_send_removed(gone);
        compositor.done();
        compositor.pump();
        QCOMPARE(namesOf(client.workspaces()), QStringList{QStringLiteral("Kept")});
        // The client answered `removed` with destroy.
        QCOMPARE(compositor.workspacesDestroyed, 1);
    }

    /// The global going away takes the list with it, in one update.
    void losingTheGlobalEmptiesTheList()
    {
        FakeCompositor compositor;
        ExtWorkspaceClient client(compositor.clientDisplay);
        compositor.advertise();
        compositor.pump();
        compositor.addWorkspace("a", "A", {}, EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE);
        compositor.done();
        compositor.pump();
        QSignalSpy changed(&client, &ExtWorkspaceClient::workspacesChanged);

        compositor.withdraw();
        compositor.pump();
        QCOMPARE(changed.count(), 1);
        QVERIFY(client.workspaces().isEmpty());
        QVERIFY(!client.isSupported());
        QCOMPARE(compositor.workspacesDestroyed, 1);
    }

    void finishedEndsSupport()
    {
        FakeCompositor compositor;
        ExtWorkspaceClient client(compositor.clientDisplay);
        compositor.advertise();
        compositor.pump();
        QVERIFY(client.isSupported());

        ext_workspace_manager_v1_send_finished(compositor.manager);
        wl_resource_destroy(compositor.manager);
        compositor.pump();
        QVERIFY(!client.isSupported());
        // Finished needs no stop.
        QCOMPARE(compositor.stops, 0);
    }
};

QTEST_GUILESS_MAIN(TestExtWorkspaceClient)
#include "test_extworkspaceclient.moc"
