# PrintableBinary Project Structure

This document describes the organization of the PrintableBinary project after the restructuring.

## Directory Structure

```
printable-binary/
├── bin/                    # Entry points + build outputs
│   ├── printable-binary        # LuaJIT implementation (main CLI)
│   ├── printable-binary-node.js
│   └── printable-binary-c      # Built by `make release` (not committed)
├── js/                     # Shared JavaScript module used by CLI/browser
├── bm/                     # Benchmark helpers and comparison scripts
├── test/                   # All tests (Lua + JS + WASM + fuzz)
│   ├── test                # Main unit test suite
│   ├── test_all            # Master test runner
│   ├── fuzz_test           # Randomized testing
│   ├── benchmark_test      # Performance benchmarks
│   ├── test_wasm           # WASM validation
│   ├── test_cross_implementation.sh
│   └── js/                 # Deno/Node-based harnesses
├── utils/                  # Utility scripts (Lua + supporting data)
│   ├── audit_character_map.lua
│   ├── generate_embedded_map.lua
│   └── js/                 # Mapping/diagnostic helpers used by Node tooling
├── src/                    # C source tree
│   └── printable_binary.c  # High-performance implementation
├── Makefile               # Build system
└── [documentation files]
```

## Key Files

### Implementations
- **`bin/printable-binary`** - Original LuaJIT implementation (requires LuaJIT)
- **`src/printable_binary.c`** - C source code for high-performance version
- **`bin/printable-binary-c`** - Compiled C binary (created by `make`)

### Build System
- **`Makefile`** - Builds C implementation into `bin/` directory
  - `make` or `make release` - Build optimized version
  - `make test` - Build and run full test suite on C version
  - `make clean` - Remove build artifacts

### Testing
- **`test/test_all`** - Master test runner, calls all other test scripts
- **`test/test`** - Main unit and integration tests
- **`test/fuzz_test`** - Randomized data testing
- **`test/benchmark_test`** - Performance measurements

## Usage

### Building
```bash
# Build C implementation
make

# Build with specific compiler
make CC=clang

# Clean and rebuild
make clean && make
```

### Testing

#### Default Testing (LuaJIT version)
```bash
# Run from project root
make test           # via Makefile
# OR
test/test_all       # directly

# Run from test directory
cd test && ./test_all
```

#### Testing C Implementation
```bash
# Set environment variable to test C version
IMPLEMENTATION_TO_TEST=bin/printable-binary-c make test
# OR
cd test && IMPLEMENTATION_TO_TEST=../bin/printable-binary-c ./test_all
```

#### Testing Custom Implementation
```bash
# Test any implementation by setting the path
IMPLEMENTATION_TO_TEST=/path/to/my/version make test
```

### Environment Variable Support

All test scripts support the `IMPLEMENTATION_TO_TEST` environment variable:

- **Default**: `../bin/printable-binary` (LuaJIT version)
- **C version**: `../bin/printable-binary-c`
- **Custom**: Any path to a compatible implementation

This allows the same comprehensive test suite to validate any implementation.

## Implementation Compatibility

Both implementations provide identical functionality:
- Same command-line interface
- Same encoding/decoding behavior
- Same output format
- All tests pass for both versions

The C version offers significantly better performance while maintaining full compatibility with the LuaJIT version.
