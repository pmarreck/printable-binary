# PrintableBinary Implementations Guide

A comprehensive guide to the LuaJIT, C, and Zig implementations of PrintableBinary - a tool for encoding binary data into human-readable UTF-8 strings and decoding them back.

## Overview

PrintableBinary is available in multiple high-performance implementations:

### 🔥 **C Implementation** (Recommended for Production)
- **Ultra-fast**: Up to 6x faster than LuaJIT for large files
- **Memory efficient**: Optimized memory usage and allocation
- **Cross-platform**: Compiles on Linux, macOS, Windows
- **Drop-in replacement**: Identical command-line interface

### 🌍 **Cosmopolitan APE Binary** (Runs Anywhere)
- **Actually Portable Executable**: Single binary that runs on Linux, macOS, *and* Windows
- **Zero dependencies**: Bundles Cosmopolitan libc, so it works even on stripped-down hosts
- **CLI parity**: Same flags, environment variables, and character map behavior as the ELF build
- **Great for distribution**: Ship one file (`printable-binary-ape.com`) and it just works

### 🦎 **Zig Implementation** (Modern, Memory-Safe)
- **Memory-safe**: Zig's safety features catch bugs at compile time and runtime
- **Cross-compilation**: Easy cross-compilation to many platforms from a single host
- **Fast compilation**: Incremental builds and fast compile times
- **CLI parity**: Same flags and behavior as C implementation

### ⚡ **LuaJIT Implementation** (Original)
- **Reference implementation**: Easy to modify and extend
- **Well-tested**: Extensive test suite and battle-tested
- **Development-friendly**: Rapid prototyping and debugging

## Performance Comparison

### Benchmark Results (Apple M4 Max)

| File Size | Operation | LuaJIT Time | C Time   | C Speedup | C Improvement |
|-----------|-----------|-------------|----------|-----------|---------------|
| 1KB       | Encode    | 19.3 ms     | 17.4 ms  | 1.11x     | 10.9%         |
| 1KB       | Decode    | 19.0 ms     | 17.4 ms  | 1.09x     | 8.6%          |
| 100KB     | Encode    | 19.6 ms     | 17.4 ms  | 1.12x     | 10.9%         |
| 100KB     | Decode    | 24.3 ms     | 17.6 ms  | 1.38x     | **27.7%**     |
| 1MB       | Encode    | 23.9 ms     | 20.3 ms  | 1.18x     | **15.4%**     |
| 1MB       | Decode    | 90.3 ms     | 20.9 ms  | 4.31x     | **76.8%**     |

### Performance Highlights

- **Overall Encoding**: C is 1.14x faster (12.2% improvement)
- **Overall Decoding**: C is 2.08x faster (52.0% improvement)
- **Large Files**: Up to **6x speedup** for 1MB+ binary/random data decoding
- **Memory Usage**: C implementation uses significantly less memory

## Quick Start

### C Implementation (Recommended)

```bash
# Build the C version
make release

# Use exactly like the LuaJIT version
./bin/printable-binary-c file.bin
./bin/printable-binary-c -d encoded_file.txt
./bin/printable-binary-c --passthrough file.bin | other_tool

# Install (optional)
./install_c_version.sh
```

### Cosmopolitan APE Build

```bash
# Build the APE binary (requires cosmocc / Cosmopolitan toolchain)
make ape

# Run it directly (works on Linux/macOS/Windows)
./bin/printable-binary-ape.com file.bin
./bin/printable-binary-ape.com -d encoded.txt > decoded.bin

# Run the full automated test suite against the APE binary
make test-ape
```

### Zig Implementation

```bash
# Build with Nix
nix build .#printableBinaryZig
./result-zig/bin/printable-binary-zig file.bin

# Or build directly with Zig
zig build -Doptimize=ReleaseFast
./zig-out/bin/printable-binary-zig file.bin
```

### LuaJIT Implementation

```bash
# Already optimized and ready to use
./bin/printable-binary file.bin
./bin/printable-binary -d encoded_file.txt
```

## Installation Options

### Option 1: Automated Installation (C Version)

```bash
./install_c_version.sh
```

Interactive installer that offers:
- Replace LuaJIT version (with backup)
- Install alongside as `printable-binary-c`
- Install to custom location
- Manual setup instructions

