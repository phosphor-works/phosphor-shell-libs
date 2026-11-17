# SPDX-FileCopyrightText: 2026 fuddlesworth
# SPDX-License-Identifier: GPL-3.0-or-later
#
# phosphor-service-nightlight-cli: acceptance harness + worked example for the
# phosphor-service-nightlight library. Holds a colour temperature on every
# output of the running compositor for a few seconds through
# wlr-gamma-control, logging what the library reports, then fades back. See
# main.cpp for the arguments. Pinned to Qt6 6.6 so it builds stand-alone from
# this directory too.
find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui)

add_executable(phosphor-service-nightlight-cli
    main.cpp
)

target_link_libraries(phosphor-service-nightlight-cli
    PRIVATE
        Qt6::Core
        Qt6::Gui
        PhosphorServiceNightLight::PhosphorServiceNightLight
)

# Intentionally no `install(TARGETS ...)` rule. This CLI is the acceptance
# harness + worked example for phosphor-service-nightlight, not a user-facing
# tool.
