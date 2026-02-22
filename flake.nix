{
  description = "PrintableBinary C implementation development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        cosmoccVersion = "4.0.2";
        cosmoccHash = "sha256-6KZv7KU2rJhwfc9k6z9I6ZdfIS1KqRFLZUo8YyuD7ZY=";
        cosmoccSrc = pkgs.fetchzip {
          url = "https://cosmo.zip/pub/cosmocc/cosmocc-${cosmoccVersion}.zip";
          hash = cosmoccHash;
          stripRoot = false;
        };
        cosmoccBin = pkgs.stdenvNoCC.mkDerivation {
          pname = "cosmocc-bin";
          version = cosmoccVersion;
          src = cosmoccSrc;
          phases = [ "installPhase" ];
          installPhase = ''
            mkdir -p $out
            cp -r $src/* $out/
          '';
        };

        emscriptenFlags = "-O3 -DNDEBUG "
          + "-s STANDALONE_WASM=1 "
          + "-s FILESYSTEM=1 "
          + "-s INITIAL_MEMORY=134217728 "
          + "-DPRINTABLE_BINARY_HELP_NAME=\\\"printable_binary\\\"";

        printableBinaryNative = pkgs.stdenv.mkDerivation {
          pname = "printable-binary-c";
          version = "1.0.0";

          src = ./.;

          nativeBuildInputs = [ pkgs.clang ];

          buildPhase = ''
            clang -O3 -Wall -Wextra -I. -o printable_binary_c src/printable_binary.c
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp printable_binary_c $out/bin/
            cp character_map.txt $out/bin/character_map.txt
          '';

          meta = with pkgs.lib; {
            description = "High-performance C implementation of PrintableBinary";
            license = licenses.mit;
            platforms = platforms.unix;
          };
        };

        printableBinaryWasm = pkgs.stdenv.mkDerivation {
          pname = "printable-binary-wasm";
          version = "1.0.0";

          src = ./.;

          nativeBuildInputs = [ pkgs.emscripten ];

          buildPhase = ''
            export EM_CACHE="$TMPDIR/emscripten_cache"
            mkdir -p "$EM_CACHE"
            emcc -I. src/printable_binary.c ${emscriptenFlags} -o printable_binary.wasm
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp printable_binary.wasm $out/bin/printable_binary.wasm
            cp character_map.txt $out/bin/character_map.txt
          '';

          meta = with pkgs.lib; {
            description = "PrintableBinary compiled to WebAssembly via Emscripten";
            license = licenses.mit;
            platforms = platforms.all;
          };
        };

        # APE build - only works reliably on Linux in pure nix builds
        # On darwin, cosmocc's APE loader needs system tools; use `nix develop` + make ape
        printableBinaryApe = pkgs.stdenvNoCC.mkDerivation {
          pname = "printable-binary-ape";
          version = "1.0.0";

          src = ./.;

          nativeBuildInputs = [
            cosmoccBin
          ] ++ pkgs.lib.optionals pkgs.stdenv.isLinux [
            pkgs.coreutils
          ];

          buildPhase = ''
            export PATH=${cosmoccBin}/bin:$PATH
            export HOME=$TMPDIR
            # Clear any inherited include paths that might conflict
            unset C_INCLUDE_PATH CPATH CPLUS_INCLUDE_PATH OBJC_INCLUDE_PATH
            cosmocc -O3 -DNDEBUG -I. -o printable_binary_ape.com src/printable_binary.c
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp printable_binary_ape.com $out/bin/printable_binary_ape.com
            cp character_map.txt $out/bin/character_map.txt
          '';

          meta = with pkgs.lib; {
            description = "PrintableBinary built as an Actually Portable Executable (Cosmopolitan, pinned ${cosmoccVersion})";
            license = licenses.mit;
            # APE pure nix build only works reliably on Linux; on darwin use nix develop + make ape
            platforms = platforms.linux;
          };
        };

        # Zig implementation
        printableBinaryZig = pkgs.stdenv.mkDerivation {
          pname = "printable-binary-zig";
          version = "1.0.0";

          src = ./.;

          nativeBuildInputs = [ pkgs.zig ];

          buildPhase = ''
            export HOME=$TMPDIR
            zig build -Doptimize=ReleaseFast
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp zig-out/bin/printable_binary_zig $out/bin/
            cp character_map.txt $out/bin/character_map.txt
          '';

          meta = with pkgs.lib; {
            description = "PrintableBinary Zig implementation";
            license = licenses.mit;
            platforms = platforms.unix;
          };
        };
      in
      {
        devShells.default = pkgs.mkShell {
          buildInputs =
            let
              linuxOnly = with pkgs; lib.optionals (!stdenv.isDarwin) [ valgrind perf-tools ];
            in
            with pkgs; [
              # C compilation/tools
              gcc
              clang

              # Build systems
              gnumake
              cmake
              ninja

              # Performance and profiling tools
              hyperfine
              time

              # Cross-compilation targets (optional)
              pkgsCross.mingwW64.buildPackages.gcc

              # Actually Portable Executable (cosmopolitan, pinned)
              cosmoccBin

              # Development utilities
              xxd
              hexdump
              file

              # Benchmarking and testing
              luajit
              wazero

              # Zig compiler
              zig

              # JavaScript/TypeScript runtime for web implementation
              deno
              nodejs_20

              # WebAssembly toolchain
              emscripten
            ]  ++ lib.optionals stdenv.isDarwin [ lldb ]
               ++ lib.optionals (!stdenv.isDarwin) [ gdb ]
               ++ linuxOnly;

          shellHook = ''
            echo "PrintableBinary Development Environment"
            echo "========================================"
            echo "Available compilers:"
            echo "  gcc: $(gcc --version | head -n1)"
            echo "  clang: $(clang --version | head -n1)"
            echo ""
            echo "Available tools:"
            echo "  make, cmake, ninja"
            echo "  gdb, valgrind"
            echo "  hyperfine (for benchmarking)"
            echo "  deno (for JavaScript/web implementation)"
            echo "  node (for CLI/automation tests)"
            echo "  emcc (Emscripten) for WebAssembly builds"
            echo "  cosmocc ${cosmoccVersion} (pinned) for APE builds (fat x86_64 + arm64)"
            echo "  wazero (WASI runtime for testing)"
            echo ""
            echo "Example build commands:"
            echo "  gcc -O3 -o printable_binary_c src/printable_binary.c"
            echo "  clang -O3 -march=native -o printable_binary_c src/printable_binary.c"
            echo "  emcc src/printable_binary.c ${emscriptenFlags} -o printable_binary.wasm"
            echo "  cosmocc -O3 -o printable_binary_ape.com src/printable_binary.c   # fat APE"
            echo "  make -B wasm                                                 # uses emcc"
            echo "  CONFIRM_BIG_DEP_DOWNLOAD=1 make -B ape                       # uses pinned cosmocc"
            echo "  nix build .#printableBinaryApe    # fat APE (pinned cosmocc ${cosmoccVersion})"
            echo "  nix build .#printableBinaryWasm   # wasm"
            echo "  nix build .#default               # suite (native+wasm+ape)"
            echo ""
            echo "Test JavaScript implementation:"
            echo "  deno run --allow-read --allow-env test/js/test_printable_binary.js"
            echo ""
            echo "Serve web interface (requires simple http server):"
            echo "  python3 -m http.server 8000"
            echo "  # Then open http://localhost:8000 in your browser"
            echo ""
            echo "Cross-compilation example:"
            echo "  x86_64-w64-mingw32-gcc -O3 -o printable_binary.exe src/printable_binary.c"
            echo ""
          '';

          CC = "gcc";
          CXX = "g++";
        };

        packages = {
          printableBinaryNative = printableBinaryNative;
          printableBinaryWasm = printableBinaryWasm;
          printableBinaryApe = printableBinaryApe;
          printableBinaryZig = printableBinaryZig;
          default = pkgs.symlinkJoin {
            name = "printable-binary-suite";
            paths = [ printableBinaryNative printableBinaryWasm printableBinaryZig ]
              ++ pkgs.lib.optionals pkgs.stdenv.isLinux [ printableBinaryApe ];
          };
        };

        checks = {
          test-c = pkgs.stdenv.mkDerivation {
            name = "test-c";
            src = ./.;
            nativeBuildInputs = with pkgs; [ clang python3 xxd hexdump ];
            buildPhase = ''
              clang -O3 -Wall -Wextra -I. -o printable_binary_c src/printable_binary.c
              IMPLEMENTATION_TO_TEST=./printable_binary_c bash ./test/test
            '';
            installPhase = "mkdir -p $out && touch $out/passed";
          };

          test-zig-unit = pkgs.stdenv.mkDerivation {
            name = "test-zig-unit";
            src = ./.;
            nativeBuildInputs = [ pkgs.zig ];
            buildPhase = ''
              export HOME=$TMPDIR
              zig build test
            '';
            installPhase = "mkdir -p $out && touch $out/passed";
          };

          test-zig = pkgs.stdenv.mkDerivation {
            name = "test-zig";
            src = ./.;
            nativeBuildInputs = with pkgs; [ zig python3 xxd hexdump ];
            buildPhase = ''
              export HOME=$TMPDIR
              zig build -Doptimize=ReleaseFast
              IMPLEMENTATION_TO_TEST=./zig-out/bin/printable_binary_zig bash ./test/test
            '';
            installPhase = "mkdir -p $out && touch $out/passed";
          };
        } // pkgs.lib.optionalAttrs pkgs.stdenv.isLinux {
          test-ape = pkgs.stdenvNoCC.mkDerivation {
            name = "test-ape";
            src = ./.;
            nativeBuildInputs = with pkgs; [ cosmoccBin coreutils python3 xxd hexdump bash ];
            buildPhase = ''
              export PATH=${cosmoccBin}/bin:$PATH
              export HOME=$TMPDIR
              unset C_INCLUDE_PATH CPATH CPLUS_INCLUDE_PATH OBJC_INCLUDE_PATH
              cosmocc -O3 -DNDEBUG -I. -o printable_binary_ape.com src/printable_binary.c
              chmod +x printable_binary_ape.com
              IMPLEMENTATION_TO_TEST=./printable_binary_ape.com bash ./test/test
            '';
            installPhase = "mkdir -p $out && touch $out/passed";
          };
        };
      });
}
