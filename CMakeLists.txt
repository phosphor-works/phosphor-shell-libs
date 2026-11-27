# SPDX-FileCopyrightText: 2026 Phosphor Works Team
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Phosphor shell libraries: the theme and popout foundation, the Phosphor.* QML
# surfaces, and the per-domain system service libraries a desktop shell is built
# from. They build on phosphor-libs.
#
# Two ways in:
#
#   * On its own. cmake --workflow --preset dev configures, builds and tests
#     every library here. phosphor-libs comes from the commit
#     cmake/PhosphorShellLibsDependencies.cmake pins, fetched with CPM, or from
#     a local checkout with -DCPM_phosphor-libs_SOURCE=<dir>.
#   * As a subproject of an app. Lambent pins one commit of this repository
#     and adds it with CPM after phosphor-libs, so the app's phosphor-libs pin
#     wins and this tree fetches nothing of its own. CPM's
#     -DCPM_phosphor-shell-libs_SOURCE=<dir> builds a local checkout instead.
#
# As a subproject the app's top level has already set up ECM, the KDE settings,
# Qt and the compile definitions below. Each of those steps here is either
# idempotent or runs only at the top level, so the app's settings are the ones
# in force.

# 3.23: the phosphor-libs tree added below asks for it (target_sources with
# FILE_SET). The presets (CMakePresets.json) need 3.25.
cmake_minimum_required(VERSION 3.23)

