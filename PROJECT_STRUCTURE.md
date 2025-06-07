# PrintableBinary Project Structure

This document describes the organization of the PrintableBinary project after the restructuring.

## Directory Structure

```
printable-binary/
├── bin/                    # Compiled binaries
│   └── printable_binary_c  # C implementation (compiled)
├── test/                   # All test files
│   ├── test               # Main unit test suite
│   ├── test_all           # Master test runner
│   ├── fuzz_test          # Randomized testing
│   ├── benchmark_test     # Performance benchmarks
│   └── test_binary.bin    # Test data file
├── utils/                  # Utility scripts
├── printable_binary        # LuaJIT implementation (main script)
├── printable_binary.c      # C source code
├── Makefile               # Build system
└── [documentation files]
```

## Key Files

### Implementations
- **`printable_binary`** - Original LuaJIT implementation (requires LuaJIT)
- **`printable_binary.c`** - C source code for high-performance version
- **`bin/printable_binary_c`** - Compiled C binary (created by `make`)

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
IMPLEMENTATION_TO_TEST=bin/printable_binary_c make test
# OR
cd test && IMPLEMENTATION_TO_TEST=../bin/printable_binary_c ./test_all
```

#### Testing Custom Implementation
```bash
# Test any implementation by setting the path
IMPLEMENTATION_TO_TEST=/path/to/my/version make test
```

### Environment Variable Support

All test scripts support the `IMPLEMENTATION_TO_TEST` environment variable:

- **Default**: `../printable_binary` (LuaJIT version)
- **C version**: `../bin/printable_binary_c`
- **Custom**: Any path to a compatible implementation

This allows the same comprehensive test suite to validate any implementation.

## Implementation Compatibility

Both implementations provide identical functionality:
- Same command-line interface
- Same encoding/decoding behavior
- Same output format
- All tests pass for both versions

The C version offers significantly better performance while maintaining full compatibility with the LuaJIT version.