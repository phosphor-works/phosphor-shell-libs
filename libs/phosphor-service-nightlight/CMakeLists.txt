# SPDX-FileCopyrightText: 2026 fuddlesworth
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# PhosphorServiceNightLight: a compositor-neutral night light for Phosphor-based
# desktop shells. It warms the screen by setting each output's gamma ramps
# through wlr-gamma-control-unstable-v1, follows a constant, fixed-hours or
# sunrise/sunset schedule, fades between temperatures and supports temporary
# inhibition. The app owns persistence: the library takes its settings through
# setters and reports its state through properties and signals.

cmake_minimum_required(VERSION 3.16)

set(PHOSPHORSERVICENIGHTLIGHT_VERSION "0.1.0")

# The Qt floor this library requires AND advertises in its generated
# PhosphorServiceNightLightConfig.cmake. Taken from the top-level QT_MIN_VERSION
# when built in-tree so the two cannot drift, with its own value for a
# STANDALONE build, where that variable does not exist.
if(DEFINED QT_MIN_VERSION)
    set(PHOSPHORSERVICENIGHTLIGHT_QT_MIN ${QT_MIN_VERSION})
else()
    set(PHOSPHORSERVICENIGHTLIGHT_QT_MIN "6.10.0")
endif()

if(NOT PROJECT_VERSION)
    project(PhosphorServiceNightLight VERSION ${PHOSPHORSERVICENIGHTLIGHT_VERSION} LANGUAGES C CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_AUTOMOC ON)
endif()

include(GenerateExportHeader)

# ═══════════════════════════════════════════════════════════════════════════════
# Dependencies
# ═══════════════════════════════════════════════════════════════════════════════
#
# Core / Qml: the QObject + QML surface. Gui reaches the session's wl_display
# through the public QNativeInterface::QWaylandApplication, so no Qt private
# module is needed. libwayland-client and wayland-scanner build the
# gamma-control client; both stay PRIVATE, no Wayland type appears in a public
# header.
find_package(Qt6 ${PHOSPHORSERVICENIGHTLIGHT_QT_MIN} REQUIRED COMPONENTS Core Gui Qml)
find_package(Wayland REQUIRED COMPONENTS Client)
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ═══════════════════════════════════════════════════════════════════════════════
# Wayland protocol code generation
# ═══════════════════════════════════════════════════════════════════════════════
#
# protocols/wlr-gamma-control-unstable-v1.xml is vendored unchanged from
# wlr-protocols (https://gitlab.freedesktop.org/wlroots/wlr-protocols),
# unstable/wlr-gamma-control-unstable-v1.xml at commit
# 1106a4a1c57562ddd438c547da4c0d6dc150d624. It keeps its own copyright and
# licence text (HPND-sell-variant), which the generated code carries too.

set(_nl_proto_dir ${CMAKE_CURRENT_SOURCE_DIR}/protocols)
set(_nl_gen_dir ${CMAKE_CURRENT_BINARY_DIR}/generated-protocols)
file(MAKE_DIRECTORY ${_nl_gen_dir})

add_custom_command(
    OUTPUT ${_nl_gen_dir}/wlr-gamma-control-unstable-v1-client-protocol.h
           ${_nl_gen_dir}/wlr-gamma-control-unstable-v1-protocol.c
    COMMAND ${WAYLAND_SCANNER} client-header
            ${_nl_proto_dir}/wlr-gamma-control-unstable-v1.xml
            ${_nl_gen_dir}/wlr-gamma-control-unstable-v1-client-protocol.h
    COMMAND ${WAYLAND_SCANNER} private-code
            ${_nl_proto_dir}/wlr-gamma-control-unstable-v1.xml
            ${_nl_gen_dir}/wlr-gamma-control-unstable-v1-protocol.c
    DEPENDS ${_nl_proto_dir}/wlr-gamma-control-unstable-v1.xml
    COMMENT "PhosphorServiceNightLight: generating wlr-gamma-control protocol code"
)

# Generated C code: skip PCH and unity build
set_source_files_properties(
    ${_nl_gen_dir}/wlr-gamma-control-unstable-v1-protocol.c
    PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
        SKIP_UNITY_BUILD_INCLUSION ON
)

# ═══════════════════════════════════════════════════════════════════════════════
# Library
# ═══════════════════════════════════════════════════════════════════════════════