project(PhosphorShellLibs VERSION 0.1.0 LANGUAGES C CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(PHOSPHOR_SHELL_LIBS_TOP_LEVEL ON)
else()
    set(PHOSPHOR_SHELL_LIBS_TOP_LEVEL OFF)
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Top-level setup. Skipped as a subproject, where the app has done it.
# ═══════════════════════════════════════════════════════════════════════════════

if(PHOSPHOR_SHELL_LIBS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOMOC_PARALLEL_ENABLED ON)

    option(CMAKE_UNITY_BUILD "Enable unity builds for faster compilation" ON)
    set(CMAKE_UNITY_BUILD_BATCH_SIZE 16 CACHE STRING "Number of sources per unity batch")
    option(ENABLE_PCH "Share one precompiled header across the test targets" ON)

    find_program(CCACHE_PROGRAM ccache)
    if(CCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
        set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    endif()

    set(QT_MIN_VERSION "6.10.0")
    set(KF_MIN_VERSION "6.26.0")

    find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
    set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

    # Must come before KDECMakeSettings, which includes CTest and locks
    # BUILD_TESTING into the cache as ON.
    option(BUILD_TESTING "Build the test suite" OFF)

    include(KDEInstallDirs)
    include(KDECMakeSettings)
    include(KDECompilerSettings NO_POLICY_SCOPE)
    include(FeatureSummary)

    # Makes the Qt6 string-literal rule a compile error rather than a
    # convention: no implicit const char* to QString.
    add_compile_definitions(QT_NO_CAST_FROM_ASCII)

    include(KDEClangFormat)
    file(GLOB_RECURSE PHOSPHOR_SHELL_LIBS_CLANG_FORMAT_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/*.h
    )
    kde_clang_format(${PHOSPHOR_SHELL_LIBS_CLANG_FORMAT_FILES})

    # The shell libraries draw their icons through Kirigami and have no Qt-only
    # fallback, so the KDE frameworks are not optional here.
    find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS Kirigami)
endif()

# The libraries' tests build only in this repository's own build, or in a
# library configured on its own. An app or the workspace superbuild that adds
# the tree never builds them: each repository runs only its own tests.
if(BUILD_TESTING AND PHOSPHOR_SHELL_LIBS_TOP_LEVEL)
    set(PHOSPHOR_SHELL_LIBS_TESTS ON)
else()
    set(PHOSPHOR_SHELL_LIBS_TESTS OFF)
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Shared lookups. find_package is idempotent, so these also run as a subproject.
# ═══════════════════════════════════════════════════════════════════════════════
# At this scope, before phosphor-libs is added. An IMPORTED target belongs to the
# directory that found it, and phosphor-libs' own top level, which repeats these
# lookups, is a subdirectory here: the targets it found there (Qt's private
# modules, XKB through GuiPrivate) would be invisible to the libraries below, and
# Qt's QML plugin dependency walk warns for each one.

if(NOT DEFINED QT_MIN_VERSION)
    set(QT_MIN_VERSION "6.10.0")
endif()

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Concurrent
    Core
    DBus
    Gui
    Qml
    Quick
    QuickControls2
    ShaderTools
    Svg
    WaylandClient
    Widgets
)

# CMake's if(POLICY) knows only CMP policies. Qt's existence test for its own
# QTP policies is the QT_KNOWN_POLICY_<policy> variable.
if(QT_KNOWN_POLICY_QTP0001)
    qt_policy(SET QTP0001 NEW)
endif()
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

# The private Qt modules the libraries use (QRhi, QShaderBaker, the QML
# animation module's QuickPrivate, the Wayland QPA plugin). Looked up here,
# once, so each IMPORTED target is visible to every library and consumer.
# Some distros (Fedora) create these targets inside the public module's
# Config file instead of shipping standalone ones, hence the TARGET checks.
foreach(_phosphor_private GuiPrivate ShaderToolsPrivate QuickPrivate WaylandClientPrivate)
    if(NOT TARGET Qt6::${_phosphor_private})
        find_package(Qt6${_phosphor_private} CONFIG REQUIRED)
    endif()
endforeach()

# ═══════════════════════════════════════════════════════════════════════════════
# phosphor-libs
# ═══════════════════════════════════════════════════════════════════════════════
# The libraries every shell library links. Added first, so each find_package
# below resolves to its in-tree target. phosphor-libs' own top level does the Qt
# lookups (private modules included) every library here shares.

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PhosphorShellLibsDependencies.cmake")
phosphor_shell_libs_add_phosphor_libs()

# The shared test helpers (PhosphorTestIsolation, PhosphorTestPch) live in
# phosphor-libs, reachable by name from here on.
list(APPEND CMAKE_MODULE_PATH "${PHOSPHOR_LIBS_CMAKE_DIR}")

# ═══════════════════════════════════════════════════════════════════════════════
# Libraries
# ═══════════════════════════════════════════════════════════════════════════════
#
# Order matters: phosphor-shell links theme and popout, so they precede it.
# icontheme is consumed by sni and notifications, so it configures first among
# the services.

add_subdirectory(libs/phosphor-theme)   # M3 color-token store + Phosphor.Theme QML singletons
add_subdirectory(libs/phosphor-popout)  # central popout coordinator (lifetime, focus, scope arbitration)
add_subdirectory(libs/phosphor-shell)   # shell infrastructure and QML types
add_subdirectory(libs/phosphor-shell-widgets)  # Phosphor.Widgets M3 atom library; depends on phosphor-theme
add_subdirectory(libs/phosphor-shell-osd)      # Phosphor.OSD on-screen-display framework; depends on theme + widgets
add_subdirectory(libs/phosphor-shell-notifications) # Phosphor.Notifications toast framework; depends on theme + widgets
add_subdirectory(libs/phosphor-shell-control-center) # Phosphor.ControlCenter tile surface; depends on theme + widgets + Kirigami
add_subdirectory(libs/phosphor-shell-launcher)       # Launcher core: ILauncherProvider implementations, fzf-style matcher, .desktop scanner; depends on registry
add_subdirectory(libs/phosphor-shell-bar)      # Phosphor.Bar connected-corner bar surface. Depends on theme + widgets, and is one of the shell-tier libs that also require KF6 Kirigami (bar, power, control-center, launcher and lock)
add_subdirectory(libs/phosphor-shell-power)    # Phosphor.Power session menu; depends on theme + widgets + Kirigami
add_subdirectory(libs/phosphor-shell-dashboard) # Phosphor.Dashboard: dashboard grid + keybind cheatsheet on the placement map; depends on theme + widgets + protocol
add_subdirectory(libs/phosphor-shell-picker)   # Phosphor.Picker wallpaper and theme strip; depends on theme + widgets
add_subdirectory(libs/phosphor-shell-polkit)   # Phosphor.Polkit authentication prompt; depends on theme + widgets
add_subdirectory(libs/phosphor-shell-lock)     # Phosphor.Lock lockscreen surface; depends on theme + widgets + Kirigami + phosphor-wayland (lock surfaces)

# Order: icontheme is consumed by sni (SNI publisher uses the
# image provider + theme resolver), so configure it first. The
# other service libraries are independent.
add_subdirectory(libs/phosphor-service-icontheme) # XDG icon-theme resolver + Qt image provider
add_subdirectory(libs/phosphor-service-sni)       # StatusNotifierItem host + watcher + dbusmenu
add_subdirectory(libs/phosphor-service-upower)    # UPower battery / power-supply readouts
add_subdirectory(libs/phosphor-service-mpris)     # MPRIS2 media-player discovery + control
add_subdirectory(libs/phosphor-service-pipewire)  # PipeWire mixer
add_subdirectory(libs/phosphor-service-network)   # NetworkManager devices / connectivity
add_subdirectory(libs/phosphor-service-bluetooth) # BlueZ adapters / devices / pairing
add_subdirectory(libs/phosphor-service-brightness) # display / keyboard backlight brightness
add_subdirectory(libs/phosphor-service-notifications) # org.freedesktop.Notifications server
add_subdirectory(libs/phosphor-service-polkit)        # PolicyKit authentication agent
add_subdirectory(libs/phosphor-service-clipboard)     # Wayland clipboard history (data-control)
add_subdirectory(libs/phosphor-service-lock)          # PAM auth + ext-session-lock-v1 coordination
add_subdirectory(libs/phosphor-service-session)       # logind session/power actions + inhibitors
add_subdirectory(libs/phosphor-service-nightlight)    # night light over wlr-gamma-control (compositor-neutral)

# Example demos and CLI acceptance harnesses are not part of the installed
# product, and no test uses them. They are built only when asked for, so CI
# and the apps that add this tree never build them. The Nix flake builds from
# a source tree with examples/ stripped (flake.nix removes it from the
# fileset), so the directory must also exist.
option(PHOSPHOR_SHELL_LIBS_BUILD_EXAMPLES "Build the demos and acceptance harnesses in examples/" OFF)
if(PHOSPHOR_SHELL_LIBS_BUILD_EXAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples")
# Phosphor SDK groundwork demos:
#   phosphor-theme-demo  — swatch-sheet viewer, hot-reloads the PaletteStore.
#   phosphor-theme-cli   — headless wallpaper → matugen → palette → template.
#   phosphor-popout-demo — popout arbitration harness (popout + theme).
#   phosphor-registry-demo — in-process IBarWidgetFactory registration
#                            (registry + theme).
#   phosphor-ipc-demo    — three IpcTargets driven via phosphorctl.
#   phosphor-widgets-kitchen-sink — every Phosphor.Widgets atom in its
#                            enabled/disabled states (widgets + theme).
#   phosphor-osd-demo — OSDHost overlay; the four built-in OSDs via
#                            Registry<IOSDFactory>, driven by phosphorctl.
#   phosphor-toast-demo — ToastHost overlay fed by a real NotificationServer
#                            (notify-send) + buttons; DND exercises the rules seam.
add_subdirectory(examples/phosphor-theme-demo)
add_subdirectory(examples/phosphor-theme-cli)
add_subdirectory(examples/phosphor-popout-demo)
add_subdirectory(examples/phosphor-registry-demo)
add_subdirectory(examples/phosphor-ipc-demo)
add_subdirectory(examples/phosphor-widgets-kitchen-sink)
add_subdirectory(examples/phosphor-osd-demo)
add_subdirectory(examples/phosphor-toast-demo)
add_subdirectory(examples/phosphor-control-center-demo)
add_subdirectory(examples/phosphor-launcher-demo)
add_subdirectory(examples/phosphor-authentication-preview)

# phosphor-perscreen-demo. Acceptance harness for the Phosphor.Shell
# PerScreen helper. Opens a small window per monitor;
# hot-plug a display → window appears. Links PhosphorShell::PhosphorShellQml
# (PerScreen lives in libs/phosphor-shell).
add_subdirectory(examples/phosphor-perscreen-demo)

# phosphor-service-pipewire-cli. Acceptance harness for the
# phosphor-service-pipewire library. Headless driver
# exercising the read paths (list sinks/sources/streams, default),
# write paths (set-volume, mute/unmute), and metadata writes
# (set-default-sink/source) against a live PipeWire + WirePlumber
# daemon.
add_subdirectory(examples/phosphor-service-pipewire-cli)

# phosphor-service-network-cli. Acceptance harness for the
# phosphor-service-network library. Headless driver
# exercising the read paths (status, list-devices, list-connections,
# list-aps) and write paths (scan, connect) against a live
# org.freedesktop.NetworkManager.
add_subdirectory(examples/phosphor-service-network-cli)

# phosphor-service-bluetooth-cli. Acceptance harness for the
# phosphor-service-bluetooth library. Headless driver exercising
# the read paths (status, list-adapters, list-devices) and write paths
# (power, scan, pair, connect, trust, remove) against a live org.bluez, with
# an interactive pairing agent.
add_subdirectory(examples/phosphor-service-bluetooth-cli)

# phosphor-service-brightness-cli. Acceptance harness for the
# phosphor-service-brightness library. Headless driver exercising
# the read path (list, get) and write path (set) against real sysfs backlights
# and logind.
add_subdirectory(examples/phosphor-service-brightness-cli)

# phosphor-service-notifications-cli. Acceptance harness for the
# phosphor-service-notifications library. Both a server (watch: own
# org.freedesktop.Notifications and log incoming Notify + close events) and a
# client (send / close / info via D-Bus).
add_subdirectory(examples/phosphor-service-notifications-cli)

# phosphor-service-polkit-cli. Acceptance harness for the phosphor-service-polkit
# library. Runs as the session's PolicyKit authentication agent and
# answers PAM prompts (trigger with `pkexec true`).
add_subdirectory(examples/phosphor-service-polkit-cli)

# phosphor-service-idle-cli. Acceptance harness for the phosphor-service-idle
# library. Reports idle support, logs each idle stage as it fires
# with a wall-clock timestamp, and can hold an inhibition for a fixed duration
# (--inhibit-for).
add_subdirectory(examples/phosphor-service-idle-cli)

# phosphor-service-clipboard-cli. Acceptance harness for the
# phosphor-service-clipboard library. Watches the clipboard, lists
# the history, and copies an entry back.
add_subdirectory(examples/phosphor-service-clipboard-cli)

# phosphor-service-lock-cli. Acceptance harness for the phosphor-service-lock
# library. Authenticates the user (PAM) and drives the session lock.
add_subdirectory(examples/phosphor-service-lock-cli)

# phosphor-service-session-cli. Acceptance harness for the
# phosphor-service-session library. Drives the logind session
# capabilities + actions (status / lock / logout / suspend / ... / power-off).
add_subdirectory(examples/phosphor-service-session-cli)

# phosphor-service-nightlight-cli. Acceptance harness for the
# phosphor-service-nightlight library. Holds a colour temperature on every output
# of the running compositor through wlr-gamma-control for a few seconds, then
# fades back.
add_subdirectory(examples/phosphor-service-nightlight-cli)

# phosphor-registry-plugin-demo. Sibling of phosphor-registry-demo
# that exercises PluginLoader and the live reload of added or removed
# plugins. The third (CPU-meter) widget builds as a separate bar-widget
# Qt plugin and is loaded by PluginLoader from the build tree's plugin
# root, which it searches ahead of the standard plugin paths. Links
# PhosphorTheme.
add_subdirectory(examples/phosphor-registry-plugin-demo)
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Top-level finish
# ═══════════════════════════════════════════════════════════════════════════════

if(PHOSPHOR_SHELL_LIBS_TOP_LEVEL)
    # One precompiled header shared by every test target, phosphor-libs' as
    # well. An app that adds this tree makes the same call itself.
    if(BUILD_TESTING AND ENABLE_PCH)
        include(PhosphorTestPch)
        phosphor_share_test_pch()
    endif()

    feature_summary(WHAT REQUIRED_PACKAGES_FOUND REQUIRED_PACKAGES_NOT_FOUND
                         OPTIONAL_PACKAGES_FOUND OPTIONAL_PACKAGES_NOT_FOUND
                    FATAL_ON_MISSING_REQUIRED_PACKAGES)
endif()
