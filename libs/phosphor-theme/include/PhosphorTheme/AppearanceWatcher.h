// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include <PhosphorTheme/phosphortheme_export.h>
#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantMap>

namespace PhosphorTheme {

// The cross-process view of saved appearance or its live, locked preview.
// Native decorations, the compositor, and application adapters share this
// reader so an orphaned preview cannot become a permanent desktop style.
// `appearancePath` is the file the producing app's AppearanceStore writes.
class PHOSPHORTHEME_EXPORT AppearanceWatcher : public QObject
{
    Q_OBJECT
public:
    explicit AppearanceWatcher(const QString& appearancePath, QObject* parent = nullptr);
    QVariantMap values() const
    {
        return m_values;
    }

Q_SIGNALS:
    void changed();

private:
    void reload();
    void watchPaths();
    QString m_appearancePath;
    QVariantMap m_values;
    QFileSystemWatcher m_watcher;
    QTimer m_reload;
    QTimer m_previewPoll;
};

}
