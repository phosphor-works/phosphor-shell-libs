// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

/**
 * @file PhosphorServiceIconTheme.h
 * @brief Umbrella header for the PhosphorServiceIconTheme library.
 *
 * Exposes:
 * - `IconThemeResolver`, XDG Icon Theme Specification 0.13 lookup
 *   (`iconForName(name, size, scale, extraThemeDir)`) shared across
 *   shells and any consumer that needs spec-compliant theme walks.
 * - `IconImageProvider`, Qt image provider mounted at
 *   `image://phosphor-service-icontheme/` that holds a thread-safe
 *   `QImage` registry so models can hand QML a URL even when the
 *   payload is a raw bitmap rather than a file.
 */

#include <PhosphorServiceIconTheme/IconThemeResolver.h>
#include <PhosphorServiceIconTheme/IconImageProvider.h>
#include <PhosphorServiceIconTheme/QmlRegistration.h>