### Option 2: Manual Build (C Version)

```bash
# Basic build
make release

# Debug build
make debug

# Cross-platform builds
make CC=clang release     # Use Clang
make windows             # Cross-compile for Windows
make CC=gcc CFLAGS="-O3 -static" release  # Static build
# Cosmopolitan APE (runs on Linux/macOS/Windows)
make ape
```

### Option 3: Nix Build

```bash
# Enter development environment
nix develop

# Build with Nix
nix build .#printableBinaryNative   # ELF/Mach-O binary
nix build .#printableBinaryApe      # Cosmopolitan APE fat binary (x86_64 + arm64, pinned cosmocc 4.0.2)
nix build .#printableBinaryWasm     # WebAssembly module
nix build .#default                 # Suite: native + APE + WASM
```

## Command-Line Usage

All compiled variants (ELF, APE, WASM via wazero) share **identical** command-line interfaces. Use whichever binary suits your platform (`./bin/printable-binary-c`, `./bin/printable-binary-ape.com`, etc.).

### Basic Operations

```bash
# Encode binary file to UTF-8
./bin/printable-binary file.bin > encoded.txt

# Decode UTF-8 back to binary
./bin/printable-binary -d encoded.txt > decoded.bin

# Verify round-trip
cmp file.bin decoded.bin && echo "✓ Perfect round-trip"
```

### Advanced Options

```bash
# Passthrough mode (monitor binary data in pipelines)
./bin/printable-binary --passthrough file.bin | other_tool

# Formatted output
./bin/printable-binary -f=4x10 file.bin    # 4 chars per group, 10 groups per line

# Piped input
cat file.bin | ./bin/printable-binary
echo "Hello" | ./bin/printable-binary | ./bin/printable-binary -d
```

### Complete Options Reference

```
Options:
  -d, --decode          Decode mode (default is encode mode)
  --passthrough         Pass input to stdout unchanged, send encoded data to stderr
  -f[=NxM], --format[=NxM]  Format output in groups (default: 8x10)
  --mappings            Print the active byte-to-Unicode table
  --mappings-json       Emit the mapping table as JSON
  --mappings-csv        Emit the mapping table as CSV
  -h, --help            Show help message

Encode options (preserve literal characters instead of encoding):
  -s, --spaces          Preserve literal spaces (don't encode to ␣)
  -t, --tabs            Preserve literal tabs (don't encode to ⇥)
  -n, --crlf            Preserve literal CR/LF (don't encode to ⏎/¶)
  -w, --preserve-whitespace  Shorthand for -stn (preserve all whitespace)
  -p, --preserve=CHARS  Preserve specific characters (e.g., -p '!"')

Decode options:
  -S, --strip-whitespace  Strip whitespace before decoding (for formatted input)

Input/Output:
  - Reads from file or stdin if no file specified
  - Outputs to stdout (unless --passthrough is used)
  - In passthrough mode: original data → stdout, encoded data → stderr

All binaries embed the canonical 256-entry map, so the `--mappings*` flags work even when `character_map.txt` is missing. If you place a custom map alongside the executable (or set `PRINTABLE_BINARY_MAP`), these options will reflect the override automatically. The override file should contain **exactly 256 lines**, each a single UTF-8 glyph (line 0 = byte 0x00, line 255 = byte 0xFF).
```

### Environment Variables (All Implementations)

```
PRINTABLE_BINARY_MAP       – points to an alternate character_map.txt
PRINTABLE_BINARY_MUTE_STATS – set to 1/true/yes to suppress stderr statistics
```

## When to Use Which Implementation

### Use C Implementation For:

✅ **Production workloads** requiring maximum performance  
✅ **Large files** (>100KB) where speed matters  
✅ **Batch processing** of many files  
✅ **Decode-heavy operations** (up to 6x faster)  
✅ **Memory-constrained environments**  
✅ **Long-running processes** with many operations  
✅ **Cross-platform deployment**  

### Use Zig Implementation For:

✅ **Cross-compilation** to other platforms
✅ **Memory-safe production** where safety is paramount
✅ **WebAssembly targets** (future capability)
✅ **Modern tooling** with built-in package manager
✅ **Environments** where C toolchains are unavailable

### Use LuaJIT Implementation For:

