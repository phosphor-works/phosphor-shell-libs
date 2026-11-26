# SPDX-FileCopyrightText: 2026 Phosphor Works Team
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# What the shell libraries build from that is not in this repository: the
# phosphor-libs commit they build against, pinned to its archive by version,
# URL and SHA-256. nix/package.nix repeats the pin, so change both. The phosphor-libs
# tree pins its own third-party sources (Luau, jsoncons) in its
# cmake/PhosphorDependencies.cmake, at the commit pinned here.
#
# An app that adds this tree as a subproject (Lambent) has added
# phosphor-libs already, from its own pin, and phosphor_shell_libs_add_phosphor_libs()
# then uses that tree and fetches nothing. The app's pin wins.
#
# How phosphor-libs is obtained otherwise, in order:
#
#   1. PHOSPHOR_DEPS_ARCHIVES names a directory holding the archives under their
#      FILE names: this one and the library tree's. Each is checked against its
#      SHA-256 and extracted into the build tree. Nothing is downloaded and CPM
#      is never loaded.
#   2. PHOSPHOR_LIBS_GIT_REF names a branch, tag or commit. CPM clones
#      phosphor-libs at that ref instead of the pinned archive
#      (-DPHOSPHOR_LIBS_GIT_REF=main).
#   3. Otherwise CPM.cmake (pinned by hash) downloads the pinned archive once
#      into CPM_SOURCE_CACHE, .cache/cpm at this repository's root unless the
#      cache variable or the environment variable says otherwise.
#
# CPM's own override applies to 2 and 3: -DCPM_phosphor-libs_SOURCE=<dir> builds
# a local checkout, such as the sibling ../phosphor-libs, in place of either.
#
# Script mode, for packagers and CI:
#
#   cmake -DPHOSPHOR_DEPS_FETCH=<dir> [-DPHOSPHOR_DEPS_FETCH_TESTS=ON] -P cmake/PhosphorShellLibsDependencies.cmake
#
# downloads the phosphor-libs archive and every archive that tree pins into
# <dir>, so <dir> can be handed to PHOSPHOR_DEPS_ARCHIVES. With
# PHOSPHOR_DEPS_FETCH_TESTS it also downloads the archives only that tree's
# test suite reads, for an offline build that runs the tests.
#
# Bumping the pin: change its VERSION, URL, FILE and SHA256 together, with the
# hash taken from a download you checked.

include_guard(GLOBAL)

# ─── Pins ────────────────────────────────────────────────────────────────────
set(PHOSPHOR_SHELL_LIBS_DEPS phosphor-libs)

# The phosphor-libs commit (LGPL-2.1-or-later, phosphor-works/phosphor-libs)
# the shell libraries build against. A commit, not a release, so VERSION is the
# commit hash.
set(PHOSPHOR_SHELL_LIBS_DEP_phosphor-libs_VERSION "5f7ce79f4a1282d50f8936caa72dd7332545137c")
set(PHOSPHOR_SHELL_LIBS_DEP_phosphor-libs_URL "https://github.com/phosphor-works/phosphor-libs/archive/5f7ce79f4a1282d50f8936caa72dd7332545137c.tar.gz")
set(PHOSPHOR_SHELL_LIBS_DEP_phosphor-libs_FILE "phosphor-libs-5f7ce79f4a1282d50f8936caa72dd7332545137c.tar.gz")
set(PHOSPHOR_SHELL_LIBS_DEP_phosphor-libs_SHA256 "b44963ce30abfe91ff022d2d94f71bd9cc3c454c43660b94d4a75de72d933e0a")

# CPM.cmake itself, the same release phosphor-libs pins, so that tree uses
# this copy.
set(PHOSPHOR_CPM_VERSION "0.43.2")
set(PHOSPHOR_CPM_SHA256 "49a3bef91ceb65bb66d57255e12d1ffd22abc2f6408fa9fe4534c544a2f232aa")

