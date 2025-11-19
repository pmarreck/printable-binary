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
        emscriptenFlags = "-O3 -DNDEBUG "
          + "-s STANDALONE_WASM=1 "
          + "-s FILESYSTEM=1 "
          + "-s INITIAL_MEMORY=134217728 "
          + "-DPRINTABLE_BINARY_HELP_NAME=\\\"printable_binary\\\"";

        printableBinaryNative = pkgs.stdenv.mkDerivation {
          pname = "printable-binary-c";
          version = "1.0.0";

          src = ./.;

          buildInputs = [ pkgs.gcc ];

          buildPhase = ''
            gcc -O3 -march=native -Wall -Wextra -o printable_binary_c printable_binary.c
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp printable_binary_c $out/bin/
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
            emcc printable_binary.c ${emscriptenFlags} -o printable_binary.wasm
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

        printableBinaryApe = pkgs.stdenv.mkDerivation {
          pname = "printable-binary-ape";
          version = "1.0.0";

          src = ./.;

          nativeBuildInputs = [ pkgs.cosmopolitan ];

          buildPhase = ''
            cosmocc -O3 -DNDEBUG -o printable_binary_ape.com printable_binary.c
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp printable_binary_ape.com $out/bin/printable_binary_ape.com
            cp character_map.txt $out/bin/character_map.txt
          '';

          meta = with pkgs.lib; {
            description = "PrintableBinary built as an Actually Portable Executable (Cosmopolitan)";
            license = licenses.mit;
            platforms = platforms.all;
          };
        };
      in
      {
        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            # C compilation/tools
            gcc
            clang
            gdb
            valgrind

            # Build systems
            gnumake
            cmake
            ninja

            # Performance and profiling tools
            perf-tools
            hyperfine
            time

            # Cross-compilation targets (optional)
            pkgsCross.mingwW64.buildPackages.gcc

            # Actually Portable Executable (cosmopolitan)
            cosmopolitan

            # Development utilities
            xxd
            hexdump
            file

            # Benchmarking and testing
            luajit
            wazero

            # JavaScript/TypeScript runtime for web implementation
            deno
            nodejs_20

            # WebAssembly toolchain
            emscripten
          ];

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
            echo "  cosmocc (Cosmopolitan) for APE builds"
            echo "  wazero (WASI runtime for testing)"
            echo ""
            echo "Example build commands:"
            echo "  gcc -O3 -o printable_binary_c printable_binary.c"
            echo "  clang -O3 -march=native -o printable_binary_c printable_binary.c"
            echo "  emcc printable_binary.c ${emscriptenFlags} -o printable_binary.wasm"
            echo "  cosmocc -O3 -o printable_binary_ape.com printable_binary.c"
            echo ""
            echo "Test JavaScript implementation:"
            echo "  deno run --allow-read --allow-env test_printable_binary.js"
            echo ""
            echo "Serve web interface (requires simple http server):"
            echo "  python3 -m http.server 8000"
            echo "  # Then open http://localhost:8000 in your browser"
            echo ""
            echo "Cross-compilation example:"
            echo "  x86_64-w64-mingw32-gcc -O3 -o printable_binary.exe printable_binary.c"
            echo ""
          '';

          CC = "gcc";
          CXX = "g++";
        };

        packages = {
          printableBinaryNative = printableBinaryNative;
          printableBinaryWasm = printableBinaryWasm;
          default = pkgs.symlinkJoin {
            name = "printable-binary-suite";
            paths = [ printableBinaryNative printableBinaryWasm printableBinaryApe ];
          };
        };
      });
}
