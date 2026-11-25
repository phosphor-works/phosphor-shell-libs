// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "waylandgammabackend.h"

#include "wlr-gamma-control-unstable-v1-client-protocol.h"

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QtGui/qguiapplication_platform.h>

#include <wayland-client.h>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcNightLightWayland, "phosphor.service.nightlight.wayland")

namespace PhosphorServiceNightLight {

namespace {

// Writes all of @p table to a new sealed memfd, rewound to its start. -1 on
// failure.
int tableFile(const QByteArray& table)
{
    const int fd = memfd_create("phosphor-night-light-gamma", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0) {
        return -1;
    }
    qsizetype written = 0;
    while (written < table.size()) {
        const ssize_t n = ::write(fd, table.constData() + written, size_t(table.size() - written));
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            ::close(fd);
            return -1;
        }
        written += n;
    }
    // A compositor may read() rather than mmap or pread, so start it at the top.
    if (::lseek(fd, 0, SEEK_SET) != 0) {
        ::close(fd);
        return -1;
    }
    ::fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE | F_SEAL_SEAL);
    return fd;
}

} // namespace

struct WaylandGammaBackend::Listeners
{
    static void global(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
    {
        // Both interfaces are bound at version 1, which every version offers.
        Q_UNUSED(version)
        auto* self = static_cast<WaylandGammaBackend*>(data);
        if (std::strcmp(interface, wl_output_interface.name) == 0) {
            auto output = std::make_unique<Output>();
            output->backend = self;
            output->name = name;
            // Version 1: the output is only a handle for get_gamma_control, its
            // events (geometry, mode) are not needed and go unheard.
            output->output = static_cast<wl_output*>(wl_registry_bind(registry, name, &wl_output_interface, 1));
            Output& added = *output;
            self->m_outputs.push_back(std::move(output));
            if (self->m_active) {
                self->acquire(added);
                self->flush();
            }
            self->updateAvailable();
        } else if (std::strcmp(interface, zwlr_gamma_control_manager_v1_interface.name) == 0 && !self->m_manager) {
            self->m_manager = static_cast<zwlr_gamma_control_manager_v1*>(
                wl_registry_bind(registry, name, &zwlr_gamma_control_manager_v1_interface, 1));
            self->m_managerName = name;
            if (self->m_active) {
                for (auto& output : self->m_outputs) {
                    self->acquire(*output);
                }
                self->flush();
            }
            self->updateAvailable();
        }
    }

    static void globalRemove(void* data, wl_registry*, uint32_t name)
    {
        auto* self = static_cast<WaylandGammaBackend*>(data);
        if (self->m_manager && name == self->m_managerName) {
            self->releaseManager();
            self->updateAvailable();
            return;
        }
        for (auto it = self->m_outputs.begin(); it != self->m_outputs.end(); ++it) {
            if ((*it)->name != name) {
                continue;
            }
            self->release(**it);
            wl_output_destroy((*it)->output);
            self->m_outputs.erase(it);
            self->flush();
            self->updateAvailable();
            return;
        }
    }

    static void gammaSize(void* data, zwlr_gamma_control_v1* control, uint32_t size)
    {
        auto* output = static_cast<Output*>(data);
        if (!output || output->control != control) {
            return;
        }
        qCDebug(lcNightLightWayland) << "Output" << output->name << "has gamma ramps of" << size << "entries";
        if (size < 2) {
            // An output without usable ramps; the protocol has no other way to say so.
            output->backend->fail(*output);
            return;
        }
        output->size = size;
        output->backend->apply(*output);
        output->backend->flush();
    }

    static void failed(void* data, zwlr_gamma_control_v1* control)
    {
        auto* output = static_cast<Output*>(data);
        if (!output || output->control != control) {
            return;
        }
        output->backend->fail(*output);
    }

    static constexpr wl_registry_listener registry = {
        .global = global,
        .global_remove = globalRemove,
    };

    static constexpr zwlr_gamma_control_v1_listener control = {
        .gamma_size = gammaSize,
        .failed = failed,
    };
};

WaylandGammaBackend::WaylandGammaBackend(QObject* parent)
    : GammaBackend(parent)
{
    // qGuiApp is a plain cast; under a QCoreApplication it is not a QGuiApplication.
    auto* gui = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    auto* wayland = gui ? gui->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr;
    m_display = wayland ? wayland->display() : nullptr;
    if (!m_display) {
        qCDebug(lcNightLightWayland) << "Not a Wayland session; night light has no outputs to control";
        return;
    }
    m_registry = wl_display_get_registry(m_display);
    wl_registry_add_listener(m_registry, &Listeners::registry, this);
    flush();
    // The display goes with the platform integration, which QGuiApplication
    // deletes after it emits destroyed(). A backend that outlives the
    // application lets go of its objects here, while the connection is open.
    connect(gui, &QObject::destroyed, this, [this] {
        teardown();
    });
}

WaylandGammaBackend::~WaylandGammaBackend()
{
    teardown();
}

bool WaylandGammaBackend::isAvailable() const
{
    return m_available;
}

void WaylandGammaBackend::setActive(bool active)
{
    if (active == m_active) {
        return;
    }
    m_active = active;
    for (auto& output : m_outputs) {
        if (active) {
            acquire(*output);
        } else {
            release(*output);
        }
    }
    flush();
}

void WaylandGammaBackend::setWhitePoint(const WhitePoint& white)
{
    if (white == m_white) {
        return;
    }
    m_white = white;
    if (!m_active) {
        return;
    }
    for (auto& output : m_outputs) {
        apply(*output);
    }
    flush();
}

void WaylandGammaBackend::retry()
{
    for (auto& output : m_outputs) {
        output->failed = false;
        if (m_active) {
            acquire(*output);
        }
    }
    flush();
    updateAvailable();
}

void WaylandGammaBackend::acquire(Output& output)
{
    if (!m_manager || output.control || output.failed) {
        return;
    }
    output.size = 0;
    output.control = zwlr_gamma_control_manager_v1_get_gamma_control(m_manager, output.output);
    zwlr_gamma_control_v1_add_listener(output.control, &Listeners::control, &output);
}

void WaylandGammaBackend::release(Output& output)
{
    if (!output.control) {
        return;
    }
    // The protocol's destructor: the compositor restores the original ramps.
    zwlr_gamma_control_v1_destroy(output.control);
    output.control = nullptr;
    output.size = 0;
}

void WaylandGammaBackend::apply(Output& output)
{
    if (!output.control || output.size < 2) {
        return;
    }
    const int fd = tableFile(gammaTable(m_white, output.size));
    if (fd < 0) {
        qCWarning(lcNightLightWayland) << "Could not write a gamma table:" << std::strerror(errno);
        return;
    }
    zwlr_gamma_control_v1_set_gamma(output.control, fd);
    // libwayland duplicates the descriptor into the request.
    ::close(fd);
}

void WaylandGammaBackend::fail(Output& output)
{
    qCInfo(lcNightLightWayland) << "The compositor refused gamma control of output" << output.name
                                << "(another program may hold it, or the output has no gamma ramps)";
    release(output);
    output.failed = true;
    flush();
    updateAvailable();
}

void WaylandGammaBackend::releaseManager()
{
    for (auto& output : m_outputs) {
        release(*output);
    }
    if (m_manager) {
        zwlr_gamma_control_manager_v1_destroy(m_manager);
        m_manager = nullptr;
        m_managerName = 0;
    }
    flush();
}

void WaylandGammaBackend::teardown()
{
    if (!m_display) {
        return;
    }
    releaseManager();
    for (auto& output : m_outputs) {
        wl_output_destroy(output->output);
    }
    m_outputs.clear();
    wl_registry_destroy(m_registry);
    m_registry = nullptr;
    flush();
    m_display = nullptr;
}

void WaylandGammaBackend::updateAvailable()
{
    bool available = false;
    if (m_manager) {
        for (const auto& output : m_outputs) {
            if (!output->failed) {
                available = true;
                break;
            }
        }
    }
    if (available == m_available) {
        return;
    }
    m_available = available;
    Q_EMIT availableChanged();
}

void WaylandGammaBackend::flush()
{
    if (m_display) {
        wl_display_flush(m_display);
    }
}

} // namespace PhosphorServiceNightLight
