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
      in
      {
        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            # C compilation tools
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
            
            # Development utilities
            xxd
            hexdump
            file
            
            # Benchmarking and testing
            luajit
            
            # Memory debugging
            address-sanitizer
          ];
          
          shellHook = ''
            echo "PrintableBinary C Development Environment"
            echo "========================================"
            echo "Available compilers:"
            echo "  gcc: $(gcc --version | head -n1)"
            echo "  clang: $(clang --version | head -n1)"
            echo ""
            echo "Available tools:"
            echo "  make, cmake, ninja"
            echo "  gdb, valgrind"
            echo "  hyperfine (for benchmarking)"
            echo ""
            echo "Example build commands:"
            echo "  gcc -O3 -o printable_binary_c printable_binary.c"
            echo "  clang -O3 -march=native -o printable_binary_c printable_binary.c"
            echo ""
            echo "Cross-compilation example:"
            echo "  x86_64-w64-mingw32-gcc -O3 -o printable_binary.exe printable_binary.c"
            echo ""
          '';
          
          # Set environment variables for cross-compilation
          CC = "gcc";
          CXX = "g++";
        };
        
        # Package the C version when built
        packages.default = pkgs.stdenv.mkDerivation {
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
      });
}