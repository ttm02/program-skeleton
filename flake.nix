# requires Nix package manager with Flake enabled
# see https://nixos.org/download/
# and https://wiki.nixos.org/wiki/Flakes
{
  inputs = {
    nixpkgs.url = "nixpkgs/nixos-24.11";
  };

  outputs = {
    self, nixpkgs,
    ...
  }:
  let
    pkgs = import nixpkgs { system = "x86_64-linux"; };
    inherit (pkgs) lib;

    LLVM_VER = "16";

    # symlink all `libclang_rt.*-x86_64.so` as libclang_rt.*.so
    compiler-rt-orig = pkgs."llvmPackages_${LLVM_VER}".compiler-rt;
    compiler-rt-lib = compiler-rt-orig + "/lib/linux";
    compiler-rt-sym = pkgs.stdenv.mkDerivation {
      name = "compiler-rt-symlink";
      src = null;
      phases = [ "installPhase" ];
      installPhase = ''
        mkdir -p $out/lib
      '' + (lib.concatStringsSep "\n" (lib.lists.forEach (
        lib.filesystem.listFilesRecursive compiler-rt-lib
      ) (file:
        let
          sl = lib.strings.removePrefix (compiler-rt-lib + "/") (
            builtins.replaceStrings [ "-x86_64" ] [ "" ] file
          );
        in
        "ln -s ${file} $out/lib/${sl}"
      )));
    };
    crt-path = compiler-rt-sym + "/lib";
  in
  {
    # nix develop
    devShells.x86_64-linux.default = pkgs.mkShell.override {
      # set the Clang/LLVM toolchain as default
      stdenv = pkgs."llvmPackages_${LLVM_VER}".stdenv;
    } {
      packages = with pkgs; [
        # cmake and compiler
        cmake
        ninja
        pkgs."llvmPackages_${LLVM_VER}".libllvm
        pkgs."llvmPackages_${LLVM_VER}".bintools
        pkgs."clang_${LLVM_VER}"
        pkgs."lld_${LLVM_VER}"
        pkgs."lldb_${LLVM_VER}"
        # project libraries
        boost
        pkgs."llvmPackages_${LLVM_VER}".openmp
      ];
      shellHook = ''
        export LD_LIBRARY_PATH=${crt-path}
      '';
    };
  };
}
