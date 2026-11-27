# SPDX-FileCopyrightText: 2026 Phosphor Works Team
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# The phosphor-shell-libs package: every shell library, its headers and its
# CMake package files, built with the phosphor-libs commit
# cmake/PhosphorShellLibsDependencies.cmake pins, whose libraries the package
# includes. `withTests` builds and runs the test suite (phosphor-libs'
# included) in the sandbox, which is what the flake's `checks.<system>.tests`
# does.
{
  lib,
  stdenv,
  fetchurl,
  linkFarm,
  cmake,
  ninja,
  pkg-config,
  qt6,
  kdePackages,
  wayland,
  wayland-scanner,
  libxkbcommon,
  vulkan-headers,
  dbus,
  pipewire,
  pam,
  ddcutil,
  python3,
  glib,
  gobject-introspection,
  makeFontsConf,
  dejavu_fonts,
  src,
  version,
  withTests ? false,
}:

let
  # The pinned phosphor-libs commit (cmake/PhosphorShellLibsDependencies.cmake)
  # and the Luau and jsoncons archives that commit pins in its
  # cmake/PhosphorDependencies.cmake, with its test data (the surface packs)
  # when the tests are built. The sandbox has no network, so they are
  # fetched here as fixed-output derivations and handed to CMake as one
  # directory (PHOSPHOR_DEPS_ARCHIVES), each under the FILE name its pin gives
  # it. CMake checks each against the pin's hash again before unpacking it.
  # fetchurl, not fetchFromGitHub: the pin hashes the archive file itself.
  depsArchives = linkFarm "phosphor-shell-libs-deps-archives" (
    [
      {
        name = "phosphor-libs-8373b436c6454b4d73e28c63bffcd0f87f059c2b.tar.gz";
        path = fetchurl {
          url = "https://github.com/phosphor-works/phosphor-libs/archive/8373b436c6454b4d73e28c63bffcd0f87f059c2b.tar.gz";
          # sha256 986fde5e4895ff596e2e850a4f9113e8418bc97020b20222805b1ccda6706c47
          hash = "sha256-mG/eXkiV/1luLoUKT5ET6EGLyXAgsgIigFsczaZwbEc=";
        };
      }
      {
        name = "luau-0.745.tar.gz";
        path = fetchurl {
          url = "https://github.com/luau-lang/luau/archive/refs/tags/0.745.tar.gz";
          # sha256 53c22773bace6cabbabaead576fbc08d11a0317fce11a3dbf9d027dda6849248
          hash = "sha256-U8Inc7rObKu6uurVdvvAjRGgMX/OEaPb+dAn3aaEkkg=";
        };
      }
      {
        name = "jsoncons-1.10.0.tar.gz";
        path = fetchurl {
          url = "https://github.com/danielaparker/jsoncons/archive/refs/tags/v1.10.0.tar.gz";
          # sha256 c829350a2eece662beb378143e2ab1c1cbb07d703687bf168626665a8a7d8bc1
          hash = "sha256-yCk1Ci7s5mK+s3gUPiqxwcuwfXA2h78WhiZmWop9i8E=";
        };
      }
    ]
    ++ lib.optionals withTests [
      {
        name = "phosphor-surface-packs-572e2240ef306d48650ed45ba6672af5ab31e75f.tar.gz";
        path = fetchurl {
          url = "https://github.com/phosphor-works/phosphor-surface-packs/archive/572e2240ef306d48650ed45ba6672af5ab31e75f.tar.gz";
          # sha256 2fb063f95503dcca9b23c2905213940f1f4aa8701623f1020c695335dd90475e
          hash = "sha256-L7Bj+VUD3MqbI8KQUhOUDx9KqHAWI/ECDGlTNd2QR14=";
        };
      }
    ]
  );
  trayFixturePython = python3.withPackages (ps: [
    ps.dbus-python
    ps.pygobject3
  ]);
