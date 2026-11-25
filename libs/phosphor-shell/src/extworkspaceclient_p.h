// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "workspacebackend_p.h"
#include "workspacestate_p.h"

#include <PhosphorShell/phosphorshell_export.h>

#include <cstdint>
#include <memory>
#include <unordered_map>

struct wl_display;
struct wl_registry;
struct ext_workspace_manager_v1;

namespace PhosphorShell {

/// The compositor's workspaces over ext-workspace-v1.
///
/// Binds ext_workspace_manager_v1 from its own wl_registry on `display`
/// and follows the global: `supported` turns true when the compositor
/// announces it and false when it removes it (or finishes the manager),
/// and comes back if the global does. The proxies live on the display's
/// default queue, so whoever dispatches that queue (Qt, for the display
/// shared() uses) delivers the events, on that thread.
class PHOSPHORSHELL_EXPORT ExtWorkspaceClient : public WorkspaceBackend
{
    Q_OBJECT

public:
    /// Null `display`: inert and unsupported, for a session that is not
    /// Wayland.
    explicit ExtWorkspaceClient(wl_display* display, QObject* parent = nullptr);
    ~ExtWorkspaceClient() override;

    /// The process-wide client on Qt's own Wayland connection, parented to
    /// qApp and created on first use. GUI thread only; null off it.
    [[nodiscard]] static ExtWorkspaceClient* shared();

    [[nodiscard]] bool isSupported() const override;
    [[nodiscard]] QList<WorkspaceEntry> workspaces() const override;
    void activate(const QString& id) override;

private:
    struct Handle;
    struct Listeners;
    friend struct Listeners;

    void bindManager(wl_registry* registry, uint32_t name);
    /// Drop the manager and everything it announced. `stop`: send the
    /// stop request first, for a manager the compositor has not finished.
    void releaseManager(bool stop);
    void setSupported(bool supported);

    wl_registry* m_registry = nullptr;
    ext_workspace_manager_v1* m_manager = nullptr;
    uint32_t m_managerName = 0;
    bool m_supported = false;
    WorkspaceState m_state;
    // Workspace and group handles by their state key. A Handle is each
    // proxy's user data, so it lives exactly as long as the proxy.
    std::unordered_map<WorkspaceState::Key, std::unique_ptr<Handle>> m_handles;
};

} // namespace PhosphorShell
