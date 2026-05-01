# requires Nix package manager with Flake enabled
# see https://nixos.org/download/
# and https://wiki.nixos.org/wiki/Flakes
{
  inputs = {
    # update by running `nix flake update nixpkgs`
    # last update on 19.10.2025
    nixpkgs.url = "nixpkgs/nixos-unstable";
  };

  outputs = {
    self, nixpkgs,
    ...
  }:
  let
    pkgs = import nixpkgs { system = "x86_64-linux"; };
    inherit (pkgs) lib dockerTools;

    LLVM_VER = "21";
    LLVM_PKGS = pkgs."llvmPackages_${LLVM_VER}";

    # symlink all `libclang_rt.*-x86_64.so` as libclang_rt.*.so
    compiler-rt = LLVM_PKGS.compiler-rt-libc;
    clang-custom = LLVM_PKGS.clangNoCompilerRtWithLibc.overrideAttrs (final: prev: {
      postFixup = ''
        ${prev.postFixup or ""}

        ln -s ${compiler-rt}/share $out/resource-root/share
        mkdir -p $out/resource-root/lib/linux
        mkdir -p $out/resource-root/lib/x86_64-unknown-linux-gnu
      '' + (lib.concatStringsSep "\n" (lib.lists.forEach (
        lib.filesystem.listFilesRecursive "${compiler-rt}/lib/linux"
      ) (file:
        let
          bf = lib.strings.removePrefix ("${compiler-rt}/lib/linux/") file;
          rs = builtins.replaceStrings [ "-x86_64" ] [ "" ] bf;
          lib-path = "$out/resource-root/lib";
          ln-file = (target: link: ''
            ln -s ${target} ${lib-path}/linux/${link}
            ln -s ${target} ${lib-path}/x86_64-unknown-linux-gnu/${link}
          '');
        in
        (ln-file file bf) + (lib.optionals (bf != rs) (ln-file file rs))
      )));
    });

    python-custom = (pkgs.python3.withPackages (ps: with ps; [
      pandas
      matplotlib
      seaborn
    ]));

    wrapper-alias = (binName: varName: pkgs.writeShellScriptBin binName ''
      if [ -z "''$${varName}" ]; then
        echo "environment variable \"${varName}\" not set!"
        exit 1
      fi
      if ! [ -x "''$${varName}" ]; then
        echo "The file \"''$${varName}\" is not executable!"
        exit 1
      fi
      exec -a ${binName} "''$${varName}" $@
    '');

    # QoL change for testing
    shellHook = ''
      GIT_REPO_ROOT=$(git rev-parse --show-toplevel)
      THREAD_SAN_PATH="''${GIT_REPO_ROOT}/build/use-cases/thread-sanitizer"

      # enable precompile pass in wrapper
      ENV_SCRIPT="''${THREAD_SAN_PATH}/setup_env.sh"
      if [ -f "$ENV_SCRIPT" ]; then
        source "$ENV_SCRIPT"
      fi
    '';
  in
  rec {
    # nix develop
    devShells.x86_64-linux.default = pkgs.mkShell.override {
      # set the Clang/LLVM toolchain as default
      inherit (LLVM_PKGS) stdenv;
    } {
      packages = with pkgs; [
        # own needs
        zsh
        # cmake and compiler
        cmake
        ninja
        clang-custom
        LLVM_PKGS.flang # lacks useable linker integration (and flang-rt)
        LLVM_PKGS.bintools
        LLVM_PKGS.libllvm
        LLVM_PKGS.lld
        mpi
        # project libraries
        boost
        LLVM_PKGS.openmp
        # project scripts
        git
        gnumake
        gnupatch
        python-custom
        rsync
        time
        util-linux
        # aliases for wrappers
        (wrapper-alias "clang_wrap_cc"  "CLANG_WRAP_CC")
        (wrapper-alias "clang_wrap_cxx" "CLANG_WRAP_CXX")
        (wrapper-alias "flang_wrap"     "CLANG_WRAP_FC")
      ];
      inherit shellHook;
    };
    packages.x86_64-linux.docker-image = pkgs.dockerTools.buildNixShellImage {
      name = "localhost/precompute-devshell";
      tag = "latest";
      drv = devShells.x86_64-linux.default.overrideAttrs (final: prev: {
        shellHookScript = pkgs.writeShellScript "shellHook" (''
          if [ -n "$HPC_SCRATCH" ] && [ -d "$HPC_SCRATCH" ]; then
            export TMP=$HPC_SCRATCH
            export TMPDIR=$HPC_SCRATCH
            export TEMP=$HPC_SCRATCH
            export TEMPDIR=$HPC_SCRATCH
          fi
          export SSL_CERT_FILE=${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt
        '' + shellHook);
        shellHook = "source ${final.shellHookScript}";
        packages = prev.nativeBuildInputs ++ (with pkgs; [
          dockerTools.binSh dockerTools.usrBinEnv
          coreutils util-linux
          curl gnugrep ncurses
        ]);
      });
      shell = "${pkgs.bashInteractive}/bin/bash";
    };
  };
}