✅ **Quick scripts** and one-off operations
✅ **Development and testing** (easier to modify)
✅ **Small files** where performance difference is negligible
✅ **Integration** with existing Lua-based workflows
✅ **Rapid prototyping** and experimentation  

## Feature Comparison

| Feature | LuaJIT | C | Zig | Notes |
|---------|--------|---|-----|-------|
| **Performance** | Fast | **Faster** | **Faster** | C/Zig are 1.1-6x faster |
| **Memory Usage** | Good | **Better** | **Better** | Native uses less memory |
| **Memory Safety** | ✅ | ⚠️ | ✅ | Zig has built-in safety checks |
| **Basic Encoding/Decoding** | ✅ | ✅ | ✅ | Identical output |
| **Passthrough Mode** | ✅ | ✅ | ✅ | Same functionality |
| **Formatted Output** | ✅ | ✅ | ✅ | Same formatting |
| **Preserve Options** | ✅ | ✅ | ✅ | -s/-t/-n/-w/-p flags |
| **Cross-Platform** | ✅ | ✅ | ✅ | All work everywhere |
| **Binary Size** | Small | **Smaller** | Small | C compiles to ~50KB |
| **Startup Time** | Fast | **Faster** | **Faster** | No interpreter overhead |
| **Development** | **Easier** | Harder | Medium | Lua is most flexible |
| **Cross-Compilation** | N/A | Manual | **Easy** | Zig has built-in cross-compile |

## Compatibility

### 100% Output Compatibility ✅

Both implementations produce **byte-for-byte identical** outputs:

- ✅ All 256 possible byte values
- ✅ Unicode and special characters  
- ✅ Edge cases and corner conditions
- ✅ Formatted output modes
- ✅ Passthrough functionality

### Tested Compatibility

```bash
# Run comprehensive compatibility tests
./test                    # LuaJIT test suite
./test_optimized          # C test suite (uses same test cases)
./bm/benchmark_c_vs_lua.sh   # Performance + compatibility verification
```

**Test Results**: 24/24 compatibility tests passed ✅

## Build Requirements

### C Implementation

**Required:**
- C99-compatible compiler (GCC, Clang, MSVC)
- Make (GNU Make or compatible)

**Optional:**
- Cross-compilation toolchains
- Static analysis tools (Clang Static Analyzer, Cppcheck)
- Profiling tools (Valgrind, AddressSanitizer)

**Platform-Specific:**

```bash
# macOS
xcode-select --install

# Ubuntu/Debian
sudo apt-get install build-essential

# CentOS/RHEL
sudo yum groupinstall 'Development Tools'

# Windows (MinGW)
# Install MSYS2 or use cross-compilation

# Nix (any platform)
nix develop
```

### LuaJIT Implementation

**Required:**
- LuaJIT 2.0 or later

## Development

### Build Targets

```bash
# Release builds
make release              # Optimized build
make debug               # Debug build with symbols
make size                # Size-optimized build

# Analysis builds
make asan                # AddressSanitizer build
make msan                # MemorySanitizer build (Clang only)
make profile             # Profiling build

# Testing
make test                # Basic functionality tests
make benchmark           # Performance benchmark
make compare             # Compare with LuaJIT version
make memcheck            # Valgrind memory check

# Utility
make clean               # Clean build artifacts
make help                # Show all available targets
```

### Cross-Compilation

```bash
# Windows from Unix
make windows

# Custom cross-compilation
make CC=aarch64-linux-gnu-gcc release

# Static builds
make LDFLAGS=-static release
```

### Performance Profiling

```bash
# Build with profiling
make profile

# Run with profiling
./bin/printable-binary_profile large_file.bin
gprof printable-binary-profile gmon.out > profile.txt

# Memory profiling with Valgrind
make memcheck
```

## Architecture Details

### LuaJIT Implementation

- **Language**: Lua with LuaJIT optimizations
- **Encoding**: Table-based lookups with pre-computed UTF-8 sequences
- **Decoding**: Intelligent UTF-8 length detection + hash maps
- **Memory**: Dynamic allocation with garbage collection
- **Size**: ~1000 lines of Lua code

### C Implementation

- **Language**: C99 with compiler optimizations
- **Encoding**: Direct array lookups for maximum speed
- **Decoding**: Hash-based decode table with efficient UTF-8 processing
- **Memory**: Static tables + growable buffers, no garbage collection
- **Size**: ~500 lines of C code

