// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "extworkspaceclient_p.h"

#include "ext-workspace-v1-client-protocol.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QPointer>
#include <QThread>
#include <QtGui/qguiapplication_platform.h>

#include <wayland-client.h>

#include <cstring>

namespace {
Q_LOGGING_CATEGORY(lcExtWorkspace, "phosphorshell.workspaces.ext")
constexpr uint32_t kManagerVersion = 1;
} // namespace

namespace PhosphorShell {

/// The user data of one workspace or group proxy.
struct ExtWorkspaceClient::Handle
{
    ExtWorkspaceClient* client = nullptr;
    WorkspaceState::Key key = 0;
    ext_workspace_handle_v1* workspace = nullptr;
    ext_workspace_group_handle_v1* group = nullptr;

    ~Handle()
    {
        if (workspace) {
            ext_workspace_handle_v1_destroy(workspace);
        }
        if (group) {
            ext_workspace_group_handle_v1_destroy(group);
        }
    }
};

struct ExtWorkspaceClient::Listeners
{
    static ExtWorkspaceClient::Handle* handleOf(void* proxy)
    {
        return proxy ? static_cast<Handle*>(wl_proxy_get_user_data(static_cast<wl_proxy*>(proxy))) : nullptr;
    }

    // ── wl_registry ─────────────────────────────────────────────────────

    static void global(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t)
    {
        auto* self = static_cast<ExtWorkspaceClient*>(data);
        if (std::strcmp(interface, ext_workspace_manager_v1_interface.name) != 0 || self->m_manager) {
            return;
        }
        self->bindManager(registry, name);
    }

    static void globalRemove(void* data, wl_registry*, uint32_t name)
    {
        auto* self = static_cast<ExtWorkspaceClient*>(data);
        if (!self->m_manager || name != self->m_managerName) {
            return;
        }
        qCDebug(lcExtWorkspace) << "ext_workspace_manager_v1 removed";
        // The bound object outlives its global on the compositor's side, so
        // tell it to stop before letting go.
        self->releaseManager(true);
    }

    static constexpr wl_registry_listener registry = {
        .global = global,
        .global_remove = globalRemove,
    };

    // ── ext_workspace_manager_v1 ───────────────────────────────────────

    static void managerGroup(void* data, ext_workspace_manager_v1*, ext_workspace_group_handle_v1* proxy)
    {
        auto* self = static_cast<ExtWorkspaceClient*>(data);
        auto handle = std::make_unique<Handle>();
        handle->client = self;
        handle->group = proxy;
        handle->key = self->m_state.addGroup();
        ext_workspace_group_handle_v1_add_listener(proxy, &group, handle.get());
        self->m_handles.emplace(handle->key, std::move(handle));
    }

    static void managerWorkspace(void* data, ext_workspace_manager_v1*, ext_workspace_handle_v1* proxy)
    {
        auto* self = static_cast<ExtWorkspaceClient*>(data);
        auto handle = std::make_unique<Handle>();
        handle->client = self;
        handle->workspace = proxy;
        handle->key = self->m_state.addWorkspace();
        ext_workspace_handle_v1_add_listener(proxy, &workspace, handle.get());
        self->m_handles.emplace(handle->key, std::move(handle));
    }

    static void managerDone(void* data, ext_workspace_manager_v1*)
    {
        auto* self = static_cast<ExtWorkspaceClient*>(data);
        if (self->m_state.commit()) {
            Q_EMIT self->workspacesChanged();
        }
    }

    static void managerFinished(void* data, ext_workspace_manager_v1*)
    {
        // The compositor has destroyed the manager on its side. Whatever it
        // still advertises, this process is done with it.
        auto* self = static_cast<ExtWorkspaceClient*>(data);
        qCDebug(lcExtWorkspace) << "ext_workspace_manager_v1 finished";
        self->releaseManager(false);
    }

    static constexpr ext_workspace_manager_v1_listener manager = {
        .workspace_group = managerGroup,
        .workspace = managerWorkspace,
        .done = managerDone,
        .finished = managerFinished,
    };

    // ── ext_workspace_group_handle_v1 ──────────────────────────────────

    static void groupCapabilities(void*, ext_workspace_group_handle_v1*, uint32_t)
    {
    }

    static void groupOutput(void*, ext_workspace_group_handle_v1*, wl_output*)
    {
    }

    static void groupWorkspaceEnter(void* data, ext_workspace_group_handle_v1*, ext_workspace_handle_v1* proxy)
    {
        auto* group = static_cast<Handle*>(data);
        if (auto* workspace = handleOf(proxy)) {
            group->client->m_state.enterGroup(group->key, workspace->key);
        }
    }

    static void groupWorkspaceLeave(void* data, ext_workspace_group_handle_v1*, ext_workspace_handle_v1* proxy)
    {
        auto* group = static_cast<Handle*>(data);
        if (auto* workspace = handleOf(proxy)) {
            group->client->m_state.leaveGroup(group->key, workspace->key);
        }
    }

    static void groupRemoved(void* data, ext_workspace_group_handle_v1*)
    {
        auto* group = static_cast<Handle*>(data);
        ExtWorkspaceClient* self = group->client;
        self->m_state.removeGroup(group->key);
        // Destroys the proxy, as the protocol asks after `removed`.
        self->m_handles.erase(group->key);
    }

