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
      devShells.${system}.default = pkgs.mkShell {
        name = "pollcoro-shell";

        buildInputs = with pkgs; [
          git
          stdenv
          bash
          gcc
          cmake
          ninja
          pkg-config
          gdb
          cmake-format
          valgrind
          llvmPackages_20.clang-tools
          just
          coreutils
        ];

        shellHook = ''
          export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:${pkgs.stdenv.cc.cc.lib}/lib"
        '';
      };
    };
}
