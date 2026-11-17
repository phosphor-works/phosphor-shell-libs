// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "workspacestate_p.h"

#include <algorithm>
#include <tuple>

namespace PhosphorShell {

WorkspaceState::Key WorkspaceState::addWorkspace()
{
    const Key key = m_nextKey++;
    m_workspaces.insert(key, Workspace{});
    return key;
}

WorkspaceState::Key WorkspaceState::addGroup()
{
    const Key key = m_nextKey++;
    m_groups.append(key);
    return key;
}

void WorkspaceState::setId(Key workspace, const QString& id)
{
    if (auto it = m_workspaces.find(workspace); it != m_workspaces.end()) {
        it->protocolId = id;
    }
}

void WorkspaceState::setName(Key workspace, const QString& name)
{
    if (auto it = m_workspaces.find(workspace); it != m_workspaces.end()) {
        it->name = name;
    }
}

void WorkspaceState::setCoordinates(Key workspace, const QList<uint32_t>& coordinates)
{
    if (auto it = m_workspaces.find(workspace); it != m_workspaces.end()) {
        it->coordinates = coordinates;
    }
}

void WorkspaceState::setState(Key workspace, uint32_t state)
{
    if (auto it = m_workspaces.find(workspace); it != m_workspaces.end()) {
        it->state = state;
    }
}

void WorkspaceState::enterGroup(Key group, Key workspace)
{
    if (auto it = m_workspaces.find(workspace); it != m_workspaces.end() && m_groups.contains(group)) {
        it->group = group;
    }
}

void WorkspaceState::leaveGroup(Key group, Key workspace)
{
    if (auto it = m_workspaces.find(workspace); it != m_workspaces.end() && it->group == group) {
        it->group = 0;
    }
}

void WorkspaceState::removeWorkspace(Key workspace)
{
    m_workspaces.remove(workspace);
}

void WorkspaceState::removeGroup(Key group)
{
    m_groups.removeOne(group);
    for (auto& workspace : m_workspaces) {
        if (workspace.group == group) {
            workspace.group = 0;
        }
    }
}

bool WorkspaceState::commit()
{
    QList<Key> keys;
    keys.reserve(m_workspaces.size());
    for (auto it = m_workspaces.cbegin(); it != m_workspaces.cend(); ++it) {
        if (!(it->state & StateHidden)) {
            keys.append(it.key());
        }
    }
    // A workspace in no group sorts after every grouped one. Within a group
    // the coordinates decide, and a workspace without them follows the ones
    // that have them. The key, the order of appearance, settles the rest.
    const auto rank = [this](Key key) {
        const Workspace& workspace = m_workspaces[key];
        const qsizetype group = m_groups.indexOf(workspace.group);
        return std::make_tuple(group < 0 ? m_groups.size() : group, workspace.coordinates.isEmpty(),
                               workspace.coordinates, key);
    };
    std::sort(keys.begin(), keys.end(), [&rank](Key a, Key b) {
        return rank(a) < rank(b);
    });

    QList<WorkspaceEntry> next;
    QHash<QString, Key> nextKeys;
    next.reserve(keys.size());
    for (const Key key : std::as_const(keys)) {
        const Workspace& workspace = m_workspaces[key];
        WorkspaceEntry entry;
        // The protocol's id is optional. Without one the key stands in: it
        // is unique and lasts as long as the workspace does, which is all
        // the model's workspaceId promises.
        entry.id = workspace.protocolId.isEmpty() ? QStringLiteral("workspace-%1").arg(key) : workspace.protocolId;
        entry.name = workspace.name;
        entry.active = workspace.state & StateActive;
        entry.urgent = workspace.state & StateUrgent;
        nextKeys.insert(entry.id, key);
        next.append(entry);
    }
    m_publishedKeys = nextKeys;
    if (next == m_published) {
        return false;
    }
    m_published = next;
    return true;
}

bool WorkspaceState::clear()
{
    const bool hadPublished = !m_published.isEmpty();
    m_workspaces.clear();
    m_groups.clear();
    m_published.clear();
    m_publishedKeys.clear();
    return hadPublished;
}

QList<WorkspaceEntry> WorkspaceState::workspaces() const
{
    return m_published;
}

WorkspaceState::Key WorkspaceState::keyFor(const QString& id) const
{
    const Key key = m_publishedKeys.value(id, 0);
    // A workspace removed since the last commit is still published until the
    // next one, but there is nothing left to ask for.
    return m_workspaces.contains(key) ? key : 0;
}

} // namespace PhosphorShell