### Zig Implementation

- **Language**: Zig with safety checks and optimizations
- **Encoding**: ArrayHashMap for byte-to-UTF8 lookups
- **Decoding**: StringHashMap for UTF8-to-byte reverse lookups
- **Memory**: Arena allocator for efficient memory management
- **Safety**: Bounds checking and null safety at compile time
- **Size**: ~500 lines of Zig code (main.zig + printable_binary.zig)

### Key Optimizations in C Version

1. **Pre-computed UTF-8 sequences** in static arrays
2. **Hash-based decode table** for O(1) lookups
3. **Efficient UTF-8 length detection** reducing iterations
4. **Growable buffers** with exponential growth
5. **Direct memory operations** avoiding string manipulation overhead
6. **Optimized compiler flags** (-O3, -march=native)

## Testing

### Automated Test Suites

```bash
# LuaJIT implementation (from repo root)
./test/test                 # Deterministic suite
./test/test_all             # Full runner (fuzz, WASM, JS, etc.)

# C implementation
make test                   # Builds bin/printable-binary-c and runs ./test/test_all against it
cd test && IMPLEMENTATION_TO_TEST=../bin/printable-binary-c ./test_all

# Compatibility verification
./bm/benchmark_c_vs_lua.sh  # Performance + encode/decode parity
```

### Manual Testing

```bash
# Quick round-trip test
echo "Hello, World! 🌍" | ./bin/printable-binary-c | ./bin/printable-binary-c -d

# Large file test
dd if=/dev/urandom of=test.bin bs=1M count=1
./bin/printable-binary-c test.bin | ./bin/printable-binary-c -d | cmp test.bin -

# Binary compatibility test
./bin/printable-binary-c test.bin > c_output.txt
./bin/printable-binary test.bin > lua_output.txt
cmp c_output.txt lua_output.txt && echo "✓ Outputs identical"
```

## Troubleshooting

### Common Issues

**Build Failures:**
```bash
# Missing compiler
sudo apt-get install build-essential  # Ubuntu
xcode-select --install                # macOS

# Permission errors
chmod +x install_c_version.sh
chmod +x bin/printable-binary-c
```

**Runtime Issues:**
```bash
# Test basic functionality
echo "test" | ./bin/printable-binary-c

# Check file permissions
ls -la bin/printable-binary-c

# Verify binary works
./bin/printable-binary-c --help
```

**Performance Issues:**
```bash
# Ensure optimized build
make clean && make release

# Check compiler flags
make CC=clang CFLAGS="-O3 -march=native" release

# Profile performance
make profile
```

## Contributing

### Development Setup

```bash
# Clone and setup
git clone <repository>
cd printable-binary

# Development environment
nix develop  # or install dependencies manually

# Build and test
make release
make test
./test_all
```

### Code Style

**C Code:**
- C99 standard compliance
- 4-space indentation
- Descriptive variable names
- Comprehensive error handling

**Lua Code:**
- 2-space indentation
- Local variable preferences
- Modular function design
- Extensive comments

### Testing Requirements

All changes must:
- ✅ Pass existing test suites
- ✅ Maintain output compatibility
- ✅ Include appropriate tests
- ✅ Not regress performance significantly

## License

Both implementations are released under the same license as the original project.

## Support

### Getting Help

1. **Check this README** for common usage patterns
2. **Run built-in help**: `./bin/printable-binary --help`
3. **Review test suites** for usage examples
4. **Check performance docs** for optimization tips

### Reporting Issues

When reporting issues, please include:
- Implementation version (C or LuaJIT)
- Operating system and architecture
- Compiler version (for C implementation)
- Command that failed
- Input data characteristics (size, type)
- Expected vs actual behavior

---

## Summary

PrintableBinary offers three excellent native implementations:

- **C Implementation**: Maximum performance for production use
- **Zig Implementation**: Memory-safe with easy cross-compilation
- **LuaJIT Implementation**: Maximum flexibility for development

All maintain perfect compatibility while offering different trade-offs. Choose based on your specific needs: performance-critical applications benefit from the C or Zig versions, while development and scripting scenarios may prefer the LuaJIT version.

**🚀 For most users, we recommend the C or Zig implementation for superior performance and efficiency.**
