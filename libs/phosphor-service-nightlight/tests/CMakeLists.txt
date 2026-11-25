# SPDX-FileCopyrightText: 2026 Phosphor Works Team
# SPDX-License-Identifier: LGPL-2.1-or-later

find_package(Qt6 6.6 REQUIRED COMPONENTS Test)

# Shared test isolation from phosphor-libs (a private D-Bus session and a
# per-target XDG sandbox). The guard covers a configure of this library on its
# own, where nothing has set PHOSPHOR_LIBS_CMAKE_DIR.
if(PHOSPHOR_LIBS_CMAKE_DIR AND EXISTS "${PHOSPHOR_LIBS_CMAKE_DIR}/PhosphorTestIsolation.cmake")
    include(${PHOSPHOR_LIBS_CMAKE_DIR}/PhosphorTestIsolation.cmake)
else()
    function(phosphor_apply_test_isolation _target)
    endfunction()
    function(phosphor_append_test_environment _test_name)
        set_property(TEST ${_test_name} APPEND PROPERTY ENVIRONMENT ${ARGN})
    endfunction()
endif()

function(_phosphorservicenightlight_register target)
    add_test(NAME ${target} COMMAND ${target})
    phosphor_apply_test_isolation(${target})
    # APPEND, never a bare SET: set_tests_properties(... ENVIRONMENT ...)
    # replaces the list and would drop the sandbox installed above.
    phosphor_append_test_environment(${target} "QT_QPA_PLATFORM=offscreen")
    set_tests_properties(${target} PROPERTIES LABELS "phosphorservicenightlight")
endfunction()

# Colour temperature to white point and gamma table (the exported math).
add_executable(test_phosphorservicenightlight_colortemperature test_colortemperature.cpp)
target_link_libraries(test_phosphorservicenightlight_colortemperature
    PRIVATE Qt6::Test PhosphorServiceNightLight::PhosphorServiceNightLight
)
_phosphorservicenightlight_register(test_phosphorservicenightlight_colortemperature)

# Schedule evaluation and the sun's position at injected moments. Compiles the
# internal sources directly, so they stay unexported.
add_executable(test_phosphorservicenightlight_schedule
    test_schedule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/schedule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/solarposition.cpp
)
target_include_directories(test_phosphorservicenightlight_schedule
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(test_phosphorservicenightlight_schedule PRIVATE Qt6::Test)
_phosphorservicenightlight_register(test_phosphorservicenightlight_schedule)

# NightLight's state transitions over a fake GammaBackend and an injected clock,
# plus the production object's inert construction without a Wayland session.
add_executable(test_phosphorservicenightlight_nightlight test_nightlight.cpp)
target_link_libraries(test_phosphorservicenightlight_nightlight
    PRIVATE Qt6::Test PhosphorServiceNightLight::PhosphorServiceNightLight
)
_phosphorservicenightlight_register(test_phosphorservicenightlight_nightlight)
