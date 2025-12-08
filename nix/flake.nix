{
  description = "Paravision development shell";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs {
        inherit system;
      };
    in
    {
      devShells.${system}.default = pkgs.mkShell.override {
        stdenv = pkgs.overrideCC pkgs.gcc14Stdenv pkgs.llvmPackages_20.clang;
      } {
        name = "pollcoro-shell";

        buildInputs = with pkgs; [
          git
          bash
          cmake
          ninja
          pkg-config
          gdb
          cmake-format
          valgrind
          llvmPackages_20.clang-tools
          just
        ];

        shellHook = ''
          set -eo pipefail

          export PATH="${pkgs.llvmPackages_20.clang-tools}/bin:$PATH"
        '';
      };
    };
}