get_filename_component(_PHOSPHOR_SHELL_LIBS_DEPS_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# _phosphor_shell_libs_extract(<archive> <dest> <stamp>): <archive> unpacked into
# <dest> with its single top-level directory removed. The stamp is written after
# the extraction finishes, so one cut short is redone next time.
function(_phosphor_shell_libs_extract archive dest stamp)
    if(EXISTS "${stamp}" AND IS_DIRECTORY "${dest}")
        return()
    endif()
    file(REMOVE_RECURSE "${dest}" "${dest}.tmp")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${dest}.tmp")
    file(GLOB _top LIST_DIRECTORIES true "${dest}.tmp/*")
    list(LENGTH _top _count)
    if(_count EQUAL 1 AND IS_DIRECTORY "${_top}")
        file(RENAME "${_top}" "${dest}")
        file(REMOVE_RECURSE "${dest}.tmp")
    else()
        file(RENAME "${dest}.tmp" "${dest}")
    endif()
    file(TOUCH "${stamp}")
endfunction()

# ─── Script mode: download every archive ─────────────────────────────────────
if(CMAKE_SCRIPT_MODE_FILE)
    if(NOT PHOSPHOR_DEPS_FETCH)
        message(FATAL_ERROR "usage: cmake -DPHOSPHOR_DEPS_FETCH=<dir> -P ${CMAKE_CURRENT_LIST_FILE}")
    endif()
    get_filename_component(_dir "${PHOSPHOR_DEPS_FETCH}" ABSOLUTE)
    file(MAKE_DIRECTORY "${_dir}")
    set(_name phosphor-libs)
    set(_file "${_dir}/${PHOSPHOR_SHELL_LIBS_DEP_${_name}_FILE}")
    set(_have "")
    if(EXISTS "${_file}")
        file(SHA256 "${_file}" _have)
    endif()
    if(NOT _have STREQUAL PHOSPHOR_SHELL_LIBS_DEP_${_name}_SHA256)
        message(STATUS "Downloading ${PHOSPHOR_SHELL_LIBS_DEP_${_name}_URL}")
        file(DOWNLOAD "${PHOSPHOR_SHELL_LIBS_DEP_${_name}_URL}" "${_file}.part"
             EXPECTED_HASH SHA256=${PHOSPHOR_SHELL_LIBS_DEP_${_name}_SHA256} STATUS _status)
        list(GET _status 0 _code)
        if(NOT _code EQUAL 0)
            file(REMOVE "${_file}.part")
            list(GET _status 1 _why)
            message(FATAL_ERROR "Could not download ${PHOSPHOR_SHELL_LIBS_DEP_${_name}_URL}: ${_why}")
        endif()
        file(RENAME "${_file}.part" "${_file}")
    endif()
    message(STATUS "${PHOSPHOR_SHELL_LIBS_DEP_${_name}_FILE}: ok")
    # The library tree's own pins, read from the pinned tree itself.
    set(_libs_tmp "${_dir}/.phosphor-libs-${PHOSPHOR_SHELL_LIBS_DEP_phosphor-libs_VERSION}")
    _phosphor_shell_libs_extract("${_file}" "${_libs_tmp}" "${_libs_tmp}.stamp")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DPHOSPHOR_DEPS_FETCH=${_dir}"
                "-DPHOSPHOR_DEPS_FETCH_TESTS=${PHOSPHOR_DEPS_FETCH_TESTS}"
                -P "${_libs_tmp}/cmake/PhosphorDependencies.cmake"
        RESULT_VARIABLE _rc)
    file(REMOVE_RECURSE "${_libs_tmp}" "${_libs_tmp}.stamp")
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "Fetching the archives phosphor-libs ${PHOSPHOR_SHELL_LIBS_DEP_phosphor-libs_VERSION} pins failed")
    endif()
    return()
endif()

# ─── Configure mode ──────────────────────────────────────────────────────────