in
stdenv.mkDerivation {
  pname = if withTests then "phosphor-shell-libs-tests" else "phosphor-shell-libs";
  inherit version src;

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
    kdePackages.extra-cmake-modules
    wayland-scanner
    # Its environment hook collects every Qt input's plugin and QML directories
    # (QtQuick, Controls, Kirigami, each in its own store path) into
    # qtWrapperArgs, which preCheck turns into the test environment.
    qt6.wrapQtAppsHook
  ];

  # Libraries and their QML modules, no applications to wrap.
  dontWrapQtApps = true;

  buildInputs = [
    qt6.qtbase
    qt6.qtdeclarative
    qt6.qtshadertools
    qt6.qtsvg
    qt6.qtwayland
    wayland
    # Qt declares QVulkanInstance only with the Vulkan headers present, and
    # phosphor-surfaces uses it when Qt has Vulkan, as nixpkgs' Qt does.
    vulkan-headers
    # The shell libraries draw their icons through Kirigami.
    kdePackages.kirigami
    # phosphor-shell-lock reads the keyboard layout and Caps Lock from the
    # seat's keymap.
    libxkbcommon
    # The service libraries' system interfaces: PipeWire, polkit-qt6, PAM, and
    # ddcutil for phosphor-service-brightness's optional external-monitor
    # backend.
    pipewire
    kdePackages.polkit-qt-1
    pam
    ddcutil
    # phosphor-libs' optional lookups, so it builds the code they switch on.
    kdePackages.kglobalaccel
    kdePackages.plasma-activities
  ];

  # dbus-run-session gives each test a private session bus
  # (cmake/PhosphorTestIsolation.cmake). The tray tests run a fixture of real
  # StatusNotifierItem apps (libs/phosphor-shell-bar/tests/tray-fixture.py)
  # through a Python with dbus-python and PyGObject, which reaches GLib
  # through gobject-introspection.
  nativeCheckInputs = [
    dbus
    trayFixturePython
    gobject-introspection
  ];
  checkInputs = [ glib ];

  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
    "-DBUILD_TESTING=${if withTests then "ON" else "OFF"}"
    "-DPHOSPHOR_DEPS_ARCHIVES=${depsArchives}"
    "-DKDE_INSTALL_QTPLUGINDIR=${placeholder "out"}/${qt6.qtbase.qtPluginPrefix}"
    "-DKDE_INSTALL_QMLDIR=${placeholder "out"}/${qt6.qtbase.qtQmlPrefix}"
  ]
  # The interpreter the tray tests launch their fixture with, named so CMake
  # cannot pick another Python on PATH that lacks the modules.
  ++ lib.optional withTests "-DPython3_EXECUTABLE=${trayFixturePython.interpreter}";

  doCheck = withTests;
  # The tests run uninstalled, from the build tree, so nothing wraps them.
  # nixpkgs' Qt finds QML modules through NIXPKGS_QT6_QML_IMPORT_PATH and
  # plugins through QT_PLUGIN_PATH, and wrapQtAppsHook puts both only into the
  # wrapper arguments of installed programs. Export the same entries here, so
  # the tests that load QtQuick, Controls or Kirigami find them. Then the
  # offscreen platform for the GUI tests, and a writable HOME for the ones the
  # XDG sandbox does not cover.
  preCheck = ''
    for ((i = 0; i + 3 < ''${#qtWrapperArgs[@]}; i++)); do
      if [[ "''${qtWrapperArgs[i]}" == --prefix ]]; then
        case "''${qtWrapperArgs[i + 1]}" in
          NIXPKGS_QT6_QML_IMPORT_PATH | QT_PLUGIN_PATH)
            addToSearchPath "''${qtWrapperArgs[i + 1]}" "''${qtWrapperArgs[i + 3]}"
            ;;
        esac
      fi
    done
    export NIXPKGS_QT6_QML_IMPORT_PATH QT_PLUGIN_PATH
    # GLib's typelibs, for the tray fixture's `from gi.repository import GLib`.
    addToSearchPath GI_TYPELIB_PATH "${glib.out}/lib/girepository-1.0"
    export GI_TYPELIB_PATH
    # One real font family, so the theme's font tests resolve an installed face.
    export FONTCONFIG_FILE=${makeFontsConf { fontDirectories = [ dejavu_fonts ]; }}
    export QT_QPA_PLATFORM=offscreen
    export HOME="$(mktemp -d)"
    export CTEST_OUTPUT_ON_FAILURE=1
  '';

  meta = {
    description = "LGPL Qt 6 libraries for building a Wayland desktop shell";
    homepage = "https://github.com/phosphor-works/phosphor-shell-libs";
    license = with lib.licenses; [
      lgpl21Plus
      mit
      boost
      # The vendored wlr-gamma-control protocol of phosphor-service-nightlight.
      hpndSellVariant
    ];
    platforms = [
      "x86_64-linux"
      "aarch64-linux"
    ];
  };
}
