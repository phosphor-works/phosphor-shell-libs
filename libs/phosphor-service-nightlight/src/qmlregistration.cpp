// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <PhosphorServiceNightLight/QmlRegistration.h>

#include <PhosphorServiceNightLight/NightLight.h>

#include <QQmlEngine>

#include <mutex>

namespace PhosphorServiceNightLight {

namespace {
constexpr int kModuleVersionMajor = 1;
constexpr int kModuleVersionMinor = 0;
constexpr const char* kModule = "Phosphor.Service.NightLight";
} // namespace

void registerQmlTypes()
{
    // qmlRegister* is process-global, not per-engine; the call_once guard makes
    // repeat calls from every engine setup safe, matching the sibling service
    // libraries.
    static std::once_flag once;
    std::call_once(once, [] {
        // Instantiable, not a singleton: the gamma of the outputs belongs to one
        // night light per process, and the app decides which object that is.
        qmlRegisterType<NightLight>(kModule, kModuleVersionMajor, kModuleVersionMinor, "NightLight");
    });
}

} // namespace PhosphorServiceNightLight
