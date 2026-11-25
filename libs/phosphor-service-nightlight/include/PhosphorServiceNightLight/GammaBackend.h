// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <PhosphorServiceNightLight/ColorTemperature.h>
#include <PhosphorServiceNightLight/phosphorservicenightlight_export.h>

#include <QObject>

namespace PhosphorServiceNightLight {

/**
 * @brief What NightLight drives: something that can tint every output.
 *
 * The default backend is a wlr-gamma-control-unstable-v1 client
 * (NightLight's one-argument constructor creates it). Another backend, or a
 * fake in a test, is handed to NightLight's injecting constructor.
 *
 * A backend holds the outputs' gamma only while active. Releasing it gives
 * each output back its original ramps.
 */
class PHOSPHORSERVICENIGHTLIGHT_EXPORT GammaBackend : public QObject
{
    Q_OBJECT

public:
    explicit GammaBackend(QObject* parent = nullptr);
    ~GammaBackend() override;

    /// Whether the backend can tint at least one output. Before the first
    /// activation this is the best the backend knows without taking the
    /// outputs (for the Wayland backend: the compositor offers gamma control
    /// and has an output). An output whose control failed stops counting until
    /// retry().
    [[nodiscard]] virtual bool isAvailable() const = 0;

    /// Take (true) or release (false) the outputs' gamma. While active the
    /// backend applies the last white point to every output it controls,
    /// including outputs that appear later.
    virtual void setActive(bool active) = 0;

    /// The colour to tint every controlled output with. Remembered while
    /// inactive and applied on activation.
    virtual void setWhitePoint(const WhitePoint& white) = 0;

    /// Try again on outputs whose control failed (for example after another
    /// program that held their gamma has quit).
    virtual void retry() = 0;

Q_SIGNALS:
    void availableChanged();
};

} // namespace PhosphorServiceNightLight
