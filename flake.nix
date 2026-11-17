{
  description = "LGPL Qt 6 libraries for building a Wayland desktop shell";

  # The same nixpkgs as PlasmaZones' flake.lock, so both build against one Qt.
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    { self, nixpkgs }:
    let
      inherit (nixpkgs) lib;
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f: lib.genAttrs supportedSystems (system: f nixpkgs.legacyPackages.${system});

      # project(PhosphorShellLibs VERSION x.y.z) in CMakeLists.txt is the one
      # source of the version.
      version =
        let
          m = builtins.match ".*project\\(PhosphorShellLibs VERSION ([0-9.]+).*" (builtins.readFile ./CMakeLists.txt);
        in
        if m == null then throw "no project(PhosphorShellLibs VERSION ...) in CMakeLists.txt" else builtins.head m;

      package =
        pkgs: withTests:
        pkgs.callPackage ./nix/package.nix {
          inherit version withTests;
          src = self;
        };
    in
    {
      packages = forAllSystems (pkgs: {
        phosphor-shell-libs = package pkgs false;
        default = package pkgs false;
      });

      # `nix flake check` builds every library, phosphor-libs' included, and
      # runs the whole test suite.
      checks = forAllSystems (pkgs: {
        tests = package pkgs true;
      });

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          inputsFrom = [ (package pkgs true) ];
          packages = [
            pkgs.ccache
            pkgs.clang-tools
          ];
        };
      });

      formatter = forAllSystems (pkgs: pkgs.nixfmt-rfc-style);
    };
}
