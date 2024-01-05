{
  description = "IDE for MiniZinc, a medium-level constraint modelling language";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        packages = {
          minizinc-ide = pkgs.callPackage ./default.nix { inherit self; inherit (pkgs.darwin.apple_sdk.frameworks) Cocoa; };
          default = self.packages.${system}.minizinc-ide;
        };

        devShells.default = pkgs.mkShell {
          name = "minizinc-ide";

          packages = with pkgs; [
            qt6.qtbase
            qt6.qtwebsockets
            qt6.qmake
            minizinc
          ] ++ lib.optionals stdenv.isDarwin [ darwin.apple_sdk.frameworks.Cocoa ];
        };
      }
    );
}
