{
  description = "Sweetlock development environment";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { nixpkgs, ... }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
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
