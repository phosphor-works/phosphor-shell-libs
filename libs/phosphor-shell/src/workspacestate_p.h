// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "workspacebackend_p.h"

#include <PhosphorShell/phosphorshell_export.h>

#include <QHash>
#include <QList>
#include <QString>

#include <cstdint>

namespace PhosphorShell {

/// The ext-workspace-v1 client state, without the wire: the setters mirror
/// the protocol's events, and commit() is its `done`. What workspaces()
/// returns changes only at commit(), so a batch of events is seen as one
/// atomic update, as the protocol asks.
///
/// Workspaces and groups are named by keys this class hands out, never by
/// proxy pointers: a handle destroyed and a new one created in the same
/// batch can share an address, a key is never reused.
class PHOSPHORSHELL_EXPORT WorkspaceState
{
public:
    using Key = quint64;

    /// ext_workspace_handle_v1.state bits.
    static constexpr uint32_t StateActive = 1;
    static constexpr uint32_t StateUrgent = 2;
    static constexpr uint32_t StateHidden = 4;

    /// A new workspace (manager.workspace) or group (manager.workspace_group).
    [[nodiscard]] Key addWorkspace();
    [[nodiscard]] Key addGroup();

    void setId(Key workspace, const QString& id);
    void setName(Key workspace, const QString& name);
    /// Empty: the compositor no longer orders the workspace geometrically.
    void setCoordinates(Key workspace, const QList<uint32_t>& coordinates);
    void setState(Key workspace, uint32_t state);
    void enterGroup(Key group, Key workspace);
    void leaveGroup(Key group, Key workspace);
    void removeWorkspace(Key workspace);
    void removeGroup(Key group);

    /// manager.done: publish the pending state. True when what
    /// workspaces() returns changed.
    bool commit();
    /// Forget everything, published state included (the global is gone).
    /// True when the published list was not empty.
    bool clear();

    /// The published list, in display order: by workspace group in the
    /// order the groups appeared, then by coordinates, then in the order
    /// the workspaces appeared. Hidden workspaces are left out.
    [[nodiscard]] QList<WorkspaceEntry> workspaces() const;
    /// The workspace a published id names, or 0.
    [[nodiscard]] Key keyFor(const QString& id) const;

private:
    struct Workspace
    {
        QString protocolId;
        QString name;
        QList<uint32_t> coordinates;
        uint32_t state = 0;
        Key group = 0;
    };

    Key m_nextKey = 1;
    // Keys only grow, so a key's order is the order of appearance.
    QHash<Key, Workspace> m_workspaces;
    QList<Key> m_groups;
    QList<WorkspaceEntry> m_published;
    QHash<QString, Key> m_publishedKeys;
};

} // namespace PhosphorShell
