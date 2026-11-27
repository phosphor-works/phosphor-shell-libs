# SPDX-FileCopyrightText: 2026 Phosphor Works Team
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# PhosphorServiceMpris: MPRIS2 (`org.mpris.MediaPlayer2.*`) media-player
# discovery and control. Pure D-Bus client; no UI.

cmake_minimum_required(VERSION 3.16)

set(PHOSPHORSERVICEMPRIS_VERSION "0.1.0")

# The Qt floor this library requires AND advertises in its generated
# PhosphorServiceMprisConfig.cmake. Taken from the top-level QT_MIN_VERSION when built in-tree so
# the two cannot drift, with its own value for a STANDALONE build, where that
# variable does not exist. The fallback is not optional: the Config template
# substitutes this, and an undefined variable expands to nothing and ships a config
# whose find_dependency(Qt6 ...) carries no version floor at all.
if(DEFINED QT_MIN_VERSION)
    set(PHOSPHORSERVICEMPRIS_QT_MIN ${QT_MIN_VERSION})
else()
    set(PHOSPHORSERVICEMPRIS_QT_MIN "6.10.0")
endif()

if(NOT PROJECT_VERSION)
    project(PhosphorServiceMpris VERSION ${PHOSPHORSERVICEMPRIS_VERSION} LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_AUTOMOC ON)
endif()

include(GenerateExportHeader)

find_package(Qt6 ${PHOSPHORSERVICEMPRIS_QT_MIN} REQUIRED COMPONENTS Core Qml DBus)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(phosphorservicempris_public_HDRS
    include/PhosphorServiceMpris/PhosphorServiceMpris.h
    include/PhosphorServiceMpris/QmlRegistration.h
    include/PhosphorServiceMpris/MprisHost.h
    include/PhosphorServiceMpris/MprisPlayer.h
    include/PhosphorServiceMpris/MprisPlayerModel.h
)

set(phosphorservicempris_SRCS
    src/qmlregistration.cpp
    src/mprishost.cpp
    src/mprisplayer.cpp
    src/mprisplayermodel.cpp
)

if(NOT COMMAND phosphor_install_targets)
    # Configured on its own: the install functions come with phosphor-libs.
    find_package(PhosphorPaths REQUIRED)
endif()
add_library(PhosphorServiceMpris SHARED
    ${phosphorservicempris_public_HDRS}
    ${phosphorservicempris_SRCS}
)

add_library(PhosphorServiceMpris::PhosphorServiceMpris ALIAS PhosphorServiceMpris)

generate_export_header(PhosphorServiceMpris
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/PhosphorServiceMpris/phosphorservicempris_export.h
    EXPORT_MACRO_NAME PHOSPHORSERVICEMPRIS_EXPORT
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

target_include_directories(PhosphorServiceMpris
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}>
)

target_compile_features(PhosphorServiceMpris PUBLIC cxx_std_20)

target_link_libraries(PhosphorServiceMpris
    PUBLIC
        Qt6::Core
        Qt6::Qml
        Qt6::DBus
)

set_target_properties(PhosphorServiceMpris PROPERTIES
    VERSION ${PHOSPHORSERVICEMPRIS_VERSION}
    SOVERSION 0
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    UNITY_BUILD OFF
)

phosphor_install_targets(TARGETS PhosphorServiceMpris EXPORT PhosphorServiceMprisTargets)

phosphor_install_devel(FILES ${phosphorservicempris_public_HDRS}
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/PhosphorServiceMpris
)
phosphor_install_devel(FILES ${CMAKE_CURRENT_BINARY_DIR}/PhosphorServiceMpris/phosphorservicempris_export.h
    DESTINATION ${KDE_INSTALL_INCLUDEDIR}/PhosphorServiceMpris
)

phosphor_install_devel(EXPORT PhosphorServiceMprisTargets
    FILE PhosphorServiceMprisTargets.cmake
    NAMESPACE PhosphorServiceMpris::
    DESTINATION ${KDE_INSTALL_LIBDIR}/cmake/PhosphorServiceMpris
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/PhosphorServiceMprisConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/PhosphorServiceMprisConfig.cmake"
    INSTALL_DESTINATION ${KDE_INSTALL_LIBDIR}/cmake/PhosphorServiceMpris
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/PhosphorServiceMprisConfigVersion.cmake"
    VERSION ${PHOSPHORSERVICEMPRIS_VERSION}
    COMPATIBILITY SameMajorVersion
)
phosphor_install_devel(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/PhosphorServiceMprisConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/PhosphorServiceMprisConfigVersion.cmake"
    DESTINATION ${KDE_INSTALL_LIBDIR}/cmake/PhosphorServiceMpris
)

if(CMAKE_PROJECT_NAME STREQUAL "PhosphorServiceMpris" OR PHOSPHOR_SHELL_LIBS_TESTS)
    enable_testing()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    endif()
endif()
