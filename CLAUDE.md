# Working With This Repository

## Project Overview
The PrintableBinary tool encodes arbitrary binary data into human-readable UTF-8 strings and decodes them back to the original binary. It's designed as an alternative to hexadecimal encoding with better visual density and immediate recognition of embedded ASCII text.

## Code Organization
- Main executable: `printable_binary` (Lua script using LuaJIT)
- Test scripts: Files prefixed with `test_` and shell scripts with `_test.sh` suffix
- Supporting files: `dump_maps.lua`, `bench.lua`, etc.

## Development Philosophy

### Test-Driven Development (TDD)
When making changes to this codebase, follow TDD principles:

1. **Write a test first** that demonstrates the expected behavior or reproduces the bug
2. **Run the test** to confirm it fails as expected
3. **Implement minimal changes** to make the test pass
4. **Refactor** while ensuring tests still pass

### Testing
- Use `test_*.lua` files for unit/functional tests
- `*_test.sh` or `verify.sh` for integration/verification tests
- Tests should be atomic and independent
- Cover both normal use cases and edge cases
- `fuzz_test.sh` provides randomized testing

## Debug Mode
- Set `DEBUG=1` environment variable to enable detailed diagnostic output
- Example: `DEBUG=1 ./printable_binary -a /usr/bin/ditto`
- Helpful when diagnosing architecture selection or disassembly issues

## Architecture Selection
- The tool supports multiple architectures: x64, x32, arm64, arm
- Auto-detection selects the best architecture based on instruction compatibility
- Universal binaries (macOS) are detected with appropriate warnings
- Architecture names are displayed in user-friendly format (e.g., "x86_64" instead of "x64")
- On Apple Silicon, arm64 is preferred when appropriate

## Disassembly Features
- Uses Capstone (`cstool`) for disassembly
- Groups similar instructions for better readability
- Identifies common patterns like NUL sequences and NOPs
- Formats output with configurable grouping
- Uses 🧾 emoji to separate binary data from disassembly text
- Handles alignment issues by correctly interpreting hexadecimal offsets

## Potential Issues and Workarounds
- If auto-detection selects the wrong architecture, specify it with `--arch`
- Universal binaries can only be disassembled for one architecture at a time
- Decoding from disassembly output won't match the original binary for universal binaries
- Long sequences of repeated bytes are grouped for readability

## Code Style
- Follow existing patterns and naming conventions
- Add comments for complex logic
- Keep functions focused on a single responsibility
- Use Lua patterns consistently
- Handle errors gracefully