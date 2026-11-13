{
  description = "Sweetlock screen locker for Wayland";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs, ... }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});

      # NixOS needs `security.pam.services.sweetlock = {};` beside the package
      package =
        {
          lib,
          stdenv,
          meson,
          ninja,
          pkg-config,
          wayland-scanner,
          versionCheckHook,
          wayland,
          wayland-protocols,
          libxkbcommon,
          linux-pam,
          cairo,
          pango,
          libjpeg,
        }:
        stdenv.mkDerivation {
          pname = "sweetlock";
          version = builtins.head (
            builtins.match ".*version: '([0-9.]+)'.*" (builtins.readFile ./meson.build)
          );

          # only what the build needs, so the private test/ never enters it
          src = lib.fileset.toSource {
            root = ./.;
            fileset = lib.fileset.unions [
              ./meson.build
              ./meson.options
              ./src
              ./protocol
              ./pam
              ./third_party
            ];
          };

          strictDeps = true;
          depsBuildBuild = [ pkg-config ];
          nativeBuildInputs = [
            meson
            ninja
            pkg-config
            wayland-scanner
          ];
          buildInputs = [
            wayland
            wayland-protocols
            libxkbcommon
            linux-pam
            cairo
            pango
            libjpeg
          ];

          nativeInstallCheckInputs = [ versionCheckHook ];
          doInstallCheck = true;

          meta = {
            description = "Screen locker for Wayland";
            homepage = "https://github.com/naurissteins/sweetlock";
            license = lib.licenses.gpl3Plus;
            mainProgram = "sweetlock";
            platforms = lib.platforms.linux;
          };
        };
    in
    {
      packages = forAllSystems (pkgs: {
        sweetlock = pkgs.callPackage package { };
        default = self.packages.${pkgs.stdenv.hostPlatform.system}.sweetlock;
      });

      overlays.default = final: _prev: {
        sweetlock = final.callPackage package { };
      };

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          packages = with pkgs; [
            meson
            ninja
            pkg-config
            cairo
            pango
            wayland
            wayland-protocols
            wayland-scanner
            libxkbcommon
            linux-pam
            libjpeg
            # Newest LLVM, to match the rolling Arch CI image. clang-tools
            # comes first so its wrapped clang-tidy finds the libc headers
            llvmPackages_latest.clang-tools
            llvmPackages_latest.clang
            valgrind
            shellcheck
            actionlint
          ];

          # The debug gates use -Werror, and fortify warns without optimization
          hardeningDisable = [ "fortify" ];

          # clang's setup hook claims CC; GCC stays the default, CC=clang opts in
          shellHook = ''
            export CC=gcc
          '';
        };
      });

      formatter = forAllSystems (pkgs: pkgs.nixfmt);
    };
}