# phosphor_shell_libs_add_phosphor_libs()
#
# Makes the phosphor-libs targets available, and leaves PHOSPHOR_LIBS_SOURCE_DIR
# naming the tree in use. A macro, so CPM loads and sets its variables in the
# caller's scope: call it from the top-level CMakeLists.
macro(phosphor_shell_libs_add_phosphor_libs)
    # Asked of a target, not of PHOSPHOR_LIBS_CMAKE_DIR: that is a cache entry,
    # so it outlives the configure that set it, and a reconfigure would take a
    # stale one for a tree that is not added this time.
    if(TARGET PhosphorIpc)
        # An app added phosphor-libs before this tree, from its own pin.
    elseif(PHOSPHOR_DEPS_ARCHIVES)
        set(_psl_archive "${PHOSPHOR_DEPS_ARCHIVES}/${PHOSPHOR_SHELL_LIBS_DEP_phosphor-libs_FILE}")
        if(NOT EXISTS "${_psl_archive}")
            message(FATAL_ERROR
                "PHOSPHOR_DEPS_ARCHIVES is ${PHOSPHOR_DEPS_ARCHIVES}, which has no ${PHOSPHOR_SHELL_LIBS_DEP_phosphor-libs_FILE}. "
                "Fill it with cmake -DPHOSPHOR_DEPS_FETCH=<dir> -P cmake/PhosphorShellLibsDependencies.cmake")
        endif()
        file(SHA256 "${_psl_archive}" _psl_have)
        if(NOT _psl_have STREQUAL PHOSPHOR_SHELL_LIBS_DEP_phosphor-libs_SHA256)
            message(FATAL_ERROR "${_psl_archive} does not match its pin:\n"
                                "  expected ${PHOSPHOR_SHELL_LIBS_DEP_phosphor-libs_SHA256}\n  got      ${_psl_have}")
        endif()
        set(_psl_root "${CMAKE_BINARY_DIR}/_phosphor_deps")
        set(_psl_dest "${_psl_root}/phosphor-libs-${PHOSPHOR_SHELL_LIBS_DEP_phosphor-libs_VERSION}")
        _phosphor_shell_libs_extract("${_psl_archive}" "${_psl_dest}"
            "${_psl_root}/.phosphor-libs-${PHOSPHOR_SHELL_LIBS_DEP_phosphor-libs_VERSION}-${_psl_have}")
        add_subdirectory("${_psl_dest}" "${CMAKE_BINARY_DIR}/_deps/phosphor-libs-build")
    else()
        if(NOT COMMAND CPMAddPackage)
            if(NOT DEFINED CPM_SOURCE_CACHE AND NOT DEFINED ENV{CPM_SOURCE_CACHE})
                set(CPM_SOURCE_CACHE "${_PHOSPHOR_SHELL_LIBS_DEPS_ROOT}/.cache/cpm" CACHE PATH
                    "Where CPM.cmake keeps downloaded packages, shared by every build directory")
            endif()
            if(DEFINED CPM_SOURCE_CACHE)
                set(_psl_cpm "${CPM_SOURCE_CACHE}/cpm/CPM_${PHOSPHOR_CPM_VERSION}.cmake")
            else()
                set(_psl_cpm "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${PHOSPHOR_CPM_VERSION}.cmake")
            endif()
            set(_psl_cpm_have "")
            if(EXISTS "${_psl_cpm}")
                file(SHA256 "${_psl_cpm}" _psl_cpm_have)
            endif()
            if(NOT _psl_cpm_have STREQUAL PHOSPHOR_CPM_SHA256)
                file(DOWNLOAD
                    "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${PHOSPHOR_CPM_VERSION}/CPM.cmake"
                    "${_psl_cpm}.part" EXPECTED_HASH SHA256=${PHOSPHOR_CPM_SHA256} STATUS _psl_cpm_status)
                list(GET _psl_cpm_status 0 _psl_cpm_code)
                if(NOT _psl_cpm_code EQUAL 0)
                    file(REMOVE "${_psl_cpm}.part")
                    message(FATAL_ERROR "Could not download CPM.cmake (${_psl_cpm_status}). Configure with network "
                                        "access once, or pass -DPHOSPHOR_DEPS_ARCHIVES=<dir>.")
                endif()
                file(RENAME "${_psl_cpm}.part" "${_psl_cpm}")
            endif()
            include("${_psl_cpm}")
        endif()
        set(PHOSPHOR_LIBS_GIT_REF "" CACHE STRING
            "Build phosphor-libs from this git branch, tag or commit instead of the pinned archive. Empty: the pin")
        if(PHOSPHOR_LIBS_GIT_REF)
            CPMAddPackage(NAME phosphor-libs
                          GIT_REPOSITORY "https://github.com/phosphor-works/phosphor-libs.git"
                          GIT_TAG "${PHOSPHOR_LIBS_GIT_REF}"
                          GIT_SHALLOW NO)
        else()
            CPMAddPackage(NAME phosphor-libs
                          URL "${PHOSPHOR_SHELL_LIBS_DEP_phosphor-libs_URL}"
                          URL_HASH SHA256=${PHOSPHOR_SHELL_LIBS_DEP_phosphor-libs_SHA256})
        endif()
    endif()
    if(NOT PHOSPHOR_LIBS_CMAKE_DIR)
        message(FATAL_ERROR "The phosphor-libs tree did not set PHOSPHOR_LIBS_CMAKE_DIR. It is older than the "
                            "commit cmake/PhosphorShellLibsDependencies.cmake pins.")
    endif()
    get_filename_component(PHOSPHOR_LIBS_SOURCE_DIR "${PHOSPHOR_LIBS_CMAKE_DIR}/.." ABSOLUTE)
endmacro()
