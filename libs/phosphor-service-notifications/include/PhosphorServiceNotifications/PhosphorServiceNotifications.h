// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

/**
 * @file PhosphorServiceNotifications.h
 * @brief Umbrella header for the PhosphorServiceNotifications library.
 *
 * PhosphorServiceNotifications is the session-bus server for
 * `org.freedesktop.Notifications` (Desktop Notifications Spec 1.3). It owns the
 * well-known name and answers it: the library IS the notification daemon, not a
 * client of one. The interface is exported through a generated adaptor
 * (`qt6_add_dbus_adaptor`) that forwards to `NotificationServer`. The library
 * has no UI of its own: it stores and lifecycles notifications and surfaces
 * them as Qt/QML types. phosphor-shell-notifications draws the toasts and
 * the notification center.
 *
 * Shells consume:
 *   - `NotificationServer`: owns the bus name, ingest, id allocation, expiry,
 *     and the close/action lifecycle.
 *   - `Notification`: one decoded live notification (summary / body / actions /
 *     urgency / image / hints), mutated in place on `replaces_id`.
 *   - `NotificationModel`: a `QAbstractListModel` over a server's live
 *     notifications, for a `ListView` / `Repeater` to bind.
 */

#include <PhosphorServiceNotifications/Notification.h>
#include <PhosphorServiceNotifications/NotificationModel.h>
#include <PhosphorServiceNotifications/NotificationServer.h>
#include <PhosphorServiceNotifications/QmlRegistration.h>
