# SPDX-FileCopyrightText: 2026 Phosphor Works Team
# SPDX-License-Identifier: LGPL-2.1-or-later

find_package(Qt6 6.6 REQUIRED COMPONENTS Test)

# Shared test isolation: a private D-Bus session (so a test that touches D-Bus
# cannot activate the INSTALLED daemon, which then holds the test's stdout pipe
# open and hangs ctest after the test itself passed) plus a per-target XDG
# sandbox (so nothing writes into the developer's real ~/.config/plasmazones).
# The module lives in phosphor-libs, whose top-level CMakeLists names its cmake/
# directory PHOSPHOR_LIBS_CMAKE_DIR. The guard covers a configure of this
# library on its own, where nothing has set it.
if(PHOSPHOR_LIBS_CMAKE_DIR AND EXISTS "${PHOSPHOR_LIBS_CMAKE_DIR}/PhosphorTestIsolation.cmake")
    include(${PHOSPHOR_LIBS_CMAKE_DIR}/PhosphorTestIsolation.cmake)
else()
    # Stand-ins so the call sites below need no guard of their own.
    # phosphor_append_test_environment is reproduced EXACTLY, because it is
    # self-contained and because callers use it for things like
    # QT_QPA_PLATFORM=offscreen that the tests need to pass at all — a no-op
    # there would break them rather than merely un-isolate them. The isolation
    # helper degrades to nothing: its private D-Bus session and XDG sandbox need
    # module-level state that does not exist without the module.
    function(phosphor_apply_test_isolation _target)
    endfunction()
    function(phosphor_append_test_environment _test_name)
        set_property(TEST ${_test_name} APPEND PROPERTY ENVIRONMENT ${ARGN})
    endfunction()
endif()
function(_phosphorservicesni_test target source)
    qt_add_executable(${target} ${source})
    target_link_libraries(${target}
        PRIVATE
            Qt6::Test
            PhosphorServiceSni::PhosphorServiceSni
    )
    add_test(NAME ${target} COMMAND ${target})
    phosphor_apply_test_isolation(${target})
    # APPEND, never a bare SET: `set_tests_properties(... PROPERTIES
    # ENVIRONMENT ...)` REPLACES the whole list and would drop the XDG
    # sandbox `phosphor_apply_test_isolation` just installed.
    phosphor_append_test_environment(${target} "QT_QPA_PLATFORM=offscreen")
    set_tests_properties(${target} PROPERTIES
            LABELS "phosphorservicesni"
    )
endfunction()

# Smoke test: role plumbing on StatusNotifierItemModel.
_phosphorservicesni_test(test_phosphorservicesni_smoke test_smoke.cpp)

# StatusNotifierItemModel lifecycle (host attach/detach, role names
# as public contract, invalid-row no-ops).
_phosphorservicesni_test(test_phosphorservicesni_itemmodel test_itemmodel.cpp)

_phosphorservicesni_test(test_phosphorservicesni_trayitems test_trayitems.cpp)
_phosphorservicesni_test(test_phosphorservicesni_watcherlifecycle test_watcherlifecycle.cpp)

# DBusMenuModel without a real DBus source: just contract + no-op
# invariants. Live-source coverage requires a fake session bus + a
# fixture menu app, which is its own follow-up.
_phosphorservicesni_test(test_phosphorservicesni_dbusmenumodel test_dbusmenumodel.cpp)