    static constexpr ext_workspace_group_handle_v1_listener group = {
        .capabilities = groupCapabilities,
        .output_enter = groupOutput,
        .output_leave = groupOutput,
        .workspace_enter = groupWorkspaceEnter,
        .workspace_leave = groupWorkspaceLeave,
        .removed = groupRemoved,
    };

    // ── ext_workspace_handle_v1 ────────────────────────────────────────

    static void workspaceId(void* data, ext_workspace_handle_v1*, const char* id)
    {
        auto* handle = static_cast<Handle*>(data);
        handle->client->m_state.setId(handle->key, QString::fromUtf8(id));
    }

    static void workspaceName(void* data, ext_workspace_handle_v1*, const char* name)
    {
        auto* handle = static_cast<Handle*>(data);
        handle->client->m_state.setName(handle->key, QString::fromUtf8(name));
    }

    static void workspaceCoordinates(void* data, ext_workspace_handle_v1*, wl_array* coordinates)
    {
        auto* handle = static_cast<Handle*>(data);
        QList<uint32_t> values;
        const auto* begin = static_cast<const uint32_t*>(coordinates->data);
        values.assign(begin, begin + coordinates->size / sizeof(uint32_t));
        handle->client->m_state.setCoordinates(handle->key, values);
    }

    static void workspaceState(void* data, ext_workspace_handle_v1*, uint32_t state)
    {
        auto* handle = static_cast<Handle*>(data);
        handle->client->m_state.setState(handle->key, state);
    }

    static void workspaceCapabilities(void*, ext_workspace_handle_v1*, uint32_t)
    {
    }

    static void workspaceRemoved(void* data, ext_workspace_handle_v1*)
    {
        auto* handle = static_cast<Handle*>(data);
        ExtWorkspaceClient* self = handle->client;
        self->m_state.removeWorkspace(handle->key);
        self->m_handles.erase(handle->key);
    }

    static constexpr ext_workspace_handle_v1_listener workspace = {
        .id = workspaceId,
        .name = workspaceName,
        .coordinates = workspaceCoordinates,
        .state = workspaceState,
        .capabilities = workspaceCapabilities,
        .removed = workspaceRemoved,
    };
};

ExtWorkspaceClient::ExtWorkspaceClient(wl_display* display, QObject* parent)
    : WorkspaceBackend(parent)
{
    if (!display) {
        return;
    }
    m_registry = wl_display_get_registry(display);
    wl_registry_add_listener(m_registry, &Listeners::registry, this);
}

ExtWorkspaceClient::~ExtWorkspaceClient()
{
    m_handles.clear();
    if (m_manager) {
        ext_workspace_manager_v1_stop(m_manager);
        ext_workspace_manager_v1_destroy(m_manager);
    }
    if (m_registry) {
        wl_registry_destroy(m_registry);
    }
}

ExtWorkspaceClient* ExtWorkspaceClient::shared()
{
    // The proxies are dispatched on the GUI thread with Qt's default queue,
    // and the function-static below is unguarded.
    if (!qApp || QThread::currentThread() != qApp->thread()) {
        qCCritical(lcExtWorkspace) << "ExtWorkspaceClient::shared() called off the GUI thread";
        return nullptr;
    }
    // QPointer: the client is parented to qApp, and this static outlives it.
    // A second application object in the same process (a test binary) gets
    // a fresh client instead of a dangling one.
    static QPointer<ExtWorkspaceClient> instance;
    if (instance.isNull()) {
        wl_display* display = nullptr;
        if (auto* app = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr) {
            display = app->display();
        }
        instance = new ExtWorkspaceClient(display, qApp);
    }
    return instance.data();
}

bool ExtWorkspaceClient::isSupported() const
{
    return m_supported;
}

QList<WorkspaceEntry> ExtWorkspaceClient::workspaces() const
{
    return m_state.workspaces();
}

void ExtWorkspaceClient::activate(const QString& id)
{
    if (!m_manager) {
        return;
    }
    const auto it = m_handles.find(m_state.keyFor(id));
    if (it == m_handles.end() || !it->second->workspace) {
        return;
    }
    ext_workspace_handle_v1_activate(it->second->workspace);
    ext_workspace_manager_v1_commit(m_manager);
}

void ExtWorkspaceClient::bindManager(wl_registry* registry, uint32_t name)
{
    m_manager = static_cast<ext_workspace_manager_v1*>(
        wl_registry_bind(registry, name, &ext_workspace_manager_v1_interface, kManagerVersion));
    m_managerName = name;
    ext_workspace_manager_v1_add_listener(m_manager, &Listeners::manager, this);
    setSupported(true);
}

void ExtWorkspaceClient::releaseManager(bool stop)
{
    m_handles.clear();
    if (stop) {
        ext_workspace_manager_v1_stop(m_manager);
    }
    ext_workspace_manager_v1_destroy(m_manager);
    m_manager = nullptr;
    m_managerName = 0;
    const bool emptied = m_state.clear();
    setSupported(false);
    if (emptied) {
        Q_EMIT workspacesChanged();
    }
}

void ExtWorkspaceClient::setSupported(bool supported)
{
    if (m_supported == supported) {
        return;
    }
    m_supported = supported;
    Q_EMIT supportedChanged();
}

} // namespace PhosphorShell
