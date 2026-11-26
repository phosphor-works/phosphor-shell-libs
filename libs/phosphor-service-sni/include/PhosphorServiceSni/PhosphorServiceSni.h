// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

/**
 * @file PhosphorServiceSni.h
 * @brief Umbrella header for the PhosphorServiceSni library.
 *
 * PhosphorServiceSni implements the `org.kde.StatusNotifierItem` /
 * `org.kde.StatusNotifierWatcher` system-tray protocol plus the
 * `com.canonical.dbusmenu` context-menu model that tray items use to
 * expose their right-click menus. The shell registers a
 * `StatusNotifierHost`, binds its QML tray view to
 * `StatusNotifierItemModel`, and the library handles watcher
 * registration, item discovery, icon URL publishing, and menu
 * activation.
 *
 * Icon resolution + the `image://phosphor-service-icontheme/` provider
 * are provided by the sibling `phosphor-service-icontheme` library.
 */

#include <PhosphorServiceSni/StatusNotifierItem.h>
#include <PhosphorServiceSni/StatusNotifierItemModel.h>
#include <PhosphorServiceSni/TrayItems.h>
#include <PhosphorServiceSni/StatusNotifierHost.h>
#include <PhosphorServiceSni/DBusMenuModel.h>
#include <PhosphorServiceSni/QmlRegistration.h>
