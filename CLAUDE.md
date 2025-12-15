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
- Example: `DEBUG=1 ./bin/printable_binary --mappings`
- Helpful when diagnosing mapping load issues or unexpected decode behavior

## Code Style
- Follow existing patterns and naming conventions
- Add comments for complex logic
- Keep functions focused on a single responsibility
- Use Lua patterns consistently
- Handle errors gracefully
