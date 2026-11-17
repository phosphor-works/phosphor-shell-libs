// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <PhosphorShell/phosphorshell_export.h>

#include <QList>
#include <QObject>
#include <QString>

namespace PhosphorShell {

/// One workspace as the shell shows it, in display order.
struct WorkspaceEntry
{
    /// Unique and stable while the workspace exists. The compositor's own
    /// id when it sends one, otherwise one this process made up.
    QString id;
    QString name;
    bool active = false;
    bool urgent = false;

    friend bool operator==(const WorkspaceEntry&, const WorkspaceEntry&) = default;
};

/// Where Workspaces and WorkspaceListModel read the workspace list from.
/// ExtWorkspaceClient is the real one; tests hand in their own.
class PHOSPHORSHELL_EXPORT WorkspaceBackend : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    /// Whether a workspace source is there at all: for ExtWorkspaceClient,
    /// whether the compositor currently advertises ext_workspace_manager_v1.
    [[nodiscard]] virtual bool isSupported() const = 0;
    /// The current list, in display order, without hidden workspaces.
    [[nodiscard]] virtual QList<WorkspaceEntry> workspaces() const = 0;
    /// Ask for the workspace with this id. Unknown ids are ignored. The
    /// answer, if any, arrives through workspacesChanged.
    virtual void activate(const QString& id) = 0;

Q_SIGNALS:
    void supportedChanged();
    /// The list or any entry in it changed, as one atomic update.
    void workspacesChanged();
};

} // namespace PhosphorShell
