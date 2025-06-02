#!/bin/bash
# Test script for printable_binary

# Exit on error
set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check if luajit is installed
if ! command -v luajit &> /dev/null; then
    echo -e "${RED}Error: luajit is not installed. Please install it before running tests.${NC}"
    exit 1
fi

# Path to the printable_binary script
SCRIPT="./printable_binary"

# Test counter
TEST_COUNT=0
PASS_COUNT=0
FAIL_COUNT=0

# Create temp files for testing
TMP_BINARY=$(mktemp)
TMP_ENCODED=$(mktemp)
TMP_DECODED=$(mktemp)

# Cleanup function
cleanup() {
    rm -f "$TMP_BINARY" "$TMP_ENCODED" "$TMP_DECODED"
}
trap cleanup EXIT

echo "=== Testing PrintableBinary ==="
echo "Running tests..."

# Test 1: Simple ASCII roundtrip
TEST_COUNT=$((TEST_COUNT + 1))
echo -e "${BLUE}Test #$TEST_COUNT: Simple ASCII roundtrip${NC}"
TEST_STRING="Hello, World!"
echo -n "$TEST_STRING" > "$TMP_BINARY"
$SCRIPT "$TMP_BINARY" > "$TMP_ENCODED"
$SCRIPT -d "$TMP_ENCODED" > "$TMP_DECODED"
if cmp -s "$TMP_BINARY" "$TMP_DECODED"; then
    echo -e "${GREEN}PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}FAIL${NC}"
    echo "Original: $(xxd -p "$TMP_BINARY")"
    echo "Decoded: $(xxd -p "$TMP_DECODED")"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# Test 2: Space character encoding
TEST_COUNT=$((TEST_COUNT + 1))
echo -e "${BLUE}Test #$TEST_COUNT: Space encoding${NC}"
printf ' ' > "$TMP_BINARY"
ENCODED=$($SCRIPT "$TMP_BINARY")
if [[ "$ENCODED" == "␣" ]]; then
    echo -e "${GREEN}PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}FAIL${NC}"
    echo "Expected: ␣"
    echo "Got: $ENCODED"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# Test 3: Quotes and backslashes
TEST_COUNT=$((TEST_COUNT + 1))
echo -e "${BLUE}Test #$TEST_COUNT: Quotes and backslashes roundtrip${NC}"
printf '\"\\\n' > "$TMP_BINARY"
$SCRIPT "$TMP_BINARY" > "$TMP_ENCODED"
$SCRIPT -d "$TMP_ENCODED" > "$TMP_DECODED"
if cmp -s "$TMP_BINARY" "$TMP_DECODED"; then
    echo -e "${GREEN}PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}FAIL${NC}"
    echo "Original: $(xxd -p "$TMP_BINARY")"
    echo "Decoded: $(xxd -p "$TMP_DECODED")"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# Test 4: Control characters
TEST_COUNT=$((TEST_COUNT + 1))
echo -e "${BLUE}Test #$TEST_COUNT: Control characters roundtrip${NC}"
# Create a file with control characters
printf '\000\001\002\003\004' > "$TMP_BINARY"
$SCRIPT "$TMP_BINARY" > "$TMP_ENCODED"
$SCRIPT -d "$TMP_ENCODED" > "$TMP_DECODED"
if cmp -s "$TMP_BINARY" "$TMP_DECODED"; then
    echo -e "${GREEN}PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}FAIL${NC}"
    echo "Original: $(xxd -p "$TMP_BINARY")"
    echo "Encoded: $(xxd -p "$TMP_ENCODED")"
    echo "Decoded: $(xxd -p "$TMP_DECODED")"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# Test 5: Full binary roundtrip (all 256 bytes)
TEST_COUNT=$((TEST_COUNT + 1))
echo -e "${BLUE}Test #$TEST_COUNT: Full binary roundtrip (all 256 bytes)${NC}"
# Create a file with all 256 possible byte values
> "$TMP_BINARY"
for i in {0..255}; do
    printf "\\$(printf '%03o' $i)" >> "$TMP_BINARY"
done
$SCRIPT "$TMP_BINARY" > "$TMP_ENCODED"
$SCRIPT -d "$TMP_ENCODED" > "$TMP_DECODED"
if cmp -s "$TMP_BINARY" "$TMP_DECODED"; then
    echo -e "${GREEN}PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}FAIL${NC}"
    echo "Binary file roundtrip failed."
    # Find first mismatch
    xxd "$TMP_BINARY" > "${TMP_BINARY}.hex"
    xxd "$TMP_DECODED" > "${TMP_DECODED}.hex"
    diff -u "${TMP_BINARY}.hex" "${TMP_DECODED}.hex" | head -n 20
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# Test 6: Piped input
TEST_COUNT=$((TEST_COUNT + 1))
echo -e "${BLUE}Test #$TEST_COUNT: Piped input${NC}"
RESULT=$(echo -n "Test" | $SCRIPT | $SCRIPT -d)
if [[ "$RESULT" == "Test" ]]; then
    echo -e "${GREEN}PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}FAIL${NC}"
    echo "Expected: Test"
    echo "Got: $RESULT"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# Test 7: Completeness check - make sure we can run all commands
TEST_COUNT=$((TEST_COUNT + 1))
echo -e "${BLUE}Test #$TEST_COUNT: Basic help text${NC}"
HELP_OUTPUT=$($SCRIPT --help)
if [[ "$HELP_OUTPUT" == *"Usage:"* && "$HELP_OUTPUT" == *"Options:"* ]]; then
    echo -e "${GREEN}PASS${NC}"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}FAIL${NC}"
    echo "Help output doesn't contain expected content"
    echo "Got: $HELP_OUTPUT"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi
echo ""

# Print summary
echo "=== Test Summary ==="
echo "Total tests: $TEST_COUNT"
echo -e "${GREEN}Passed: $PASS_COUNT${NC}"
if [ $FAIL_COUNT -gt 0 ]; then
  echo -e "${RED}Failed: $FAIL_COUNT${NC}"
  exit 1
else
  echo "All tests passed!"
  exit 0
fi