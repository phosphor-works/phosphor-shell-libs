// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <PhosphorShell/phosphorshell_export.h>

#include <QAbstractListModel>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QQmlEngine;
class QJSEngine;
QT_END_NAMESPACE

namespace PhosphorShell {

class WorkspaceBackend;

/**
 * @brief Incrementally-updating model of the compositor's workspaces.
 *
 * Roles:
 *   - `workspaceId` — the workspace's identifier, unique and stable while
 *                    the workspace exists. Not `id`: QML's parser rejects
 *                    `id` as a delegate property name, and qmlformat fails
 *                    outright on it.
 *   - `name`        — the user-visible label.
 *   - `isActive`    — whether this is the current workspace.
 *
 * A workspace switch emits `dataChanged` over `isActive` only, so a pager
 * re-styles two pills instead of rebuilding every delegate. The list is
 * reset only when workspaces are actually added, removed, reordered, or
 * renamed.
 */
class PHOSPHORSHELL_EXPORT WorkspaceListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        IsActiveRole,
    };
    Q_ENUM(Role)

    /// Null `backend`: an empty model.
    explicit WorkspaceListModel(WorkspaceBackend* backend, QObject* parent = nullptr);
    ~WorkspaceListModel() override;

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

private:
    void reload();

    QPointer<WorkspaceBackend> m_backend;
    QStringList m_ids;
    QStringList m_names;
    // 0-based row of the active workspace, or -1 when none is.
    int m_activeRow = -1;
};

/**
 * @brief QML singleton exposing the compositor's workspaces.
 *
 * Usage:
 *
 *     import Phosphor.Shell
 *
 *     Repeater {
 *         model: Workspaces.model
 *         delegate: Rectangle {
 *             required property string workspaceId
 *             required property string name
 *             required property bool isActive
 *             TapHandler { onTapped: Workspaces.switchTo(workspaceId) }
 *         }
 *     }
 *
 * The workspaces come from the compositor over ext-workspace-v1 (staging
 * in wayland-protocols). Every workspace group is listed, groups in the
 * order the compositor announced them, each ordered by its coordinates;
 * hidden workspaces are left out. When several groups (one per output, say)
 * each have an active workspace, `activeId` is the first of them in that
 * order.
 *
 * ## Identifiers, not indices
 *
 * The model's `workspaceId` role and `switchTo(id)` are the whole interface for
 * addressing a workspace. Positions renumber whenever a workspace is
 * created or removed, so anything holding a workspace across such an event
 * — a pager delegate mid-animation, a rule, a shortcut — must key on the
 * id.
 *
 * `supported` is true while the compositor advertises
 * ext_workspace_manager_v1, and follows the global as it appears and
 * disappears. The global arrives asynchronously, so a shell reading it at
 * startup sees false first and then true. Without it everything is empty:
 * `count` is 0 and `activeId` is empty.
 */
// Registered as a QML singleton imperatively in ShellEngine::load(), same
// as Toplevels; the QML_* macros would need qt_add_qml_module, which this
// library does not use.
class PHOSPHORSHELL_EXPORT Workspaces : public QObject
{
    Q_OBJECT

    /// Bind `Repeater { model: Workspaces.model }`.
    Q_PROPERTY(QAbstractListModel* model READ model CONSTANT)
    /// Identifier of the current workspace, or empty when there is none.
    Q_PROPERTY(QString activeId READ activeId NOTIFY activeChanged)
    Q_PROPERTY(QString activeName READ activeName NOTIFY activeChanged)
    /// 1-based position of the current workspace, for a plain numeric
    /// readout, 0 when there is none. Do NOT use it to address a
    /// workspace — see the class docs.
    Q_PROPERTY(int activeIndex READ activeIndex NOTIFY activeChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool supported READ isSupported NOTIFY supportedChanged)

public:
    /// Over the process-wide ext-workspace client on Qt's Wayland
    /// connection.
    explicit Workspaces(QObject* parent = nullptr);
    /// Over `backend`, which must outlive this object or be destroyed with
    /// it. Null: always unsupported and empty.
    explicit Workspaces(WorkspaceBackend* backend, QObject* parent = nullptr);
    ~Workspaces() override;

    /// QML-singleton factory hook.
    [[nodiscard]] static Workspaces* create(QQmlEngine* engine, QJSEngine* scriptEngine);

    [[nodiscard]] QAbstractListModel* model() const;
    [[nodiscard]] QString activeId() const;
    [[nodiscard]] QString activeName() const;
    [[nodiscard]] int activeIndex() const;
    [[nodiscard]] int count() const;
    [[nodiscard]] bool isSupported() const;

    /// Switch to the workspace with this identifier. Unknown or empty ids
    /// are ignored. Asynchronous: the compositor confirms by way of the
    /// change signals, so `activeId` does not update on return.
    Q_INVOKABLE void switchTo(const QString& id);

Q_SIGNALS:
    void activeChanged();
    void countChanged();
    void supportedChanged();

private:
    void sync();

    QPointer<WorkspaceBackend> m_backend;
    WorkspaceListModel* m_model = nullptr;
    // Last-published values, so the notifications are change-gated rather
    // than mirroring every update the backend makes.
    QString m_activeId;
    QString m_activeName;
    int m_activeIndex = 0;
    int m_count = 0;
};

} // namespace PhosphorShell
