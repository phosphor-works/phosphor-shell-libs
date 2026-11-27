<!-- SPDX-FileCopyrightText: 2026 Phosphor Works Team -->
<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->

# Changelog

This file records each release of phosphor-shell-libs, the desktop shell service and surface libraries.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html), as the phosphor-works conventions say.

## [Unreleased]

### Added

- An app can embed the libraries (phosphor-libs' `PHOSPHOR_EMBED_APP`): the install then holds only the shared
  libraries that the app's programs link, in the app's own directory. Every install rule goes through the functions of
  phosphor-libs' `cmake/PhosphorSuite.cmake`.

### Changed

- The clipboard history service watches the clipboard through `ext-data-control-v1` when the compositor has it
  (phosphor-libs `bcad89a2`), and through `wlr-data-control` only when it does not. On KWin, which has only the ext
  protocol, clipboard history now works.

## [0.1.0] - 2026-11-26

The first release. It has 28 libraries: `phosphor-popout`, `phosphor-service-bluetooth`, `phosphor-service-brightness`, `phosphor-service-clipboard`, `phosphor-service-icontheme`, `phosphor-service-lock`, `phosphor-service-mpris`, `phosphor-service-network`, `phosphor-service-nightlight`, `phosphor-service-notifications`, `phosphor-service-pipewire`, `phosphor-service-polkit`, `phosphor-service-session`, `phosphor-service-sni`, `phosphor-service-upower`, `phosphor-shell-bar`, `phosphor-shell-control-center`, `phosphor-shell-dashboard`, `phosphor-shell-launcher`, `phosphor-shell-lock`, `phosphor-shell-notifications`, `phosphor-shell-osd`, `phosphor-shell-picker`, `phosphor-shell-polkit`, `phosphor-shell-power`, `phosphor-shell-widgets`, `phosphor-shell`, `phosphor-theme`. The tag `v0.1.0` is commit `5bfa3764`, which Lambent pins.

[Unreleased]: https://github.com/phosphor-works/phosphor-shell-libs/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/phosphor-works/phosphor-shell-libs/releases/tag/v0.1.0