set(phosphorservicenightlight_public_HDRS
    include/PhosphorServiceNightLight/PhosphorServiceNightLight.h
    include/PhosphorServiceNightLight/QmlRegistration.h
    include/PhosphorServiceNightLight/ColorTemperature.h
    include/PhosphorServiceNightLight/GammaBackend.h
    include/PhosphorServiceNightLight/NightLight.h
)

set(phosphorservicenightlight_SRCS
    src/qmlregistration.cpp
    src/colortemperature.cpp
    src/gammabackend.cpp
    src/schedule.cpp
    src/schedule.h
    src/solarposition.cpp
    src/solarposition.h
    src/nightlight.cpp
    src/waylandgammabackend.cpp
    src/waylandgammabackend.h
    ${_nl_gen_dir}/wlr-gamma-control-unstable-v1-protocol.c
    ${_nl_gen_dir}/wlr-gamma-control-unstable-v1-client-protocol.h
)

add_library(PhosphorServiceNightLight SHARED
    ${phosphorservicenightlight_public_HDRS}
    ${phosphorservicenightlight_SRCS}
)

add_library(PhosphorServiceNightLight::PhosphorServiceNightLight ALIAS PhosphorServiceNightLight)

generate_export_header(PhosphorServiceNightLight
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/PhosphorServiceNightLight/phosphorservicenightlight_export.h
    EXPORT_MACRO_NAME PHOSPHORSERVICENIGHTLIGHT_EXPORT
)

if(NOT DEFINED KDE_INSTALL_INCLUDEDIR)
    include(GNUInstallDirs)
    set(KDE_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR})
endif()

if(NOT DEFINED KDE_INSTALL_LIBDIR)
    include(GNUInstallDirs)
    set(KDE_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR})
endif()

if(NOT DEFINED KDE_INSTALL_BINDIR)
    include(GNUInstallDirs)
    set(KDE_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR})
endif()

target_include_directories(PhosphorServiceNightLight
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${_nl_gen_dir}
)

target_compile_features(PhosphorServiceNightLight PUBLIC cxx_std_20)

target_link_libraries(PhosphorServiceNightLight
    PUBLIC
        Qt6::Core
        Qt6::Qml
    PRIVATE
        Qt6::Gui
        Wayland::Client
)

set_target_properties(PhosphorServiceNightLight PROPERTIES
    VERSION ${PHOSPHORSERVICENIGHTLIGHT_VERSION}
    SOVERSION 0
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    UNITY_BUILD OFF
)

# ═══════════════════════════════════════════════════════════════════════════════
# Install
# ═══════════════════════════════════════════════════════════════════════════════

install(TARGETS PhosphorServiceNightLight
    EXPORT PhosphorServiceNightLightTargets
    RUNTIME DESTINATION ${KDE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${KDE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${KDE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${KDE_INSTALL_INCLUDEDIR}
)

install(FILES ${phosphorservicenightlight_public_HDRS}
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/PhosphorServiceNightLight
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/PhosphorServiceNightLight/phosphorservicenightlight_export.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/PhosphorServiceNightLight
)

install(EXPORT PhosphorServiceNightLightTargets
    FILE PhosphorServiceNightLightTargets.cmake
    NAMESPACE PhosphorServiceNightLight::
    DESTINATION ${KDE_INSTALL_LIBDIR}/cmake/PhosphorServiceNightLight
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/PhosphorServiceNightLightConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/PhosphorServiceNightLightConfig.cmake"
    INSTALL_DESTINATION ${KDE_INSTALL_LIBDIR}/cmake/PhosphorServiceNightLight
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/PhosphorServiceNightLightConfigVersion.cmake"
    VERSION ${PHOSPHORSERVICENIGHTLIGHT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/PhosphorServiceNightLightConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/PhosphorServiceNightLightConfigVersion.cmake"
    DESTINATION ${KDE_INSTALL_LIBDIR}/cmake/PhosphorServiceNightLight
)

# ═══════════════════════════════════════════════════════════════════════════════
# Tests
# ═══════════════════════════════════════════════════════════════════════════════

if(CMAKE_PROJECT_NAME STREQUAL "PhosphorServiceNightLight" OR BUILD_TESTING)
    enable_testing()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    endif()
endif()
