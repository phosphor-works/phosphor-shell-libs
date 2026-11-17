// SPDX-FileCopyrightText: 2026 fuddlesworth
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

// Internal (not installed): the wlr-gamma-control-unstable-v1 client.

#include <PhosphorServiceNightLight/GammaBackend.h>

#include <cstdint>
#include <memory>
#include <vector>

struct wl_display;
struct wl_registry;
struct wl_output;
struct zwlr_gamma_control_manager_v1;
struct zwlr_gamma_control_v1;

namespace PhosphorServiceNightLight {

/// Binds zwlr_gamma_control_manager_v1 and every wl_output on the session's
/// Wayland display (Qt's, through QNativeInterface::QWaylandApplication), and
/// while active holds one zwlr_gamma_control_v1 per output, setting its ramps
/// from a sealed memfd. Destroying a control (on deactivation, output removal
/// or destruction of the backend) makes the compositor restore that output's
/// original ramps.
///
/// Without a Wayland platform it stays empty and unavailable. Events arrive on
/// the display's default queue, which Qt dispatches on the GUI thread.
class WaylandGammaBackend : public GammaBackend
{
    Q_OBJECT

public:
    explicit WaylandGammaBackend(QObject* parent = nullptr);
    ~WaylandGammaBackend() override;

    bool isAvailable() const override;
    void setActive(bool active) override;
    void setWhitePoint(const WhitePoint& white) override;
    void retry() override;

private:
    struct Output
    {
        WaylandGammaBackend* backend = nullptr;
        uint32_t name = 0;
        wl_output* output = nullptr;
        zwlr_gamma_control_v1* control = nullptr;
        uint32_t size = 0;
        bool failed = false;
    };
    struct Listeners;
    friend struct Listeners;

    void acquire(Output& output);
    void release(Output& output);
    void apply(Output& output);
    void fail(Output& output);
    void releaseManager();
    void teardown();
    void updateAvailable();
    void flush();

    wl_display* m_display = nullptr;
    wl_registry* m_registry = nullptr;
    zwlr_gamma_control_manager_v1* m_manager = nullptr;
    uint32_t m_managerName = 0;
    std::vector<std::unique_ptr<Output>> m_outputs;
    WhitePoint m_white;
    bool m_active = false;
    bool m_available = false;
};

} // namespace PhosphorServiceNightLight
