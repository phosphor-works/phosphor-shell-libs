// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <PhosphorShell/Workspaces.h>

#include "extworkspaceclient_p.h"
#include "workspacebackend_p.h"

#include <QCoreApplication>

namespace {
/// The label for a workspace the compositor names empty. Through
/// QCoreApplication::translate with this library's own context, the shape
/// the shell libraries use for their strings: it ships in English until a
/// catalogue for the context is extracted.
QString fallbackWorkspaceName(int position)
{
    return QCoreApplication::translate("PhosphorShell", "Workspace %1").arg(position);
}

QString displayName(const PhosphorShell::WorkspaceEntry& entry, int position)
{
    return entry.name.isEmpty() ? fallbackWorkspaceName(position) : entry.name;
}

/// Row of the first active workspace, or -1.
int activeRowOf(const QList<PhosphorShell::WorkspaceEntry>& entries)
{
    for (int row = 0; row < entries.size(); ++row) {
        if (entries.at(row).active) {
            return row;
        }
    }
    return -1;
}
} // namespace

namespace PhosphorShell {

// =====================================================================
// WorkspaceListModel
// =====================================================================

WorkspaceListModel::WorkspaceListModel(WorkspaceBackend* backend, QObject* parent)
    : QAbstractListModel(parent)
    , m_backend(backend)
{
    if (!m_backend) {
        return;
    }
    connect(m_backend.data(), &WorkspaceBackend::workspacesChanged, this, &WorkspaceListModel::reload);
    reload();
}

WorkspaceListModel::~WorkspaceListModel() = default;

int WorkspaceListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_ids.size();
}

QVariant WorkspaceListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_ids.size()) {
        return {};
    }
    switch (role) {
    case IdRole:
        return m_ids.at(index.row());
    case NameRole:
        return m_names.at(index.row());
    case IsActiveRole:
        return index.row() == m_activeRow;
    default:
        return {};
    }
}

QHash<int, QByteArray> WorkspaceListModel::roleNames() const
{
    return {
        {IdRole, "workspaceId"},
        {NameRole, "name"},
        {IsActiveRole, "isActive"},
    };
}

void WorkspaceListModel::reload()
{
    const QList<WorkspaceEntry> entries = m_backend ? m_backend->workspaces() : QList<WorkspaceEntry>();
    QStringList ids;
    QStringList names;
    ids.reserve(entries.size());
    names.reserve(entries.size());
    for (int row = 0; row < entries.size(); ++row) {
        ids.append(entries.at(row).id);
        names.append(displayName(entries.at(row), row + 1));
    }
    const int activeRow = activeRowOf(entries);

    if (ids != m_ids || names != m_names) {
        beginResetModel();
        m_ids = ids;
        m_names = names;
        m_activeRow = activeRow;
        endResetModel();
        return;
    }
    if (activeRow == m_activeRow) {
        return;
    }
    // Same list, a different active workspace: two targeted dataChanged
    // emissions rather than a reset. A pager rebuilt on every workspace
    // switch would drop hover state and restart any transition mid-flight.
    const int previous = m_activeRow;
    m_activeRow = activeRow;
    if (previous >= 0) {
        Q_EMIT dataChanged(index(previous), index(previous), {IsActiveRole});
    }
    if (m_activeRow >= 0) {
        Q_EMIT dataChanged(index(m_activeRow), index(m_activeRow), {IsActiveRole});
    }
}

// =====================================================================
// Workspaces (singleton facade)
// =====================================================================

Workspaces::Workspaces(QObject* parent)
    : Workspaces(ExtWorkspaceClient::shared(), parent)
{
}

Workspaces::Workspaces(WorkspaceBackend* backend, QObject* parent)
    : QObject(parent)
    , m_backend(backend)
    // Always a model, even with no backend, so QML can bind
    // `Repeater { model: Workspaces.model }` unconditionally.
    , m_model(new WorkspaceListModel(backend, this))
{
    if (!m_backend) {
        return;
    }
    m_activeId = activeId();
    m_activeName = activeName();
    m_activeIndex = activeIndex();
    m_count = count();
    connect(m_backend.data(), &WorkspaceBackend::workspacesChanged, this, &Workspaces::sync);
    connect(m_backend.data(), &WorkspaceBackend::supportedChanged, this, &Workspaces::supportedChanged);
}

Workspaces::~Workspaces() = default;

Workspaces* Workspaces::create(QQmlEngine* engine, QJSEngine* scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)
    // Per-engine wrapper over the process-wide client, matching Toplevels:
    // the QML registry deletes the wrapper on engine teardown while the
    // client (parented to qApp) survives the hot reload.
    return new Workspaces();
}

void Workspaces::sync()
{
    // activeChanged covers activeId, activeName and activeIndex, so it is
    // gated on any of them moving. A workspace added or removed before the
    // current one renumbers the position while the id stays put.
    const QString nextId = activeId();
    const QString nextName = activeName();
    const int nextIndex = activeIndex();
    if (nextId != m_activeId || nextName != m_activeName || nextIndex != m_activeIndex) {
        m_activeId = nextId;
        m_activeName = nextName;
        m_activeIndex = nextIndex;
        Q_EMIT activeChanged();
    }
    const int nextCount = count();
    if (nextCount != m_count) {
        m_count = nextCount;
        Q_EMIT countChanged();
    }
}

QAbstractListModel* Workspaces::model() const
{
    return m_model;
}

QString Workspaces::activeId() const
{
    if (!m_backend) {
        return {};
    }
    const auto entries = m_backend->workspaces();
    const int row = activeRowOf(entries);
    return row >= 0 ? entries.at(row).id : QString();
}

QString Workspaces::activeName() const
{
    if (!m_backend) {
        return {};
    }
    const auto entries = m_backend->workspaces();
    const int row = activeRowOf(entries);
    return row >= 0 ? displayName(entries.at(row), row + 1) : QString();
}

int Workspaces::activeIndex() const
{
    return m_backend ? activeRowOf(m_backend->workspaces()) + 1 : 0;
}

int Workspaces::count() const
{
    return m_backend ? int(m_backend->workspaces().size()) : 0;
}

bool Workspaces::isSupported() const
{
    return m_backend && m_backend->isSupported();
}

void Workspaces::switchTo(const QString& id)
{
    if (m_backend && !id.isEmpty()) {
        m_backend->activate(id);
    }
}

} // namespace PhosphorShell
